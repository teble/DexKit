# 原生 smali 输出

实验性接口 `MethodData.toSmali()` 和 `ClassData.toSmali()` 按需读取查询结果所属的
原始 DEX，复用当前 bridge，不需要引入 smali/baksmali 运行时依赖。
Kotlin 调用处使用 `@OptIn(DexKitExperimentalApi::class)`。

```kotlin
val method = bridge.getMethodData("Lexample/Target;->run()V")!!
val fragment: String = method.toSmali()
val completeClass: String = bridge.getClassData("Lexample/Target;")!!.toSmali()
```

方法输出是从 `.method` 到 `.end method` 的片段，回编时需要原所属类的上下文。
身份检查只覆盖当前一次请求，独立导出的方法片段之间不共享身份映射。
整类输出包含类声明、源文件、接口、字段、初值、方法及注解。只有引用而没有定义的
类或方法返回 `NOT_DEFINED`，不会自动跳转到另一个 DEX 中的同名定义。

返回的字符串独立持有内容，可以在 bridge 关闭后继续使用；关闭后的新调用抛出
`SmaliException`，错误为 `BRIDGE_CLOSED`。请求复用 bridge 的查询准入和生命周期锁。
解析和输出状态按次释放，整类输出逐个释放方法的临时状态。bridge 借用的外部输入内存
在所有操作期间必须保持不变。

## 选项与错误

`org.luckypray.dexkit.smali.SmaliOptions` 默认使用 `SmaliDebugMode.NONE`，完全跳过
方法 debug 流解析，包括参数名、行号和局部变量事件；保留类 `.source`、参数注解和
异常处理表。`STRICT` 保留参数名和解释后的调试事件，包括位置、局部变量、源文件切换、
prologue 和 epilogue 标记。位置事件要求真实指令起点，其他事件还允许位于方法末尾；
无法表示的位置返回 `DEBUG_NOT_REPRESENTABLE`。编码截断、无效引用／寄存器、没有 live local
的 end、没有 ended local 的 restart 返回 `MALFORMED_INPUT`。校验会初始化隐式 this 和参数
local，包括宽参数，但不等同于完整 ART 验证。不会凭空插入初始行号事件。

输出、输入和条目预算在整个请求中累计，整类的所有成员共享；code-unit 上限按单方法计算，
嵌套深度上限按当前递归层级计算：

| 选项 | 默认值 | 含义 |
| --- | ---: | --- |
| `maxOutputBytes` | 16 MiB | UTF-8 输出字节数，同时约束临时文本片段 |
| `maxInputBytes` | 64 MiB | 累计访问字节数，重复读取引用也计入 |
| `maxCodeUnits` | 1,048,576 | 单方法最多的 16 位 code units |
| `maxItems` | 1,048,576 | 累计记录读取和方法计划的工作量预算 |
| `maxAnnotationDepth` | 64 | 注解／数组嵌套深度，上限 256 |

这些预算不是进程总内存上限：输出缓冲、编码转换缓冲和 Java String 可能同时存在。
Android 构建禁用 C++ 异常，不承诺从任意分配器内存耗尽中恢复。

`SmaliException` 提供稳定的错误码和阶段码、源 DEX／成员身份、
`containerByteOffset`、`codeUnitOffset` 和数值详情，未知偏移为 `-1`。
失败不返回部分文本，也不会使 bridge 失效。C++ 的 `DexKit::GetMethodSmali` 和
`GetClassSmali` 返回 `SmaliStatus`，失败时不改变调用者的输出字符串。
C++ 调用者须自行保证析构不会与该实例的其他操作并发。

## 兼容范围

读取器接受普通小端 DEX 035、037、038、039、040 和实验性 041 容器格式。
041 使用物理容器偏移，允许引用超出当前 logical DEX 范围的数据；这不表示能够回编出
原始容器布局或字节完全相同的 DEX。

标准指令覆盖 polymorphic/custom 调用、方法类型和方法句柄。
开发中的回编测试仅在宿主测试端使用 `org.smali:smali:2.5.2`、API 28，并通过 dexlib2
独立读取比对。各版本和边界用例仍在补充，当前分支尚未完成全部验收。

以下输入明确报错：CompactDex、odex/quickened 或保留指令、反向字节序／未知版本、
link data、hidden-API 元数据、共享或孤立的 switch payload、内容相同但源 ID 不同的
已引用 method handle（smali 会合并它们）、超出支持的无引号语法的
扩展名称（包括非 BMP Unicode 名称），以及 smali 无法保留的 encoded NaN payload／符号位。
字符串按 UTF-16 单元保留嵌入 NUL 和孤立代理项；名称限于支持的 BMP Unicode 语法。
检查范围是输出所需的结构，不等同于 ART 类型流验证，也不改变旧 bridge 加载器的安全边界。

自行构建原生库时可通过 `-DDEXKIT_ENABLE_SMALI=OFF` 移除实现，接口返回
`UNSUPPORTED`。体积实验与正式构建分开，正式构建使用 `DEXKIT_SMALI_SIZE_PROBE=none`。

Assembler 限制：[smali 2.5.2 的默认值判断](https://github.com/JesusFreke/smali/blob/v2.5.2/dexlib2/src/main/java/org/jf/dexlib2/util/EncodedValueUtils.java)
把静态 `-0.0` 当成默认零值。它裁剪默认值后缀时可能丢弃符号位，即使后面还有其他默认值
字段也会发生。DexKit 输出精确的带符号十六进制字面量，但这种输入不在未修正 assembler 的
端到端位级保真保证内。这是值变化，不能仅用“DEX 字节不保证相同”解释。测试分别检查原生
文本和这一已知 oracle 限制；其他原始位模式回编测试通过后置非零字段保留输入。
