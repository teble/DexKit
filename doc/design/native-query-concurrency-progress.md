# Native Query 并发改造进度

> 当前阶段：Phase 4（MVP 推进中）  
> 对应计划：`doc/design/native-query-concurrency-design.md`  
> 相关专项记录：`doc/design/using-numbers-storage-notes.md`

## 1. 当前阶段判断

当前状态更准确地说是：

- **Phase 1：核心正确性与状态边界收敛，已基本完成**
- **Phase 2：执行模型抽象，已基本完成**
- **Phase 3：共享线程池 MVP，已基本落地**
- **Phase 4：`QueryScheduler` MVP，已开始推进**

当前已经进入 Phase 4 起步阶段的依据：

- 已新增统一执行接口：`IQueryExecutor`
- 已有当前实现：`ThreadPoolQueryExecutor`
- 已补上共享实现骨架：`SharedThreadPoolQueryExecutor`
- `FindClass` / `FindMethod` / `FindField` / `BatchFind*` 的任务提交已不再直接耦合 `ThreadPool`
- `DexKit::CreateQueryExecutor(query_context)` 已统一接管 query 执行器创建
- 已提供 native/JVM 实验切换入口：
  - native：`DexKit::SetQueryExecutorMode(...)`
  - JVM：`DexKitBridge.setSchedulerMode(SchedulerMode)`
- 已补充 shared-pool 模式下的基础 admission cap：
  - native：`DexKit::SetMaxConcurrentQueries(...)`
  - JVM：`DexKitBridge.setMaxConcurrentQueries(Int)`
- benchmark 已支持 `DEXKIT_BENCH_SCHEDULER_MODE` / `DEXKIT_BENCH_MAX_CONCURRENT_QUERIES`
- shared-pool 已不再只是“公共线程池直连”：
  - 已开始通过最小版 `QueryScheduler` 做 query 级排队与轮转分发
  - 单 query 仍可占满全部 worker，多 query 时会施加动态 in-flight 限额

但 Phase 4 还远未完成，因为以下内容还没落地：

- shared-pool 下更完整的公平性、限额与 benchmark 观测接线
- 更明确的 budget / priority / starvation guard 语义

## 2. Phase 0：基线与观测

当前已经落地：

- 新增 `:main:runSharedBridgeBenchmark`
- 不修改用户的 `Main.kt`
- 支持两种 bridge 模式：
  - `SHARED`：多个线程共享一个 `DexKitBridge`
  - `ISOLATED`：每个 worker 独占一个 `DexKitBridge`

### 2.1 推荐使用方式

#### 默认 demo 基线

```powershell
./gradlew.bat --no-daemon :main:runSharedBridgeBenchmark
```

#### 大 APK 共享 bridge 基线

```powershell
$env:DEXKIT_BENCH_PRESET='LARGE_APK'
$env:DEXKIT_BENCH_APK='D:\Project\Android\WeWa\repack\origin\origin_random_2.26.1.76.apk'
$env:DEXKIT_BENCH_BRIDGE_MODE='SHARED'
$env:DEXKIT_BENCH_SCHEDULER_MODE='SharedPool'
$env:DEXKIT_BENCH_MAX_CONCURRENT_QUERIES='2'
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
$env:DEXKIT_BENCH_SCHEDULER_MODE='SharedPool'
$env:DEXKIT_BENCH_MAX_CONCURRENT_QUERIES='2'
$env:DEXKIT_BENCH_NATIVE_THREADS='8'
$env:DEXKIT_BENCH_WORKERS='4'
$env:DEXKIT_BENCH_ITERATIONS='2'
$env:DEXKIT_BENCH_WARMUP='1'
$env:DEXKIT_BENCH_EXPECT_RESULT_SIZE='1'
./gradlew.bat --no-daemon :main:runSharedBridgeBenchmark
```

### 2.2 输出字段

