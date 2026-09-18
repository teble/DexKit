# 性能优化

在 DexKit 中，多种查询或许能实现同样的功能，但是性能差距却可能相差几十倍。本节将介绍一些性能优化的技巧。

在 native 层，DexKit 会维护 Dex 中的类、方法以及字段的列表，那么在几个 API 中，DexKit
是如何扫描这些列表的呢？`findClass`、`findMethod`、`findField` 
的遍历顺序均是按照各自列表的先后顺序进行遍历，然后再逐一对各个条件进行匹配。

## declaredClass 条件过重

可能有些用户在使用 `findMethod` 或 `findField` 时，会使用 `declaredClass` 条件写出如下的查询：

```kotlin
private fun badCode(bridge: DexKitBridge) {
    bridge.findMethod {
        matcher {
            declaredClass {
                usingStrings("getUid", "", "_event")
            }
            modifiers = Modifier.PUBLIC or Modifier.STATIC
            returnType = "long"
            addInvoke {
                name = "parseLong"
            }
            addInvoke {
                name = "toString"
            }
        }
    }.single().let {
        println(it)
    }
}
```

这个搜索耗时 `4310ms`。
乍一看这个查询似乎没有什么问题，但是实际上这个查询的性能是非常差。为什么？前面提到过，`findMethod` API
会遍历一遍所有的方法，然后再逐一对各个条件进行匹配。而 method 与 class 之间却是一个多对一的关系，即一个
class 中可能包含多个 method，但是一个 method 只能属于一个 class。因此，遍历所有方法的过程中，每个 method
都会被匹配一次 `declaredClass` 条件，这就导致了性能的浪费。

那么，我们换一个思路，先搜索 declaredClass，配合链式调用就能在符合条件的类中再搜索 method，这样不就可以避免了吗？

```kotlin
private fun goodCode(bridge: DexKitBridge) {
    bridge.findClass {
        matcher {
            usingStrings("getUid", "", "_event")
        }
    }.findMethod {
        matcher {
            modifiers = Modifier.PUBLIC or Modifier.STATIC
            returnType = "long"
            addInvoke {
                name = "parseLong"
            }
            addInvoke {
                name = "toString"
            }
        }
    }.single().let {
        println(it)
    }
}
```

这个搜索耗时 `77ms`, 性能提升了数十倍之多。

在使用 `findMethod` 或 `findField` 时，尽量避免使用 `declaredClass` 附带过于复杂的逻辑。
## 原生描述符存储实验

此实验分支增加 `DEXKIT_EXPERIMENT_VECTOR_DESCRIPTORS`，默认关闭。Gradle 使用
`-PexperimentVectorDescriptors=ON` 启用；它与 node、sparse、hybrid 描述符存储实验互斥。
`DEXKIT_EXPERIMENT_VECTOR_DESCRIPTORS_NO_PROMOTION`（Gradle 参数为
`-PexperimentVectorDescriptorsNoPromotion=ON`）保留相同的新稀疏路径与借用约定，
只关闭转换，用作对照。

每个 DEX 的方法、字段域分别从稀疏存储开始。当可回收的结构分配达到密集字符串与 ready
数组的成本后，下一次顶层操作会等待已进入的操作结束，并与缓存预热互斥，再丢弃旧字符串。
新 vector 的内容按需重建。触发阈值的那次操作可继续使用旧视图直到序列化结束；若直接关闭，
则不执行待定转换。此选项仍是实验，不保证性能收益，也不提供分配失败后的恢复承诺。

启用后，直接使用包含方法或字段描述符的 C++ Bean 时，需先建立
`auto borrow = bridge.BorrowDescriptors(required_flags)`。此会话不可移动，必须留在创建它的
线程上，直到所有 Bean、描述符视图及使用它们的任务完成。若需跨会话保留内容，应先复制到
拥有内容的字符串中。`required_flags` 指定进入会话前所需的缓存预热。在会话内使用底层
`DexItem` 访问器，不可再次进入同一个 bridge 的顶层 API。缺失会话或同 bridge 重入会中止
进程，Release 构建也会检查。
持有会话时，也不可等待其他线程完成同一个 bridge 上独立进入的顶层 API：该 API 可能需要
执行维护，而维护又在等待当前会话。线程局部的重入检查无法检测此类跨线程等待环。应先完成
准备并释放会话，再启动及等待独立 API；活跃会话内的工作线程应共享其 context，使用底层访问器。

