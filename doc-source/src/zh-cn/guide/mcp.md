# 原生 MCP 服务器

实验性的 `dexkit-mcp` 使用 Rust 和现有 C++ Core，提供 **Streamable HTTP** 与
**stdio** 两种 MCP 入口，运行时不需要 JVM。原生构建配置面向 macOS 和 Linux；
本地验收环境为 macOS arm64，当前适配层暂不支持 Windows。

在仓库根目录构建，需要 Rust 1.88+、CMake、C++20 编译器及 zlib 开发文件：

```sh
cargo build --release --manifest-path mcp/Cargo.toml --locked
```

## Streamable HTTP

启动并保持服务器运行：

```sh
mcp/target/release/dexkit-mcp --transport http --listen 127.0.0.1:7331 --allow-root /path/to/apks
```

MCP 客户端连接地址为 `http://127.0.0.1:7331/mcp`。对于使用 JSON `mcpServers`
配置的客户端，URL 配置通常如下，具体传输字段以对应客户端文档为准：

```json
{
  "mcpServers": {
    "dexkit": {"url": "http://127.0.0.1:7331/mcp"}
  }
}
```

官方 MCP SDK 在这个单一端点处理 JSON-RPC 和请求级 SSE 响应。这是 Streamable HTTP，
不是旧版独立 SSE／消息端点传输。2026-07-28 协议无需 initialize；旧版客户端仍可握手，
后续请求携带 `MCP-Protocol-Version`。HTTP 采用无状态路由，不分配 `Mcp-Session-Id`，
GET 和 DELETE 返回 405。`resources/read` 也是 MCP POST 调用，不是对 artifact URL
发起普通 HTTP GET。

**DEX 实例的生命周期独立于 HTTP 连接。** 常驻原生 worker 保存已打开输入，
`instanceId` 可跨请求、新 TCP 连接和客户端重新握手使用。调用 `dexkit_close`
释放实例；客户端断连不会自动关闭它。服务器退出后全部句柄失效。
这是本机单用户服务：所有已连接客户端共享允许目录、分析状态及配额。
客户端名称、实例 ID 不承担认证或租户隔离作用。

监听地址只接受 loopback。Host 与 Origin 按实际绑定地址及同端口 localhost 校验；
原生客户端可省略 Origin，来自其他站点的浏览器 Origin 会被拒绝。
当前不提供远程部署或认证模式。端口设为 0 时自动选取空闲端口，实际端点输出到 stderr。

HTTP 请求体至多 2 MiB，分块上传同样受限；读取请求体的期限为 10 秒；最多同时处理
16 个请求／响应流。超限分别返回 HTTP 413、408、503，503 附带 Retry-After。
请求体期限不限制原生分析时长。关闭新版 HTTP 请求会取消调用方等待，并尽可能跳过
排队任务，已开始的原生操作仍会完成。旧版无状态通知可被接收，但不作为独立取消通道。
SIGINT／SIGTERM 会停止监听，最多等待 3 秒完成 HTTP 关闭，再释放 worker 和匿名 artifact。

## stdio 兼容入口

已有的 command 配置保持兼容：不指定传输方式时仍默认 stdio，也可显式指定：

```sh
mcp/target/release/dexkit-mcp --transport stdio --allow-root /path/to/apks
```

```json
{
  "mcpServers": {
    "dexkit": {
      "command": "/path/to/DexKit/mcp/target/release/dexkit-mcp",
      "args": ["--transport", "stdio", "--allow-root", "/path/to/apks"]
    }
  }
}
```

## 输入与 worker 生命周期

可重复指定 `--allow-root`，默认只接受当前工作目录及其子目录。打开时检查文件元数据，
再由 C++ 映射同一个已打开的文件句柄，不把整个 APK 复制到内存，也不生成
临时 APK。只保留分析所需的 DEX：未压缩条目和原始 DEX 复制一次，压缩条目保留解压后的
缓冲区。源 APK/DEX 不会被修改。`open` 返回 `instanceId`、`byteLength` 和
`dexCount`，不计算整包文件指纹。
APK 读取连续的 `classes.dex`、`classes2.dex` 等条目，序号缺口及解压失败
明确报错。
文件读取沿已持有的允许目录句柄逐级打开，拒绝检查后被替换的符号链接；FIFO 等非普通
文件会直接被拒绝，不会阻塞等待。