- `bridgeMode` / `bridgeCount`
- `schedulerMode`
- `maxConcurrentQueries`
- `elapsedMs`
- `p50Ms` / `p95Ms` / `p99Ms`
- `peakJvmThreads`
- `processCpuMs` / `cpuRatio`
- 若启用 `SharedPool`，还会额外输出 scheduler 级指标：
  - `schedulerDispatchedTasks` / `schedulerBaseDispatchedTasks` / `schedulerBonusDispatchedTasks`
  - `schedulerShareCountSyncs` / `schedulerShareCountChanges` / `schedulerBudgetRebalances`
  - `schedulerRefillRounds` / `schedulerRunnableQueueRebuilds`
  - `schedulerMaxTotalInFlight` / `schedulerMaxVisibleQueryShareCount` / `schedulerMaxRunnableQueueSize`
- 当前 benchmark 也会输出 query-local 聚合指标：
  - `querySubmittedTasksTotal` / `queryDispatchedTasksTotal` / `queryCompletedTasksTotal`
  - `querySubmittedTasksAvg` / `queryDispatchedTasksAvg` / `queryCompletedTasksAvg`
  - `queryFirstDispatchDelayUsP50` / `queryFirstDispatchDelayUsP95`
  - `queryMaxInFlightP50` / `queryMaxInFlightP95` / `queryMaxInFlightMax`
  - `queryMaxQueryShareCountP50` / `queryMaxQueryShareCountP95` / `queryMaxQueryShareCountMax`

> `peakJvmThreads` 是 JVM 视角线程数，不等价于 native 总线程数。

### 2.3 当前基线结论

目前观测到：

- `SHARED + @Synchronized` 在当前实现下仍然是稳定基线
- `ISOLATED` 多实例并行在大 APK 场景下并没有带来收益，反而明显更慢

这进一步说明：  
后续 native 并发改造的关键不是“放开并发”，而是“统一调度、避免资源放大”。

## 3. Phase 1：共享可变状态盘点（第一轮）

### 3.1 结论概览

- 当前 `@Synchronized` 不只是性能策略，它还在保护 bridge 生命周期和 native 共享状态
- 若直接移除 JVM 层串行保护，会同时暴露：
  - bridge `close/query` 竞态
  - per-query 线程池放大
  - `findFirst` 竞态
  - matcher 临时缓存生命周期不清晰

### 3.2 盘点范围

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

### 3.3 JVM bridge 层共享状态

#### A. `token` 与 bridge 生命周期

当前 `DexKitBridge.token` 的访问依赖 `@Synchronized`。

风险：

- `close()` 与 query 并发时，native 指针可能被使用中释放
- `setThreadNum()` 与 query 并发时，运行时配置会发生重叠修改

建议：

- 定义显式 lifecycle state
- query 拿共享访问，`close()` 拿独占访问
- `threadNum` 后续收敛为调度配置，而不是任意时间可变实例状态

### 3.4 DexKit 实例级共享状态

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

### 3.5 DexItem 的 lazy cache / cross-ref

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

### 3.6 Query 期间的共享临时状态

#### A. `findFirst` 早停标记

原先实现使用裸 `bool` 在多个 slice task 之间共享，存在数据竞争。

#### B. `ThreadPool::_skip_unexec_tasks`

原先也是裸 `bool`，同样不适合作为正式并发协议。

#### C. per-query ThreadPool

它本身就是当前外层并发时资源放大的直接来源之一。

### 3.7 matcher 临时缓存与 `ThreadVariable`

当前大量 matcher 预处理结果存放在 `ThreadVariable` 中。

风险：

1. 这些数据更像 query 派生状态，而不是线程级长期状态
2. 一旦未来切到 shared worker pool，生命周期会明显拉长
3. `matcher 地址 + worker 线程` 不是长期可维护的缓存边界

建议：

- query 期间的预处理结果优先迁入 `QueryContext`
- 只有真正 worker scratch 的部分，才考虑保留为 thread-local

### 3.8 相对适合继续只读共享的部分

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

### 3.9 阻塞项分级

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

## 4. 当前已落地进展

本轮累计已经落地：

### 4.1 QueryContext / 早停 / query-local cache

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
   - `dex_item_matcher.cpp` 中的 matcher 预处理缓存现已统一要求存在 `QueryContext`，不再回退到 `ThreadVariable`
   - 这意味着 matcher 临时缓存的生命周期边界已进一步收敛到 query 级别，而不再依赖 worker thread 级残留状态
   - 但后续单 query 回退诊断也表明：**“query-local”不等于“必须走 `QueryContext` 共享 map”**
   - 在“单 query + 多 worker”场景下，若热点 matcher cache 统一经由 `QueryContext` 共享 map + mutex 访问，会形成明显锁竞争
   - 因而 matcher cache 的正式收敛方向已进一步调整为：`QueryContext` 负责提供 query 边界与线程绑定，实际热点 cache 优先采用 **per-thread per-query** fast path

