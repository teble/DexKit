# Native Query 并发改造设计（草案）

> 状态：Draft  
> 适用范围：`DexKitBridge` / native `DexKit` 同一实例内的并发 query  
> 当前默认保护：JVM 层 `@Synchronized`

## 1. 问题定义

当前 DexKit 更适合的执行模型是：

- **单个 query 内部并行**
- **同一个 bridge 在外层串行调用**

而不适合的是：

- **同一个 `DexKitBridge` 被多个上层线程同时重入调用**

当前问题在现象上表现为：

1. 多个线程同时对同一个 bridge 发起 `findClass` / `findMethod` / `findField`
2. 每个 query 在 native 内部又各自创建一套线程池
3. 结果出现：
   - 线程数快速膨胀
   - CPU 抢占加剧
   - cache / 内存带宽竞争恶化
   - lazy cache 初始化窗口重叠
   - 极端情况下出现性能雪崩，甚至不稳定行为

因此，“允许 native 同时被多个线程调用”本身并不会自动提升性能。  
如果不先清理共享状态与执行模型，放开并发只会把问题放大。

## 2. 设计目标

### 2.1 核心目标

1. 支持多个线程同时调用同一个 `DexKitBridge`
2. 避免“外层并发 × 内层并发”导致的线程数失控
3. 保持单个 query 在**无竞争**时能吃满实例总 worker，延迟尽量接近当前实现
4. 提升多 query 场景下的总体吞吐和稳定性
5. 为 `DexKitCacheBridge`、桌面长生命周期实例、后续批量查询调度提供统一并发基础

### 2.2 非目标

1. 不追求每个 query 各自独占一整套线程池
2. 不追求多 query 场景下线性加速
3. 不改变 Kotlin / Java DSL 语义
4. 第一阶段不处理跨 `DexKitBridge` 实例的全局调度

## 3. 当前瓶颈

### 3.1 每个 query 自建线程池

当前 `findClass` / `findMethod` / `findField` / batch query 都是 **per-query ThreadPool**。

这意味着：

- 1 个 query = 1 套 native worker
- 4 个并发 query = 4 套 native worker

问题不在于“native 是否线程安全”本身，而在于 **线程资源被重复放大**。

### 3.2 lazy cache 与 cross-ref 是共享可变状态

native 中存在多类共享状态：

- 基础索引
- lazy cache
- cross-reference 结果
- matcher 预处理缓存

它们在“单 query + 内部 worker”模型下问题不明显；  
但在“多个 query 同时进入”的模型下，如果没有明确状态机和发布语义，就很难证明并发安全。

### 3.3 matcher 临时缓存依赖 `ThreadVariable`

matcher 预处理结果当前大量依赖：

- key = matcher 指针地址
- scope = worker thread 生命周期
- cleanup = ThreadPool 析构时清理

这在 per-query 线程池模型下还能工作；  
一旦未来改成实例级共享线程池，这类缓存生命周期就会被拉长，边界会变得不清晰。

### 3.4 `findFirst` / 早停协议不够严格

当前 `findFirst` 早停依赖裸 `bool` 协调，`ThreadPool` 的 `skip_unexec_tasks` 也依赖裸 `bool`。

这类实现方式在单线程或低竞争下可能“看起来可用”，但从并发语义上并不严谨。

### 3.5 bridge 生命周期与运行时配置仍依赖 `@Synchronized`

当前 `DexKitBridge` 的：

- `close()`
- `setThreadNum()`
- 各类 native 入口

都依赖 JVM 层 `@Synchronized` 维持互斥。  
这说明它不仅仅是“保守串行化性能策略”，还承担了生命周期保护职责。

## 4. 设计原则

### 4.1 并发接入 ≠ 并发放大

允许多个线程同时调用 bridge，不代表每个 query 都能无限制放大 native worker 数。

### 4.2 实例总并行度固定

同一个 native `DexKit` 实例的总 worker 数必须受统一调度限制，不能再由 query 数量直接决定。

### 4.3 单活跃 query 必须独占可用资源

当同一实例内只有 1 个活跃 query 时：

- 它必须可以占满实例全部 worker
- 不应被公平调度或默认配额额外限流
- shared-pool 模型应尽量退化为“该 query 独占资源”的执行效果

### 4.4 基础索引尽量只读共享

一旦完成构建，应尽量转化为只读共享数据，减少后续 query 的共享写路径。

