# 在桌面平台运行

> 从 `1.1.0` 开始，`DexKit` 支持桌面平台运行，无需打包成 apk 在 Android 进行测试工作。

## 安装环境

需要 gcc/clang、cmake 以及 ninja/make 作为基础运行环境。 

### Windows

`Windows` 用户可以使用 [MSYS2](https://www.msys2.org/) 搭建运行环境。由于目前Windows系统均为64位，
所以我们使用 `mingw64.exe` 进行依赖安装：

```shell
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
```

安装完成后，我们需要将 `mingw64/bin` 目录添加进环境变量中便于后续使用。

::: warning warning
DexKit 默认将使用 `ninja` 作为默认构建系统，如果您需要在 mingw 中使用 `make` 进行构建， 需要执行 
`pacman -S mingw-w64-x86_64-make`，安装完成后需要将 `msys64\mingw64\bin\mingw32-make.exe`
重命名为 `make.exe` 或者添加为快捷方式，否则会由于 `gradle-cmake-plugin` 找不到 `make` 命令而构建失败。
同时删除 `:dexkit/build.gradle` 中的 `generator.set(generators.ninja)` ，或者修改为 
`generator.set(generators.unixMakefiles)`
:::

### Linux

对于Linux用户，正常情况只需要安装 `ninja` 即可。

### MacOS

推荐使用 [HomeBrew](https://brew.sh/) 进行依赖管理，

```shell
brew install cmake ninja
```

## 克隆 DexKit

```shell
git clone https://github.com/LuckyPray/DexKit.git
```

## 开始使用

执行子模块 `:main` 即可进行测试。

```shell
gradle :main:run
```

## 原生 MCP 服务器

实验性的 `dexkit-mcp` 使用 Rust 和现有 C++ Core 提供标准 **stdio** MCP 入口，
运行时不需要 JVM。当前原生构建配置面向 macOS 和 Linux；上面的 Windows/JVM 说明
不代表 Rust 服务器已经提供 Windows 支持。

构建需要 Rust 1.88+、CMake、C++20 编译器和 zlib 开发文件：

```sh
cargo build --release --manifest-path mcp/Cargo.toml --locked
mcp/target/release/dexkit-mcp --allow-root /path/to/apks
```

在 MCP 客户端中使用本机的绝对路径配置：

```json
{
  "mcpServers": {
    "dexkit": {
      "command": "/path/to/DexKit/mcp/target/release/dexkit-mcp",
      "args": ["--allow-root", "/path/to/apks"]
    }
  }
}
```

可重复指定 `--allow-root`，默认只接受当前工作目录及其子目录。打开时读取输入快照，
后续分析不再依赖源文件的变化，也不修改源 APK/DEX。指纹覆盖整个原始文件，包括完整
容器或压缩包。APK 读取连续的 `classes.dex`、`classes2.dex` 等条目，序号缺口及解压失败
明确报错。
文件读取沿已持有的允许目录句柄逐级打开，拒绝检查后被替换的符号链接；FIFO 等非普通
文件会直接被拒绝，不会阻塞等待。

官方 SDK 处理旧版 initialize 握手和 2026-07-28 协议。stdout 只承载 MCP 消息。
原生分析在独立子进程中串行执行，原生日志在进程启动时永久导向 stderr。工作进程崩溃
返回 `WORKER_EXITED`，此时需重启服务器并重新打开输入，旧句柄全部失效。该子进程用于
隔离运行失败，不是安全沙箱，也不是完整 DEX 验证器。取消调用方等待不会中断正在执行的
Core 查询；当前不提供原生取消和通用结果数量上限。
SDK 取消 token 会结束调用方等待，并跳过尚未开始的排队任务；SDK 不再发送已取消请求
的响应。
意外的 C++ 异常同样使工作进程失效。取消等待不会回滚已经完成的操作，保留状态仍可能
占用配额。可关闭已知实例立即回收；若丢失了打开实例的响应，则重启服务器回收。
结果集和 artifact 也会自动过期。

### 工具与查询契约

所有工具都使用 `dexkit_v1_` 前缀：

| 工具 | 作用 |
| --- | --- |
| `open`、`close`、`capabilities` | 管理输入生命周期、指纹及实际能力 |
| `find_classes`、`find_methods`、`find_fields` | 类型化条件查询，保留完整结果集 |
| `describe`、`relations` | 元数据、按需注解／指令信息，以及直接关系 |
| `page` | 读取已保留结果的后续页，不重新运行查询 |
| `smali`、`read_artifact` | 有界文本输出及受管理资源的分段读取 |

`open` 返回 `instanceId` 后，例如这样查找方法：

```json
{
  "name": "dexkit_v1_find_methods",
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

### 返回值、资源与限制

业务返回值统一为 `{"ok":true,"data":...}` 或 `{"ok":false,"error":...}`，放在
`structuredContent`，同时由同一对象生成文本回退。业务错误设置 MCP `isError`；未知
工具、非法协议请求走协议错误。零匹配是成功的空结果，编码失败或预算超限不能变成空结果。

查询完整执行并保留结果，按签名和 DEX 身份排序后分页；`pageSize` 仅控制返回页。
`coverage: "complete"` 不表示第一页已经包含全部结果。结果集和 artifact 在创建
15 分钟后过期，关闭实例立即使其失效。过期 cursor 明确报错，不会悄悄重跑查询。

当前上限包括 4 个实例、32 个结果集、每实例 50,000 个缓存实体／32 MiB 实体元数据、
200,000 个保留结果引用、1 MiB 业务请求／响应、请求 32 层／10,000 个 JSON 节点、
响应 64 层／100,000 个 JSON 节点、
256 MiB 输入文件、单压缩包 512 MiB 解压 DEX 总量。每页 1..500 行。
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

### 开发与扩展

运行 `python3 mcp/test.py` 可执行独立 Core 互通、真实 stdio 管道和已公布 Schema 的
请求／响应校验。测试依赖仓库的 Java／Android 环境来组装 smali 样本。Android 发布构建
不链接 Rust、JSON、MCP 依赖。

新增可选条件不得改变已有含义或默认行为；不同目标的新查询应增加独立工具。
需要同步更新公开 DTO、校验、原生映射、字段覆盖／编码清单、语义测试、能力登记和契约版本。
FBS 新字段、枚举、union 未分类时构建失败，但清单本身不能证明类型、默认值或语义变化兼容。
破坏公开行为时应使用新的 `dexkit_v2_` 契约。HTTP、保存查询的重放、批量查询工具、
通用正则和传递调用路径暂不提供。