### 4.2 Phase 2 已开始：执行模型抽象

8. 查询任务提交层已开始抽象出执行器接口
   - 新增 `IQueryExecutor` / `ThreadPoolQueryExecutor`
   - `FindClass` / `FindMethod` / `FindField` / `BatchFind*` 的任务提交已不再直接耦合 `ThreadPool`
   - `DexKit::CreateQueryExecutor(query_context)` 现在统一负责创建当前 query 的执行器
   - 这一步的目标不是立刻切 shared pool，而是先把“查询任务如何提交”与“底层是否自建线程池”解耦
   - 同时顺手把 `_thread_num` 收敛为原子读写，避免执行模型切换阶段再引入新的运行时配置竞争

9. Phase 2 的“双实现 + 切换入口”已经具备最小可运行骨架
    - native 侧已新增 `QueryExecutorMode`
    - 默认仍然是 `LegacyPerQuery`
    - `SharedPool` 模式下，`DexKit` 实例会惰性创建并复用实例级共享 `ThreadPool`
    - `setThreadNum()` 现在会重置共享池引用，后续 query 再按新的线程数懒创建
    - JVM 侧已新增实验 API：`DexKitBridge.setSchedulerMode(SchedulerMode)`
    - shared-pool 模式下已新增基础 admission cap：`DexKitBridge.setMaxConcurrentQueries(...)`
    - benchmark 已可通过 `DEXKIT_BENCH_SCHEDULER_MODE='SharedPool'` 与 `DEXKIT_BENCH_MAX_CONCURRENT_QUERIES` 直接压测 shared-pool 骨架
    - shared-pool 已开始从“共享 worker 池”收敛到“最小版 QueryScheduler 骨架”
      - 每个 query 拥有自己的 pending task 队列
      - 调度器按 query 轮转分发 task，而不是让单个 query 直接把公共 `ThreadPool` 队列打满
      - 同时根据当前活跃 query 数动态限制单个 query 的 in-flight task 数
      - 当实例内只有 1 个活跃 query 时，它仍可占满全部 worker
    - 当前还没有更高层的 query budget / priority / 饥饿保护，只是先把最基本的 task flooding 问题收敛掉
    - 本轮又补上了一个更明确的 **submission-complete activation** 阶段：
      - `IQueryExecutor` 新增 `OnSubmissionComplete()`
      - `SharedThreadPoolQueryExecutor` 会先把 task 只放入 query 私有 pending 队列
      - `FindClass` / `FindMethod` / `FindField` / `BatchFind*` 在完成本轮 task 提交后，才显式激活该 query
      - `QueryScheduler` 只对已激活 query 计算 budget / in-flight 配额并开始分发
      - 这样 shared-pool 的调度边界从“submit 时顺手 dispatch”进一步收敛为“先完成一轮提交，再进入统一调度”
    - 在 activation 基础上，本轮继续补了一个更明确的 **share-count fairness** 收敛：
      - scheduler 现在会把“已开始 submission、但尚未 activation 的 query”也纳入 share-count 估算
      - 因而第一个已激活 query 在 dispatch 首轮 budget / in-flight 限额时，会为这些“正在提交中的 query”预留份额
      - 这一步不能把先发优势完全消除，但可以把问题从“先完成提交就直接占满全部 worker”收敛到“最多只在更早的 pre-submission 窗口内存在优势”
    - 在 share-count fairness 之上，本轮又补了 **budget rebalance on share-count change**：
      - 一旦可见 query 份额发生变化，scheduler 会收紧已激活 query 手里尚未消耗的 `dispatch_budget`
      - 这样可以避免“某个 query 在单 query 阶段拿到的大 budget，在第二个 query 已经可见后仍继续沿用”
    - 本轮继续把调度语义从“只有总 budget”收敛到更明确的 **two-phase round fairness**：
      - 每轮先给所有可见 query 发放 base budget
      - `findFirst` / latency-sensitive query 的额外倾斜，不再直接混在 base 阶段里，而是进入单独的 bonus phase
      - 因而普通 query 至少会先拿到本轮 base 份额，然后 latency-sensitive query 才会消费自己的额外 bonus 份额
    - 同时已补上第一版 **native 内部 metrics 骨架**（暂不暴露公开 API）：
      - `QueryContext` 现已开始记录：`dispatched_tasks` / `base_dispatched_tasks` / `bonus_dispatched_tasks`
      - 以及 `first_dispatch_delay_ns` / `max_in_flight` / `max_query_share_count`
      - `QueryScheduler` 现已开始记录：share-count sync/change、budget rebalance、queue rebuild、refill 次数、dispatch/base/bonus 次数、`max_total_in_flight`
      - 这些指标当前主要用于后续 benchmark / 调度回归前的内部观测准备
    - 在内部 metrics 骨架基础上，本轮进一步补上了 **scheduler snapshot 调试出口**
      - native：`DexKit::GetQuerySchedulerMetricsSnapshot()`
      - JVM：`DexKitBridge.getSchedulerMetricsSnapshot()`
      - 当前先只暴露 scheduler 级累计指标，便于单测/基准快速判断 dispatch、share-count、budget rebalance 是否符合预期
      - query-local metrics 仍保持 native 内部态，暂不直接对外暴露
    - 在 snapshot 出口基础上，本轮继续补上了 **benchmark 输出接线**
      - `DexKitBridge.resetSchedulerMetrics()` / native `ResetQuerySchedulerMetrics()` 已补齐，便于 warm-up 后清零累计指标
      - `runSharedBridgeBenchmark` 现在会在 warm-up 结束后重置 scheduler metrics，并在测量结束后输出 scheduler 级聚合指标
      - 对 `ISOLATED` 模式会按 bridge 聚合累计计数、按 bridge 取 `max*` 指标的最大值，方便和 `SHARED` 模式对照
    - 在 scheduler metrics 之外，本轮也补上了 **query-local metrics 观测出口**
      - native：`DexKit::GetLastQueryMetricsSnapshot()`，按调用线程保留“最近一次完成 query”的 `QueryContext` snapshot
      - JVM：`DexKitBridge.getLastQueryMetricsSnapshot()`
      - 这样 benchmark 可以在每次 query 返回后立即抓取该调用线程对应的 query-local metrics，而不会与其他并发调用互相覆盖
      - 当前 `runSharedBridgeBenchmark` 已开始汇总这些 snapshot，并输出 per-query 平均任务数、首个 dispatch 延迟分位值、单 query 观测到的 `max_in_flight` / `max_query_share_count` 分布
    - 在 last-snapshot 之外，本轮继续补上了 **per-instance query metrics history 导出**
      - native：`DexKit::GetQueryMetricsHistorySnapshot()` / `ResetQueryMetricsHistory()`
      - JVM：`DexKitBridge.getQueryMetricsHistorySnapshot()` / `resetQueryMetricsHistory()`
      - 采用 DexKit 实例级有界 history buffer（当前容量 256），记录 `QueryKind + QueryPriority + QueryMetricsSnapshot`
      - 当 history 超出容量时会累计 `droppedRecords`，便于 benchmark / 单测识别样本截断
      - 当前已补齐 shared-scheduler 单测，验证 regular query 与 `findFirst` 的 kind/priority 分类，以及 reset 行为
      - 本轮继续把这套 history/classification 接到了 `runSharedBridgeBenchmark`
      - benchmark 现在会在 warm-up 后重置 history，并输出按 `kind + priority` 分组的 record 数、任务均值、base/bonus dispatch 计数与延迟/并发峰值
    - 同时已补一条 shared-scheduler batch 并发回归：
      - `testConcurrentBatchFindMethodUsingStringsOnSharedScheduler()`