### 4.5 query 状态显式化

每个 query 应拥有独立上下文，至少管理：

- `query_id`
- early-exit / internal stop 标记
- query-local matcher cache
- query-local result buffer
- metrics

### 4.6 先证明安全，再追求吞吐

Phase 1 先解决 correctness 与状态边界；  
Phase 2/3 再切执行模型；  
Phase 4/5 再做调度和性能优化。

## 5. 推荐架构

推荐目标架构：

- **每个 `DexKit` 实例只有一个共享 worker pool**
- **引入 `QueryScheduler` 统一调度实例内多个 query**
- **引入 `QueryContext` 管理每个 query 的短生命周期状态**

整体结构：

```text
Java/Kotlin caller threads
            |
            v
     DexKitBridge native entry
            |
            v
        QueryScheduler
        /            \
 shared worker pool   query queue
            |
            v
        QueryContext
        /    |     \
 internal-stop  early-exit  metrics
```

### 5.1 当前收敛策略：query admission + batch warm-up barrier

在 shared worker pool / `QueryScheduler` 最终落地前，当前更适合采用一个**过渡但可证明正确**的执行模型：

- query 进入 native 后先做 `analyze`
- `analyze` 得到本次 query 的 `need_flags`
- 若所需 cache / cross-ref 全部 ready，则作为**只读 query**直接并发执行
- 若存在 missing flags，则该 query 不立即开跑，而是把 `need_flags` 记入 pending warm-up 集合
- 一旦出现 pending warm-up，**暂停新的 query 准入**
- 等当前运行中的 query 全部结束后，统一执行一次 warm-up / `InitDexCache(union_flags)`
- warm-up 完成后，再放行这批等待 query 继续并发只读执行

它的核心思想是：

- **构建阶段独占**
- **查询阶段并发只读**

这样做的收益是：

1. 不必一开始就把所有 lazy cache / cross-ref 构建路径都改造成真并发 writer
2. `analyze` 已经能够提前给出 `need_flags`，天然适合做 admission barrier
3. 可以显著降低“不同 query 同时触发不同 cache 构建”时的状态竞争复杂度
4. 后续即使继续引入 shared pool，这个 barrier 仍然可以作为 query admission 层保留
5. 未来很多热路径中的 ready-bit 防御判断，都可以逐步上移到 admission phase

## 6. 关键改造点

### 6.1 从 per-query ThreadPool 改为 per-instance shared pool

这是最终并发方案的核心。

改造后：

- `DexKit` 初始化时创建共享 worker pool
- 所有 query 的切片任务都投递到同一池
- `threadNum` 表示实例总并行度，而不是单 query 并行度
- 若只有 1 个活跃 query，它必须可以吃满全部 worker
- scheduler / executor 需要提供 single-query fast path，避免在无竞争时引入不必要的调度损耗

### 6.2 引入 `QueryContext`

`QueryContext` 用于承载 query 的短生命周期状态。

建议至少包含：

- `query_id`
- `kind`
- `early_exit`
- `submitted_task_count`
- `completed_task_count`
- `metrics`

后续再逐步接入：

- query-local matcher cache
- query-local result buffer
- scheduler budget / priority

### 6.3 lazy cache 要有 feature 级状态机

当前 `dex_flag` / `dex_cross_flag` 只是位标记。  
未来需要升级为 feature 级状态机，例如：

- `uninitialized`
- `initializing`
- `ready`

避免多个 query 并发判断“尚未初始化”后重复进入同一构建路径。

### 6.4 matcher 临时缓存要分层

建议把 matcher 相关缓存分成三类：

1. **query-local**
   - 属于单次 query
   - 跟随 `QueryContext` 生命周期释放

2. **true worker-local**
   - 只适用于共享 worker 的 scratch buffer
   - 必须明确与 query 结果无关

3. **只读共享**
   - 一次构建
   - 多 query 复用

#### 6.4.1 matcher 热路径缓存的默认收敛方向

最新一轮单 query 回退诊断表明：**把 matcher 预处理缓存统一放到 `QueryContext` 的共享 map 中，并在热路径上通过互斥锁 `GetOrCreateCache(...)` 访问，会在“单 query + 多 worker”场景里引入明显锁竞争**。

这类竞争的特点是：

- 所有 worker 会高频访问同一批 matcher key
- 命中后返回的对象通常很小，真正的 factory 构建成本不高
- 因此互斥锁 / 哈希表访问成本会直接出现在热点路径中，并随着 worker 数增长而放大

