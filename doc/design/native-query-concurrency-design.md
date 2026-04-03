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

### 3.4 `findFirst` / cancel 协议不够严格

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
- cancel / early-exit 标记
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
 cancel  early-exit metrics
```

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
- `cancelled`
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

### 6.5 `findFirst` / cancel 统一原子化

所有早停 / 取消语义都应统一接入 `QueryContext`：

- `early_exit`：`findFirst` 命中后的早停信号
- `cancelled`：外部取消信号

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
3. 把 `findFirst` / cancel 路径原子化
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
3. 支持 cancel 与 `findFirst`
4. 输出调度指标

**出口条件**

- 多 query 吞吐优于 `@Synchronized`
- 小 query 不会被大 query 饿死
- 长时间压测稳定

### Phase 5：性能调优

**目标**

- 从“能跑”优化到“值得开启”

**工作项**

1. 调整切片粒度
2. 调整 query 配额策略
3. 评估小 query 优先 / `findFirst` 优先
4. 降低 cache 初始化的大锁影响

**建议验收指标**

- 无竞争单 query 回退 < 5%，理想情况下接近噪声级
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

## 10. Phase 0 当前落地

当前已经落地：

- 新增 `:main:runSharedBridgeBenchmark`
- 不修改用户的 `Main.kt`
- 支持两种 bridge 模式：
  - `SHARED`：多个线程共享一个 `DexKitBridge`
  - `ISOLATED`：每个 worker 独占一个 `DexKitBridge`

### 10.1 推荐使用方式

#### 默认 demo 基线

```powershell
./gradlew.bat --no-daemon :main:runSharedBridgeBenchmark
```

#### 大 APK 共享 bridge 基线

```powershell
$env:DEXKIT_BENCH_PRESET='LARGE_APK'
$env:DEXKIT_BENCH_APK='D:\Project\Android\WeWa\repack\origin\origin_random_2.26.1.76.apk'
$env:DEXKIT_BENCH_BRIDGE_MODE='SHARED'
$env:DEXKIT_BENCH_NATIVE_THREADS='8'
$env:DEXKIT_BENCH_WORKERS='4'
$env:DEXKIT_BENCH_ITERATIONS='2'
$env:DEXKIT_BENCH_WARMUP='1'
$env:DEXKIT_BENCH_EXPECT_RESULT_SIZE='1'
./gradlew.bat --no-daemon :main:runSharedBridgeBenchmark
```

#### 大 APK 多实例对照

```powershell
$env:DEXKIT_BENCH_PRESET='LARGE_APK'
$env:DEXKIT_BENCH_APK='D:\Project\Android\WeWa\repack\origin\origin_random_2.26.1.76.apk'
$env:DEXKIT_BENCH_BRIDGE_MODE='ISOLATED'
$env:DEXKIT_BENCH_NATIVE_THREADS='8'
$env:DEXKIT_BENCH_WORKERS='4'
$env:DEXKIT_BENCH_ITERATIONS='2'
$env:DEXKIT_BENCH_WARMUP='1'
$env:DEXKIT_BENCH_EXPECT_RESULT_SIZE='1'
./gradlew.bat --no-daemon :main:runSharedBridgeBenchmark
```

### 10.2 输出字段

- `bridgeMode` / `bridgeCount`
- `elapsedMs`
- `p50Ms` / `p95Ms` / `p99Ms`
- `peakJvmThreads`
- `processCpuMs` / `cpuRatio`

> `peakJvmThreads` 是 JVM 视角线程数，不等价于 native 总线程数。

### 10.3 当前基线结论

目前观测到：

- `SHARED + @Synchronized` 在当前实现下仍然是稳定基线
- `ISOLATED` 多实例并行在大 APK 场景下并没有带来收益，反而明显更慢

这进一步说明：  
后续 native 并发改造的关键不是“放开并发”，而是“统一调度、避免资源放大”。

## 11. Phase 1 共享可变状态盘点（第一轮）

### 11.1 结论概览

- 当前 `@Synchronized` 不只是性能策略，它还在保护 bridge 生命周期和 native 共享状态
- 若直接移除 JVM 层串行保护，会同时暴露：
  - bridge `close/query` 竞态
  - per-query 线程池放大
  - `findFirst` 竞态
  - matcher 临时缓存生命周期不清晰

### 11.2 盘点范围

本轮已经检查：

- `DexKitBridge.kt`
- `native-bridge.cpp`
- `dexkit.cpp`
- `dexkit.h`
- `dex_item.cpp`
- `dex_item.h`
- `dex_item_find.cpp`
- `dex_item_matcher.cpp`
- `ThreadPool.h`
- `ThreadVariable.h`

### 11.3 JVM bridge 层共享状态

#### A. `token` 与 bridge 生命周期

当前 `DexKitBridge.token` 的访问依赖 `@Synchronized`。

风险：

- `close()` 与 query 并发时，native 指针可能被使用中释放
- `setThreadNum()` 与 query 并发时，运行时配置会发生重叠修改

建议：

- 定义显式 lifecycle state
- query 拿共享访问，`close()` 拿独占访问
- `threadNum` 后续收敛为调度配置，而不是任意时间可变实例状态

### 11.4 DexKit 实例级共享状态

#### A. `_thread_num`

它当前是实例级可变配置，并被各处 `ThreadPool pool(_thread_num)` 直接读取。

建议：

- 先收紧为“初始化后冻结”
- 最终交由 scheduler 统一解释为实例总并行度

#### B. `class_declare_dex_map`

当前写入有锁、读取无锁。  
它成立的前提是：实例 create 完成后不再动态增量扩展 dex。

建议：

- 把这个“事实约束”提升为显式实例状态机

#### C. `dex_items` / `images` / `dex_cnt`

这些结构本质上也依赖“构造完成后只读”的约束。

### 11.5 DexItem 的 lazy cache / cross-ref

#### A. `dex_flag` / `dex_cross_flag`

它们当前只是位标记，不是 feature 级状态机。

风险：

- 多 query 同时判断“未初始化”时，可能重复进入同一构建路径

#### B. `InitCache()`

它会原地填充多组共享向量，例如：

- `method_opcode_seq`
- `method_using_string_ids`
- `method_using_field_ids`
- `method_invoking_ids`
- `method_using_numbers`
- 注解相关缓存

风险：

- 如果缺少清晰的初始化发布语义，这些都属于共享写路径

#### C. `PutCrossRef()`

它不仅会补缓存，还会：

- `swap()` 迁移 `class_method_ids` / `class_field_ids`
- merge caller / field rw 结果
- 回填 cross-info

风险：

- 这已经是“修改对象图归属关系”，不能只被视作普通缓存填充

### 11.6 Query 期间的共享临时状态

#### A. `findFirst` 早停标记

原先实现使用裸 `bool` 在多个 slice task 之间共享，存在数据竞争。

#### B. `ThreadPool::_skip_unexec_tasks`

原先也是裸 `bool`，同样不适合作为正式并发协议。

#### C. per-query ThreadPool

它本身就是当前外层并发时资源放大的直接来源之一。

### 11.7 matcher 临时缓存与 `ThreadVariable`

当前大量 matcher 预处理结果存放在 `ThreadVariable` 中。

风险：

1. 这些数据更像 query 派生状态，而不是线程级长期状态
2. 一旦未来切到 shared worker pool，生命周期会明显拉长
3. `matcher 地址 + worker 线程` 不是长期可维护的缓存边界

建议：

- query 期间的预处理结果优先迁入 `QueryContext`
- 只有真正 worker scratch 的部分，才考虑保留为 thread-local

### 11.8 相对适合继续只读共享的部分

更接近 immutable shared state 的主要是基础索引，例如：

- `strings`
- `type_names`
- `type_ids_map`
- `type_def_idx`
- `type_def_flag`
- `class_access_flags`
- `method_access_flags`
- `field_access_flags`
- `proto_type_list`
- `method_codes`

### 11.9 阻塞项分级

#### P0：移除 `@Synchronized` 前必须完成

1. 用 `QueryContext` + 原子标记替换裸 `bool` 的 `findFirst`
2. 清理 `skip_unexec_tasks` 的并发语义
3. 明确 bridge 生命周期协议
4. 明确实例 create 后拓扑冻结
5. 将 matcher 临时缓存默认迁向 query 生命周期

#### P1：进入 shared pool MVP 前建议完成

1. `InitDexCache()` 从全局静态锁改为实例级 coordinator
2. `dex_flag` / `dex_cross_flag` 升级为 feature 状态机
3. cross-ref 结果改为显式发布

#### P2：可以后置，但需要记录边界

1. Android classloader 初始化路径里的全局 JNI 状态
2. `ThreadVariable` 底层设施的长期并发契约

### 11.10 当前已落地的 Phase 1 第一步

本轮已经落地：

1. 新增 native `QueryContext` 骨架
   - 包含 `query_id`
   - `cancelled`
   - `early_exit`
   - 简单 task metrics

2. `findClass` / `findMethod` / `findField` 已接入 `QueryContext`

3. `findFirst` 早停不再使用裸 `bool`
   - 已改为 `QueryContext` 中的原子 `early_exit`

4. find 路径的待执行任务跳过协议已绑定到 `QueryContext`
   - `ThreadPool` 新增 `should_skip_task` 谓词入口
   - `findClass` / `findMethod` / `findField` 已通过 `query_context.ShouldStop()` 驱动队列中未执行任务的快速跳过
   - find 路径不再依赖单独的 `skip_unexec_tasks` 状态

5. `QueryContext` 当前已开始记录最小 metrics
   - `submitted_tasks`
   - `completed_tasks`

6. batch query 已接入 `QueryContext`
   - `BatchFindClassUsingStrings`
   - `BatchFindMethodUsingStrings`
   - 当前已统一到相同的任务提交 / 完成计数与 `should_skip_task` 协议

7. matcher 临时缓存已开始迁入 query 生命周期
   - `QueryContext` 新增 query-local cache 容器与线程绑定能力
   - `dex_item_find.cpp` / `dex_item_batch_find.cpp` 在任务执行期间会绑定当前 `QueryContext`
   - `dex_item_matcher.cpp` 中首批热点缓存已优先走 `QueryContext`
     - using-strings 关键词 trie / map / keyword-set
     - 注解 / 接口 / 字段 / 方法 matcher 向量展开结果
     - type-name matcher 规格化结果
     - opcode / using-fields / using-numbers 预处理结果
   - 当前仍保留“无 `QueryContext` 时回退到 `ThreadVariable`”的兼容路径

8. `InitDexCache()` 已开始从“全局静态锁 + 裸位标记”收敛为实例内协调
   - 去掉了 `DexKit::InitDexCache()` 的静态全局互斥
   - `DexItem` 为 `InitCache` / `PutCrossRef` 新增了显式的 begin / finish / wait 协调点
   - `dex_flag` / `dex_cross_flag` 已改为原子 ready-bit
   - 当前策略是：**同一 `DexItem` 上同一时刻只允许一个 cache/cross-ref 构建者，其他调用方等待 ready**
   - 这一步先解决“重复初始化 / 重复 cross-ref 构建”的状态边界问题，尚未引入更细粒度的 feature 并发执行

当前限制：

- 外部 cancel 还没有正式暴露到 API
- metrics 还没有对外输出
- matcher 迁移还处于第一批，`ThreadVariable` 兼容路径尚未删除
- `PutCrossRef()` 对 `class_method_ids` / `class_field_ids` 的发布仍是共享写路径，后续仍需要更明确的读写边界与发布语义

### 11.11 下一步建议实现顺序

1. 继续把 cancel / early-exit 协议统一到 `QueryContext`
2. 逐步迁移 matcher 临时缓存
3. 继续细化 cache / cross-ref 的 feature 状态机与发布语义
4. 然后再进入 `IQueryExecutor` / `SharedPoolExecutor` 抽象

## 12. 验收维度

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

## 13. 风险

1. 实现复杂度明显高于 `@Synchronized`
2. 单 query 延迟可能略有回退
3. lazy cache / cross-ref 的改造容易引入隐蔽 correctness 问题
4. 如果 query-local / shared-state 边界划分不清，会让后续 shared pool 更难收敛

## 14. 结论

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