### 4.3 Init / cross-ref / admission barrier

10. `InitDexCache()` 已开始从“全局静态锁 + 裸位标记”收敛为实例内协调
   - 去掉了 `DexKit::InitDexCache()` 的静态全局互斥
   - `DexItem` 为 `InitCache` / `PutCrossRef` 新增了显式的 begin / finish / wait 协调点
   - `dex_flag` / `dex_cross_flag` 已改为原子 ready-bit
   - 当前策略是：**同一 `DexItem` 上同一时刻只允许一个 cache/cross-ref 构建者，其他调用方等待 ready**
   - 当前实现中，真正执行构建的 `InitCache()` / `PutCrossRef()` / `BuildCrossRefAggregates()` 已开始假设自己收到的是 `Begin*()` 协调后返回的 claimed flags，不再在函数内部重复做同样的 ready-check
   - 这一步先解决“重复初始化 / 重复 cross-ref 构建”的状态边界问题，尚未引入更细粒度的 feature 并发执行

11. `PutCrossRef()` 已完成“稳定基础索引”与“一次性 cross-ref 工作集”的拆分
   - `class_method_ids` / `class_field_ids` 现在只保留当前 dex 内定义类的稳定基础成员索引
   - init 阶段识别出的“类声明不在当前 dex 中”的 method / field，会落入 `pending_cross_ref_method_ids` / `pending_cross_ref_field_ids`
   - `PutCrossRef()` 只消费 `pending_*` 工作集，不再原地破坏基础列表
   - 这样既避免了与 using-strings / matcher 等只读查询共享基础列表时的写冲突，又保留了 cross-ref 完成后释放一次性工作集内存的能力
   - 在当前阶段，cross-info 的发布仍然由 `dex_cross_flag` 作为 ready-bit 完成边界
   - 随着 admission barrier 接入，查询热路径已开始从“读取前再看 ready-bit”收敛为“外层先保证 ready，再直接读取 cross-info”