因此，matcher 热路径缓存的默认策略应进一步明确为：

1. **边界仍然是 query-local**
   - 结果只对当前 query 可见
   - query 切换后应立即失效

2. **实现优先使用 per-thread per-query fast path**
   - 由当前 worker thread 持有 thread-local map
   - 以 `QueryContext::Current()` 作为 query 边界
   - 当绑定的 `QueryContext` 变化时清空 thread-local matcher cache

3. **`QueryContext` 负责“定义边界”，不负责“承载每次热路径查表的共享锁”**
   - `QueryContext` 继续管理 early-exit / metrics / scheduler / snapshot 等 query 状态
   - 但不应默认成为 matcher 热缓存的全局互斥注册表

4. **只有确实值得共享的对象才升级为 query-shared / read-only shared**
   - 需要具备明显构建成本
   - 需要能够一次发布后长期只读复用
   - 并且要证明共享后的锁成本低于重复构建/重复命中成本

这条原则的目标不是回退到“线程生命周期缓存”，而是把缓存边界收敛为：**生命周期属于 query，快路径属于当前 worker**。

### 6.5 `findFirst` / early-exit 统一原子化

所有早停语义都应统一接入 `QueryContext`：

- `early_exit`：`findFirst` 命中后的早停信号

如后续确有调试或压测需求，再评估是否保留**仅内部使用**的 stop hook，而不是默认暴露公共 cancel API。

避免继续用裸 `bool` 做跨线程协调。

## 7. 与当前 `@Synchronized` 的关系

### 7.1 单个重查询

对于单个重查询，`@Synchronized` 仍然可能是非常接近最优的：

- 没有额外调度开销
- 所有 native worker 都服务同一个 query

因此，native 并发 query 方案若要可接受，至少必须满足以下硬约束：

- **当实例内只有 1 个活跃 query 时，该 query 必须能够占满实例全部 worker**
- **无竞争场景下，shared-pool 执行效果应尽量退化为当前 `@Synchronized` 基线**
- **公平调度、query 配额等机制只应在多 query 同时活跃时发挥作用**

### 7.2 多个并发查询

真正并发方案的目标不是“让每个 query 都更快”，而是：

1. 单 query 延迟尽量不明显退化
2. 多 query 总吞吐优于 `@Synchronized`
3. 最坏情况明显优于“每个 query 自建线程池”

因此，后续并发方案要优于 `@Synchronized`，必须满足：

- correctness 不退化
- 单 query 退化可接受
- 多 query 吞吐和稳定性有实质收益

## 8. API / 灰度建议

在 native 并发能力真正成熟之前，不建议立即改为默认行为。

建议提供实验开关，例如：

```kotlin
bridge.setSchedulerMode(SchedulerMode.SharedPool)
bridge.setMaxConcurrentQueries(4)
bridge.setThreadNum(8)
```

建议语义：

- `threadNum`：实例总 worker 数
- `maxConcurrentQueries`：同一实例内同时活跃 query 上限
  - 当达到上限时，新的 shared-pool query 应按 admission queue / FIFO 顺序进入 active set，而不是被 `notify_all()` 后无序竞争
- `SchedulerMode`：串行保护 / 共享调度模式

## 9. 推进计划（执行版）

### Phase 0：基线与观测

**目标**

- 固化当前问题的 benchmark 基线
- 让后续每次改造都能对比同一套数据

**工作项**

1. 提供统一 benchmark 入口
2. 固化 demo / 大 APK 两套场景
3. 固化统一输出指标
4. 固化系统属性 / 环境变量配置方式

**出口条件**

- 能稳定复现 slowdown
- 后续阶段可以反复复用同一套命令

### Phase 1：并发安全清理

**目标**

- 先证明“同一实例被多 query 重入”在技术上可以做对

**工作项**

1. 盘点共享可变状态
2. 引入 `QueryContext`
3. 把 `findFirst` / early-exit 路径原子化
4. 迁移 matcher 临时缓存
5. 为 cache 初始化建立清晰状态机

**出口条件**

- 压测下无明显 correctness 问题
- 结果与串行基线一致

### Phase 2：执行模型抽象

**目标**

- 在不改变默认行为的前提下，为共享调度做结构准备

**工作项**

