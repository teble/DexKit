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

原生工作任务应在会话内调用 `DescriptorBorrowScope::Capture()`，在工作线程上建立
`DescriptorBorrowScope scope(context)`。该 context 只借用当前 bridge 的会话，不拥有 bridge、
不延长会话，也不继承其他 bridge 的外层会话。结束拥有者会话前必须等待所有工作线程完成。
普通查询 API 与内部工作线程会自行建立作用域；返回的 FlatBuffer 拥有已序列化的描述符字节。
Java/Kotlin 查询 API 及现有示例保持原有生命周期行为。并发关闭或修改 bridge 仍不属于受支持
的原生生命周期。