12. cross-ref 反向结果容器已收敛为“本 dex 本地结果 + DexKit 聚合发布”
   - `PutCrossRef()` 不再直接向其他 `DexItem` 的 `method_caller_ids` / `field_get_method_ids` / `field_put_method_ids` 追加写入
   - `DexItem` 内这三类容器现在只保留“当前 dex 直接解析出的本地反向边”
   - `DexKit::InitDexCache()` 在全部 `DexItem::PutCrossRef()` ready 之后，新增统一的 aggregate phase
   - aggregate phase 会把外部 dex unresolved 成员的本地反向边，按 `method_cross_info` / `field_cross_info` 直接回灌到目标 `DexItem` 的最终只读索引
   - `DexKit` 现在只保留 aggregate ready-bit / 协调状态，不再长期持有运行时 `cross_dex_*` 查询索引
   - 这样运行期查询重新只读取 `DexItem` 内最终索引，不再为 callers / field get-put methods 动态拼装额外 vector
   - 这也意味着“跨 dex 结果发布”虽然仍由 `DexKit` 统一调度，但最终数据落点已经重新回到 `DexItem`
   - 当前 `BuildCrossRefAggregates()` 已开始按 **target DexItem 分区** 并行执行：每个 task 独占一个 target dex 的最终索引写入权
   - 因此 task 内对最终 `vector` 做 `reserve + insert` 是安全的，不会出现多个线程同时向同一个目标索引 `push_back / insert`
   - `PutCrossRef()` 现在还会顺手记录预解析的 pending aggregate work item：只把“真正命中 cross-info 且携带反向边载荷”的 source 成员加入 aggregate worklist，并直接带上 target dex / target idx
   - 因而 `BuildCrossRefAggregates()` 不再需要全量扫描所有 method / field，也不必再次回查 `cross_info`；前置阶段已经收敛为“扫描命中的 pending work item + 按 target 统计 reserve”