1. 抽象统一执行接口，例如 `IQueryExecutor`
2. 保留双实现：
   - `LegacyPerQueryExecutor`
   - `SharedPoolExecutor`
3. 把扫描切片抽象成可调度任务

**出口条件**

- 旧模型不回退
- 新模型可通过开关切换

### Phase 3：共享线程池 MVP

**目标**

- 优先解决 per-query 线程池导致的线程数爆炸

**工作项**

1. 每个 `DexKit` 实例持有一个共享 worker pool
2. 所有 query 都投递到该池
3. 先采用简单 FIFO / 限额调度
4. 保持实验开关，不直接默认启用

**出口条件**

- 峰值线程数不再随 query 数量线性膨胀
- 多 query 不再出现灾难性性能退化
- 单 query 退化保持在可接受范围内

### Phase 4：`QueryScheduler` MVP

**目标**

- 让多个 query 真正共享执行资源，而不是互相争抢

**工作项**

1. 引入 `QueryScheduler`
2. 支持入队、配额、公平调度
   - 至少先把当前调度语义收敛为“所有可见 query 共享 base phase，latency-sensitive query 仅在 bonus phase 获得额外份额”
3. 支持 `findFirst` 与内部早停协作
4. 输出调度指标

**出口条件**

- 多 query 吞吐优于 `@Synchronized`
- 小 query 不会被大 query 饿死
- 长时间压测稳定

### Phase 5：性能调优

**目标**

- 从“能跑”优化到“值得开启”
- 当前收敛策略：**调度层先按 Phase 4 的现状收口**，后续优先处理非调度热路径，而不是继续扩大 scheduler 功能与指标范围

**工作项**

1. 调整切片粒度
2. 继续优化 `findFirst` / early-exit 内部协议
3. 继续上移 ready-check，收敛内层防御式判断
4. 降低 cache 初始化的大锁影响
5. 审视 `QueryContext` / matcher 热路径中的锁与原子竞争

**建议验收指标**

- 无竞争单 query 回退 < 5%，理想情况下接近噪声级
- 单 query 在同一 APK 上随 worker 增长不应出现系统性反向退化
- 4 并发场景总吞吐有明确提升
- 8 并发场景最坏耗时显著优于当前 unsynchronized 思路
- 压测无 crash、无结果不一致

### Phase 6：灰度与默认化

**目标**

- 不直接替换当前串行保护，而是通过开关逐步灰度

**工作项**

1. 增加调度模式开关
2. 默认仍保持保守模式
3. 在不同 APK / query 规模下回归
4. 数据稳定后再讨论默认启用

**出口条件**

- 多平台回归通过
- benchmark 数据稳定
- 无新增 correctness 问题

## 10. 计划与进度分离说明

为避免“稳定设计/Phase 计划”与“阶段性实现进度”混写造成的混乱，本文从现在开始只保留：

- 问题定义
- 目标架构
- Phase 计划
- 验收标准
- 风险与结论

已落地进度、当前阶段判断、当前限制与下一步推进建议，统一移入：

- `doc/design/native-query-concurrency-progress.md`

当前阶段请始终以进度文档为准，不在本文重复维护。

## 11. 验收维度

每个阶段至少从四个维度评估：

1. **正确性**
   - 结果与串行基线一致

2. **稳定性**
   - 无 crash、无死锁、无悬挂

3. **性能**
   - 单 query 不明显退化
   - 多 query 总吞吐有明确收益

4. **可回滚**
   - 任意阶段都能切回当前 `@Synchronized` 串行模式

## 12. 风险

1. 实现复杂度明显高于 `@Synchronized`
2. 单 query 延迟可能略有回退
3. lazy cache / cross-ref 的改造容易引入隐蔽 correctness 问题
4. 如果 query-local / shared-state 边界划分不清，会让后续 shared pool 更难收敛

## 13. 结论

如果目标只是：

- **稳定解决同一 bridge 的并发 query 问题**

那么当前 `@Synchronized` 已经是有效且保守的方案。

如果目标升级为：

- **让同一 bridge 在多线程环境下获得更好的总体吞吐**

那么正确路线不是“直接放开并发”，而是：

1. 先清理共享可变状态
2. 再引入 `QueryContext`
3. 再把 per-query 线程池改为实例级共享线程池
4. 最后通过 `QueryScheduler` 统一调度

只有这样，native 并发 query 才有机会真正优于当前 `@Synchronized` 基线。