源文件在 `open` 完成前需保持不变；完成后可以修改或删除，不影响实例。原生加载前后
会检查文件大小、mtime 和 ctime，检测到并发变化时返回可重试的 `INPUT_CHANGED`。这不是文件系统原子
快照；若映射期间源文件被截断，也可能导致 worker 退出并返回 `WORKER_EXITED`。

默认由 Core 自动检测 CPU 线程数（`--threads 0`，至少一个线程）。可以用
`--threads N` 覆写，例如 `--threads 4`。该设置同时控制每个实例的 DEX 并行加载、
缓存初始化和查询，HTTP 与 stdio 均适用。`capabilities.nativeThreads` 返回实际采用的
线程数。这是 Core 内部工作线程的预算，不是进程总线程数或 MCP 请求并发数。

APK 加载复用 Core 的并行 ZIP 解压与批量 `AddImage` 初始化。适配层在解压前检查总预算，
在初始化前检查每个 DEX 的头部，并按输入顺序保留独立持有的 DEX 数据。
同名类定义出现在多个 DEX 时，Core 的声明查找固定采用最后一个逻辑 DEX 中的定义，
不受线程完成顺序影响。

两个传输入口还共用以下字节预算参数：

| 参数 | 默认值 | 控制范围 |
| --- | --- | --- |
| `--max-input-mib N` | `0`（不限制） | 整个 APK 或原始 DEX 文件 |
| `--max-dex-mib N` | `512` | 原始 DEX 字节数，或单个 APK 内所有 DEX 的解压总量 |

任一参数设为 `0` 即关闭对应字节上限。例如 `--max-dex-mib 1024` 允许 1 GiB DEX
数据，不会因 APK 内资源文件较大而拒绝打开。`capabilities` 的 `maxInputBytes`、
`maxDexBytes` 返回实际生效的字节值，`0` 表示未配置上限。所有 DEX 的总预算会在
解压或建立索引前统一检查；超限返回 `LIMIT_EXCEEDED`，并指出应调整的启动参数。
平台可映射范围以及 APK/DEX 格式本身的支持范围仍然适用。

官方 SDK 处理旧版 initialize 握手和 2026-07-28 协议。stdout 只承载 MCP 消息。
原生 MCP 调用在独立子进程中逐个执行，单次调用内部仍使用 Core 的并行能力；原生日志
在进程启动时永久导向 stderr。工作进程崩溃
返回 `WORKER_EXITED`，此时需重启服务器并重新打开输入，旧句柄全部失效。该子进程用于
隔离运行失败，不是安全沙箱，也不是完整 DEX 验证器。取消调用方等待不会中断正在执行的
Core 查询；当前不提供原生取消和通用结果数量上限。
对于 stdio，SDK 取消通知会结束调用方等待，并跳过尚未开始的排队任务；SDK 不再发送
已取消请求的响应。
意外的 C++ 异常同样使工作进程失效。取消等待不会回滚已经完成的操作，保留状态仍可能
占用配额。可关闭已知实例立即回收；若丢失了打开实例的响应，则重启服务器回收。
结果集和 artifact 也会自动过期。

## 工具与查询契约

如果客户端隐藏嵌套参数，或将查询显示为 `query: unknown`，可以通过
`dexkit_get_query_schema` 按需读取真实契约。帮助通过普通工具结果返回，
无需先打开 APK；原生 worker 忙碌或失效时仍可读取。

```json
{"tool":"dexkit_find_methods"}
```

省略 `pointer` 返回参数概览、导航链接、语义规则及示例；使用示例时，将实例占位值
替换为 `open` 返回的真实 ID。传入 `"pointer":""` 获取完整输入 schema，或复制
`links` 中的 JSON Pointer 读取原文片段。递归类型保留引用并提供后续链接。
片段中的引用相对该工具的完整输入 schema 解析，`standalone:false` 表示它不能单独
作为完整验证器。可选的 `ifSchemaHash` 用于确认文档与先前结果一致；遇到
`SCHEMA_CHANGED` 应重新读取概览。摘要只覆盖 schema，不代表原生行为或说明文字相同。