13. `DexKit` 查询入口已开始接入 admission + warm-up barrier
   - `Find*` / `BatchFind*` / 元数据读取入口现在会先走 DexKit 级执行准入
   - 若 `need_flags` 缺失，则先把 flags 记入 pending warm-up，再阻塞新 query 准入
   - 等当前活跃 query 数归零后，统一执行一次 warm-up
   - warm-up 完成后，等待中的 query 再进入并发只读执行
   - 这意味着：当前代码正在从“依赖内层细粒度防御”逐步收敛到“外层 phase barrier 保证”
   - 对于本身已经声明明确 `need_flags` 的元数据访问器（如 callers / invoke / field get-put / using-fields），内层 accessor 也已开始去掉 fallback，改为直接依赖外层 barrier 并通过断言暴露误用
   - 对于仍不想升级成 bridge 级全量 warm-up 的元数据读取路径，当前开始做更轻量的 **per-method 稀疏懒缓存**
     - `GetMethodOpCodes()`：未 ready `kOpSequence` 时，不再每次都重扫 code item，而是按 method 懒构建一次 opcode 序列
     - `GetUsingStrings()`：未 ready `kUsingString` 时，不再每次都重扫 code item，而是按 method 懒构建一次 string-id 列表
     - 这两条路径与 `using_numbers` 一样，builder 资格通过 CAS 抢占，miss 冲突时只在条带化 `mutex/cv` 上等待
     - 一旦未来同类能力升级成全量 ready-bit，accessor 会优先读取 full-cache，不受稀疏懒缓存影响
   - 但 `using_numbers` 仍保留为**例外路径**：它不进入 bridge 级全量 warm-up barrier，而继续作为 method matcher 的末位条件按需处理
   - 原因是 `using_numbers` 的全量预热成本和常驻内存都偏高，不适合因为单个 query 命中就把整个 dex 的 number cache 全部铺开
   - 当前实现已把它收敛成 **per-method 稀疏懒缓存**：slot 数组在 init 阶段一次性定长，真正的 number vector 只在命中某个 method 时才构建
    - 每个 method slot 用原子状态 `Empty -> Building -> Ready` 协调；builder 资格通过 CAS 抢占，因此不会有两个线程同时发布同一个 method 的 using-numbers 缓存
    - 等待路径采用条带化 `mutex/cv`，只在 miss 且发生竞争时参与阻塞；ready fast path 仍然只是原子读 + 只读访问
    - 这样既避免了重复 query 不断重解析 opcode/code item，又不会把 `using_numbers` 混入当前的“大范围共享索引 warm-up”体系
    - `findFirst` 路径也额外补了生命周期收敛：
      - 一旦某个 future 命中提前结束，剩余 future 仍会在返回前被 drain
      - 这样可以保证 `QueryContext` / executor / scheduler 绑定对象不会在后台 task 尚未退出时提前析构
      - 与此同时，lazy opcode / using-string / using-number 的 `Ready + notify` 发布顺序也已收敛到同一条带锁下，避免并发 miss 时出现丢唤醒

### 4.4 单 query 回退诊断 / matcher cache 收敛验证

14. 已补齐一组更细的 query phase metrics，用于区分“预处理 / 提交 / worker 执行”三个阶段
    - `first_task_start_delay_ns`
    - `last_task_finish_delay_ns`
    - `task_runtime_total_ns`
    - `task_runtime_max_ns`
    - `preprocess_completed_ns`
    - `submission_completed_ns`
    - `workers_completed_ns`
    - `completed_ns`
    - 这组指标当前已接到 native snapshot / JNI / JVM snapshot / 单测，用于后续对照“单 query 为什么会随着 worker 数升高而变慢”

15. 基于上述指标，本轮做了一个**最小化 A/B 验证**
    - 保持 `QueryContext`、scheduler、executor、早停协议与 metrics 原子逻辑不变
    - 只把 `dex_item_matcher.cpp` 中的 matcher 热路径缓存从：
      - `QueryContext::GetOrCreateCache(...)`（query-shared map + mutex）
      - 切换为 `thread_local` 的 **per-thread per-query** cache
    - 具体做法是：
      - key 仍然是 `QueryCacheKey(scope, matcher-address)`
      - fast path cache 挂在线程本地
      - 当 `QueryContext::Current()` 变化时自动清空该线程上的 matcher cache
    - 也就是说，这轮验证没有绕开新的并发框架，只是单独替换了 matcher cache 的共享方式

