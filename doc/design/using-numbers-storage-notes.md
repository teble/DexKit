# using-numbers 存储与释放问题记录

## 背景

`using_numbers` 的查询命中频率相对有限，但一旦做成全量缓存，构建与释放成本都很高：

- 一个 dex 中可能包含大量 number 指令
- 全量预热会把很多几乎不会再次使用的数据常驻下来
- 即便只在命中 method 时做懒缓存，若缓存载荷仍是 `vector<EncodeNumber>`，重复 method 多时仍会积累较高的内存与 release 成本

因此，`using_numbers` 不适合直接并入当前 bridge 级的全量 warm-up barrier。

## 当前阶段已落地的收敛

当前实现选择：

- 保持 `using_numbers` 作为 method matcher 末位条件
- 不通过 `Analyze(MethodMatcher)` 把它推导为 bridge 级 `kUsingNumber` 全量预热
- 在 `DexItem` 内改为 **per-method 稀疏懒缓存**
  - slot 数组固定长度
  - 某个 method 首次命中时才构建其 numbers
  - 同一个 method 只允许一个 builder，其它线程等待 ready

这一步优先解决的是：

- 并发安全
- 避免重复 query 每次都重新扫整段 bytecode
- 避免单个 query 把整个 dex 的 using-numbers 全量铺开

## 当前仍存在的问题

即便采用 per-method 稀疏懒缓存，若缓存单元仍然是 `vector<EncodeNumber>`，仍会面临：

1. **载荷偏大**
   - `EncodeNumber` 本身存在类型 tag、union 和对齐开销

2. **分配拓扑偏碎**
   - method 数量大时，会形成大量小 vector / 小块堆分配

3. **释放成本高**
   - `DexItem` 销毁时需要回收大量 method 级小块内存

4. **匹配路径仍有进一步优化空间**
   - 当前 `using_numbers` matcher 仍然围绕完整 number 列表工作，后续仍可继续收敛

## 后续可讨论的候选方向

这部分暂不进入当前实现，只记录为后续候选：

### 方向 A：packed value arena

思路：

- 不再存 `EncodeNumber` 对象数组
- 改存压缩后的 packed number 值
- method 侧只保留 slice（offset/count）
- 所有数据集中放入一个 arena

优点：

- 能显著降低单元素体积
- 重复查询不需要重新从 dex code 解码
- release 成本比 method 级小 vector 更低

### 方向 B：packed ref / 引用型缓存

思路：

- 不缓存 number 值本身
- 只缓存 number 指令在 code 中的位置信息 / 引用
- 查询时按 ref 回到 dex code 解码

优点：

- 元素载荷更小
- 更接近“零拷贝”

代价：

- 重复查询仍需重复解码 number
- CPU 成本可能高于 packed value

## 当前建议

在现阶段，优先级仍是：

1. 先把 native 并发 query 的状态边界、matcher 缓存边界、warm-up barrier 收敛清楚
2. `using_numbers` 先保持“并发安全的稀疏懒缓存”实现
3. 等主线并发改造稳定后，再单独评估：
   - 是否需要把 `vector<EncodeNumber>` 进一步收敛为 arena 形式
   - 是否值得走 packed value 或 packed ref 方案

这份文档仅记录问题背景与候选方向，暂不要求立即落地。

## 当前新增的验证工具

为了更直接判断 `using_numbers` 是否是 `initFullCache` / release 抖动的重要来源，当前已补充一个专项压测入口：

```powershell
./gradlew.bat --no-daemon :main:runInitFullCacheBenchmark
```

支持的关键环境变量：

- `DEXKIT_BENCH_APK`：目标 APK 路径
- `DEXKIT_BENCH_NATIVE_THREADS`：native 线程数
- `DEXKIT_BENCH_WARMUP`：预热轮次
- `DEXKIT_BENCH_ITERATIONS`：正式采样轮次
- `DEXKIT_BENCH_INCLUDE_USING_NUMBERS`：是否把 `using_numbers` 纳入 `initFullCache`，可取 `true/false`

输出会拆分三段耗时：

- `createMs`
- `initMs`
- `releaseMs`

因此可以直接做：

1. `includeUsingNumbers=false`
2. `includeUsingNumbers=true`

两组 A/B 对照，观察 `release` 是否随 `using_numbers` 显著放大；如果放大明显，就说明当前瓶颈更偏向 `DexItem` 级大量 `vector<EncodeNumber>` 的构建/释放，而不是 query-local matcher cache。

## 当前新增实验：full-cache using-numbers 连续存储

为了真正处理“分配/释放拓扑”而不只是测时间，当前已额外做了一个较小范围的存储收敛实验：

- **只针对 `InitFullCache(includeUsingNumbers=true)` 的 full-cache 路径**
- 把原先的：
  - `std::vector<std::vector<EncodeNumber>> method_using_numbers`
- 改成：
  - 一块 `DexItem` 级连续存储 `method_using_numbers_storage`
  - 外加每个 method 的 `offset/size` range
- `GetUsingNumbers()` 改为返回只读 view（`span`）
- lazy using-numbers 路径先保持原状，不在这轮里一起改

这样做的直接目的有两个：

1. 避免 full-cache 时为每个 method 单独分配一个小 `vector`
2. 让释放从“大量 method 级小块释放”收敛成“少量大块释放”

### 这轮实验的边界

- 它主要改善的是 **full-cache + using-numbers** 场景下的内存分配拓扑
- 不直接改变普通 query 的 lazy using-numbers 行为
- 因此它更适合配合 `runInitFullCacheBenchmark` 观察 `releaseMs` 是否回落，而不是直接拿普通 10-query 场景判断

### 当前 smoke 结果

在本地大 APK、`nativeThreads=4`、`warmup=1`、`iterations=3` 的一次 smoke 中：

- `includeUsingNumbers=false`
  - `initAvgMs ~= 100.33`
  - `releaseAvgMs ~= 90.33`
- `includeUsingNumbers=true`
  - `initAvgMs ~= 96.67`
  - `releaseAvgMs ~= 98.33`

这组数据目前只能说明：

- 结构已经切到“连续存储 + range”方案，功能与回归都正常；
- 但收益是否显著，还需要你那边更大样本、更多轮次的数据继续确认；
- 如果后续 `includeUsingNumbers=true` 的 `release` 仍明显高于 `false`，那就说明仅仅把 method 级小 vector 收敛掉还不够，下一步还要继续追 `EncodeNumber` 总载荷与构建阶段的内存拓扑。