帮助与 `tools/list` 使用同一份最终契约，原有参数校验仍然生效。这项兼容功能补充模型
缺失的参数信息，不会关闭客户端的 schema 精简策略。业务帮助回复上限为 16 KiB，
MCP 工具结果（文本及结构化内容）上限为 64 KiB，不含 JSON-RPC／SSE 外层。
`linksComplete:false` 表示导航列表不完整；超限返回 `SCHEMA_SECTION_TOO_LARGE` 和子节点链接，
不会截断 JSON。查询契约保持 v1，帮助包络使用 `discoveryVersion: 1`。

所有工具都使用稳定的 `dexkit_` 前缀，名称中不包含版本号。
`serverInfo.version` 表示程序发布版本；capabilities／查询帮助中的 `apiMajor`、
`contractRevision` 表示业务契约版本；`schemaHash` 仅标识输入 schema。
这些信息不会自动完成工具版本协商。

可用工具如下：

| 工具 | 作用 |
| --- | --- |
| `get_query_schema` | 无需实例即可读取查询契约、示例及 JSON Pointer 片段 |
| `open`、`close`、`capabilities` | 管理输入生命周期、文件字节数、DEX 数量及实际能力 |
| `find_classes`、`find_methods`、`find_fields` | 类型化条件查询，保留完整结果集 |
| `describe`、`relations` | 元数据、按需注解／指令信息，以及直接关系 |
| `page` | 读取已保留结果的后续页，不重新运行查询 |
| `smali`、`read_artifact` | 有界文本输出及受管理资源的分段读取 |

`open` 返回 `instanceId` 后，例如这样查找方法：

```json
{
  "name": "dexkit_find_methods",
  "arguments": {
    "instanceId": "returned-instance-id",
    "query": {
      "scope": {"searchPackages": ["com.example"]},
      "matcher": {
        "usingStrings": [{"value": "session expired", "match": "contains"}]
      }
    },
    "pageSize": 50
  }
}
```

这里展示的是工具名和业务参数，JSON-RPC 外壳及协议元数据由客户端／SDK 提供。
`--dump-schema` 可以导出当前准确的工具输入、输出 Schema。公开 JSON 不直接等同于
FBS JSON；未知字段、枚举会递归拒绝。可选条件应省略，不能普遍用 null 代替。
只有 `parameters.parameters` 数组中的 null 表示保留位置的通配参数；空参数数组表示
零参数，空候选集合表示无候选。空 `allOf`／`anyOf`／`noneOf` 数组明确拒绝。

`className` 使用 Java 风格类型名，返回的 `descriptor` 使用 DEX 签名。
字符串匹配支持 `equal`、`contains`、`startWith`、`endWith`，不支持通用正则。
空字符串模式只能使用 `equal`。适配层处理包括 NUL 和 emoji 在内的正常 Unicode，
不进行 Unicode 规范化，也不改变 Core 的匹配规则。

集合条件保留原生的一对一匹配语义，要求同一元素同时满足的条件应放在同一个 matcher
中。计数范围显式给出 `min`、`max`；集合 matcher 使用 `matchType`，单个字符串和 flags
条件使用 `match`。modifiers 与原始 access flags 是两项不同条件。
`usingNumbers` 保留类型，例如 `{"type":"int64","value":"9223372036854775807"}`；
浮点查询只接受有限数值，使用 Core 数值比较，不声称按位匹配。指令数字结果保留
`opcode` 和十六进制 `rawBits`。注解浮点位模式反映 Core 解码后的元数据，保留零的正负
号；它不是原始 DEX 中变长编码字节的表示。

`scope.within` 在 `entityIds` 与 `resultSetId` 中二选一，并检查实体种类和实例归属。
`scope.inClasses` 可用类句柄缩小方法／字段搜索范围。实体 ID 和 cursor 都是不透明、
实例内有效的引用，不能保存为永久主键。默认结果包含签名、flags、来源 DEX 索引；
`select` 可选择 `descriptor`、`flags`、`source` 的任意子集。来源 DEX 索引是当前快照
中的 logical 索引，不是 ZIP 条目名称，也不独立构成永久身份。