16. 当前结论已经比较明确：**大幅回退的主因不是 `QueryContext` metrics 原子本身，而是 matcher 热路径上的共享 cache 锁竞争**
    - 代表性本地结果（`BATCH_SIZE=1000`）：
      - `nativeThreads=2`，10 次串行 query：约 `286 ms`
      - `nativeThreads=2`，10 worker 并发各 1 次：约 `266 ms`
      - `nativeThreads=32`，10 次串行 query：约 `70 ms`
      - `nativeThreads=32`，10 worker 并发各 1 次：约 `60 ms`
    - 与此前“串行 480~950 ms、并行 170 ms 左右”的巨大差距相比，A/B 后串行与并行差距已显著收敛
    - 这说明先前出现的“单 query 线程越多越慢”，本质上更像是：
      - 多个 worker 在同一 query 内高频争抢同一批 matcher cache key
      - lock/mutex 成为热路径瓶颈
      - worker 数越多，竞争越重，因此单 query 反而越慢
    - 因而当前正式推荐的收敛方向是：
      - matcher cache 的**生命周期边界**仍然属于 query
      - 但 matcher cache 的**快路径所有权**应属于当前 worker/thread
      - `QueryContext` 更适合作为 query 边界、取消协议、metrics、scheduler 绑定点，而不是 matcher 热缓存的共享互斥注册表
    - 后续仍可继续观察 `ShouldStop()` 原子、metrics 原子、切片策略等次级成本，但它们不再是当前的首要矛盾

## 5. 当前限制

- 外部 cancel 还没有正式暴露到 API
- 当前 scheduler 级 snapshot、query-local last-snapshot、实例级 history buffer 与 benchmark classification 输出都已可观测；但仍缺少更通用的流式导出与 buffer 容量配置
- `SharedPool` 已经有最小版 `QueryScheduler` 骨架，但还不是完整的 scheduler 产品形态
- shared-pool 模式下虽然已经补了 submission-complete activation + share-count fairness + share-count change budget rebalance + two-phase round fairness，但还没有明确的 query budget、priority、公平性 SLA、饥饿保护等更高层策略；当前仍主要依赖基础轮转分发 + 动态 in-flight 限额 + `maxConcurrentQueries` 准入上限
- 当前实现已经能对“正在 submission、尚未 activation”的 query 预留一部分 share，但如果另一个 query 还没开始 submission，就仍可能存在更早阶段的先发优势
- 当前 bonus phase 只解决“latency-sensitive 额外份额不要压住普通 query 的 base 份额”，但还没有形成可观测、可调参的 priority SLA
- matcher 迁移已经完成 `dex_item_matcher.cpp` 主路径的 query-boundary 收敛，并已额外验证出 `QueryContext` 共享 map 锁竞争是当前单 query 回退主因；但这套 **per-thread per-query** fast path 还需要进一步正式化，并清理/降级不再适合放在热路径上的共享 cache 注册接口
- `BuildCrossRefAggregates()` 前置阶段已经从“全量 method / field 扫描”收敛到“pending worklist 扫描”，后续仍可继续评估更进一步的增量化/复用空间
- `method_cross_info` / `field_cross_info` 的第一批热路径 ready-check 已开始移除，但仍有部分非热路径/防御性判断待继续收敛
- 目前仍保留少量“单项元数据接口按需直读 dex code / annotation”的 fallback 路径；其中 opcode / using-string 已收敛为 per-method 稀疏懒缓存，annotation 访问器则仍保持纯按需直读，以避免单次元数据读取强制触发整类全量 warm-up
- `using_numbers` 已经转为稀疏懒缓存，但当前仍是 DexItem 级 capability 特例；后续若出现更多“构建昂贵但命中稀疏”的 method 特征，再考虑抽象成统一的 lazy feature slot 框架


## 6. 下一步建议实现顺序

1. 将 matcher 热路径的 **per-thread per-query** cache 方案正式化，清理 `QueryContext::GetOrCreateCache()` 在 matcher 路径上的残留依赖，并先补齐对应 benchmark 回归
2. 以 activation 阶段为基础，把当前“轮转分发 + 动态 in-flight 限额”继续扩展成更明确的 query budget / fairness 语义
3. 继续把 cancel / early-exit 协议统一到 `QueryContext`
4. 基于 admission barrier 继续上移剩余 ready-check，收敛“内层防御式判断”
5. 在已完成 target-dex 分区并行与 pending worklist 收敛的基础上，继续评估 `BuildCrossRefAggregates()` 更进一步的增量化/复用空间
6. 基于已接好的 history/classification benchmark 输出，继续评估是否需要可配置 history 容量、流式导出或更细粒度的 fairness/priority 对照实验