原生工作任务应在会话内调用 `DescriptorBorrowScope::Capture()`，在工作线程上建立
`DescriptorBorrowScope scope(context)`。该 context 只借用当前 bridge 的会话，不拥有 bridge、
不延长会话，也不继承其他 bridge 的外层会话。结束拥有者会话前必须等待所有工作线程完成。
普通查询 API 与内部工作线程会自行建立作用域；返回的 FlatBuffer 拥有已序列化的描述符字节。
Java/Kotlin 查询 API 及现有示例保持原有生命周期行为。并发关闭或修改 bridge 仍不属于受支持
的原生生命周期。
owner 检查无法检测已过期的 context，因此不能在原会话结束后复用它；读取描述符的任务析构
也必须包含在工作任务的生命周期内。使用多个 bridge 的会话时，调用方需避免相反的获取或等待顺序。

## 无缓存成员描述符

`DEXKIT_EXPERIMENT_UNCACHED_DESCRIPTORS` 默认关闭。Gradle 使用
`-PexperimentUncachedDescriptors=ON`，同时开启 `experimentStructuralDescriptors` 和
`experimentRawDescriptorLookup`。此选项与包括 vector 在内的其他描述符存储实验互斥。
fast-hit 选项不影响无缓存描述符访问。

开启后，`MethodBean::dex_descriptor` 和 `FieldBean::dex_descriptor` 是拥有内容的
`std::string`。复制 Bean 会复制字符串，移动 Bean 会转移所有权。从 Bean 取得的 `string_view`
仍然借用该 Bean 的字符串，不可超过其生命周期或跨越使视图失效的移动、修改；不要保存从临时
Bean 中取得的视图。该选项改变 C++ 类型及 ABI，原生调用方与库必须使用一致的编译选项。

每次请求方法或字段描述符都会重新生成。bridge 不保留描述符数组、哈希表、ready 字节或描述符
发布锁，也不使用 vector 实验的借用会话。现有缓存预热、查询准入和关闭、修改 bridge 的限制
继续适用。类描述符及其他借用 DEX 的视图仍遵守原生命周期要求；此选项不使所有原生元数据拥有
底层数据。

结果汇总和跨 DEX 文本去重使用仍然存活的 Bean 字符串。已序列化 FlatBuffer 与 Java/Kotlin
结果的字节及 API 行为保持不变。重复输出会重复生成字符串，同时存活的结果可能增加临时分配。
性能需要按完整工作负载评估，取消长期缓存并不保证进程峰值内存一定降低。

## 窄类型原生索引

`DEXKIT_EXPERIMENT_NARROW_TYPES` 默认关闭；Gradle 使用 `-PexperimentNarrowTypes=ON`。
该选项将本地方法 ID、类定义索引缩为 16 位，将 caller、字段 reader/writer 记录缩为 4 字节，
将紧凑关系索引的偏移缩为 32 位，保留原来的 vector 扩容方式和行内顺序。字符串 ID 和行长度
仍为 32 位，因此保留 jumbo 字符串引用和超过 65535 项的长列表。公开 Bean/schema 的 ID 类型不变。

此实验表示要求每个 DEX 最多包含 65536 个方法 ID，并在初始化时检查该上限。ID 65535 仍是
合法值，不作为空值标记。不能表示的累计偏移、计数和窄化转换会触发始终启用的检查，不会静默
回绕。这些是实验限制，不代表原 Reader 接受的所有输入都满足窄类型范围；需要更宽输入域时应
关闭此开关。内部 C++ 布局和类型随开关改变，原生调用方必须使用一致的编译定义。Java/Kotlin
API 和结果序列化格式保持不变。