关系包括 `invokes`、`callers`、`fieldReaders`、`fieldWriters`、`declaredMethods`、
`fieldReferences`，都是直接关系。`fieldReferences` 按 DEX field ID 表列举，可能包含
仅引用而未定义的字段，不能当作字段声明列表。通用方法／字段查询也可能包含定义类中的
引用项，对这类结果输出 smali 可能得到 `NotDefined`。

## 返回值、资源与限制

业务返回值统一为 `{"ok":true,"data":...}` 或 `{"ok":false,"error":...}`，放在
`structuredContent`，同时由同一对象生成文本回退。业务错误设置 MCP `isError`；未知
工具、非法协议请求走协议错误。零匹配是成功的空结果，编码失败或预算超限不能变成空结果。

查询完整执行并保留结果，按签名和 DEX 身份排序后分页；`pageSize` 仅控制返回页。
`coverage: "complete"` 不表示第一页已经包含全部结果。结果集和 artifact 在创建
15 分钟后过期，关闭实例立即使其失效。过期 cursor 明确报错，不会悄悄重跑查询。

当前上限包括 4 个实例、32 个结果集、每实例 50,000 个缓存实体／32 MiB 实体元数据、
200,000 个保留结果引用、1 MiB 业务请求／响应、请求 32 层／10,000 个 JSON 节点、
响应 64 层／100,000 个 JSON 节点。DEX 字节预算默认每输入 512 MiB，可配置或关闭；
APK 容器大小默认不限制。每页 1..500 行。
原生结果复制上限为 64 MiB，但检查发生在 Core 构造结果之后；这些不是进程总内存或
CPU 时间的硬上限。
注解元数据转换另设 32 步递归上限。超过响应限制时返回业务错误，实例仍可继续使用。

`smali` 接受类或方法 `entityId`、`debug: "none" | "strict"`、`maxOutputBytes`
（默认 1 MiB，上限 16 MiB）和 `delivery: "inline" | "artifact"`。inline 上限
64 KiB。artifact 返回 `dexkit://artifacts/...` URI 和 SHA-256，小资源可通过
`resources/read` 读取，大资源用 `read_artifact` 按 `startByte`／`nextByte` 分段读取，
每段至多 64 KiB，边界保持 UTF-8 完整。artifact 是受管理的临时文件，合计上限为
32 MiB／16 个，不提供任意文件读写。原生生成失败不返回部分 smali，并保留原始错误码、
阶段、成员身份及偏移。具体格式范围见 [smali 契约](./smali.md)。

Rust 接口明确拒绝无法表示的孤立 UTF-16 代理项元数据；smali 仍可用 ASCII 转义保存
这些单元。JVM 接口直接将 DEX MUTF-8 解码为 Java UTF-16，NUL、emoji 和注解数组现在
返回实际字符串，而不是替代的 `\uXXXX` 文本。批量查询的分组标签仍使用标准 UTF-8，
不会修改进程全局的 FlatBuffers codec。

## 开发与扩展

运行 `python3 mcp/test.py` 可执行独立 Core 互通、真实 stdio 管道、HTTP 网络请求、官方 SDK HTTP 客户端，以及已公布 Schema 的
请求／响应校验。测试依赖仓库的 Java／Android 环境来组装 smali 样本。Android 发布构建
不链接 Rust、JSON、MCP 依赖。

新增可选条件不得改变已有含义或默认行为；不同目标的新查询应增加独立工具。
需要同步更新公开 DTO、校验、原生映射、字段覆盖／编码清单、语义测试、能力登记和契约版本。
FBS 新字段、枚举、union 未分类时构建失败，但清单本身不能证明类型、默认值或语义变化兼容。
兼容变更保持工具名稳定。破坏公开行为时需要明确提升契约主版本并制定迁移策略；
只有需要不兼容契约同时存在时，才使用不同工具名或服务入口。
保存查询的重放、批量查询工具、通用正则和传递调用路径暂不提供。
