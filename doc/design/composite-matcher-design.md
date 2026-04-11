# Composite Matcher 设计草案

> 状态：Draft  
> 范围：`dexkit/` 查询 DSL、schema 与 native matcher 执行模型的逻辑组合能力  
> 相关文档：
> - `doc/design/nested-matcher-optimization-notes.md`
> - `doc/design/native-query-concurrency-design.md`
> - `doc/design/native-query-concurrency-progress.md`

## 1. 背景

DexKit 当前 matcher 模型的核心特点是：

- 一个 matcher 对应一个目标对象
- matcher 内部普通字段默认按隐式 `AND` 组合
- `MethodsMatcher` / `FieldsMatcher` / `AnnotationsMatcher` 这类集合 matcher 继续承担“contains / equal / count”这类集合语义

这套模型对大多数简单查询是足够的，但它无法自然表达以下需求：

- `name == "A" || name == "B"`
- `not(usingStrings contains "debug")`
- “同一个 using string 同时满足 `startWith("abc")` 和 `endWith("xyz")`”
- 不引入全新顶层 DSL 的前提下，在单个 matcher 节点内部表达更复杂的局部逻辑

同时，DexKit 又应尽量避免：

- Java 回调
- 闭包序列化
- 为常见场景强制引入单独的顶层 `where { ... }`
- 让现有原子查询路径产生明显性能回退

## 2. 目标

### 2.1 核心目标

1. 保留现有 `matcher { ... }` 作为主入口 DSL。
2. 在 matcher 节点内部增加最小化逻辑组合能力。
3. 同时兼容 Kotlin 与 Java API。
4. 保持现有 atomic 查询语义不变。
5. 尽量让旧查询的性能回退接近于零。
6. 不仅支持 method/class/field 顶层组合，也支持元素级 matcher 的组合。

### 2.2 非目标

1. 不支持 Kotlin 原生操作符语法，如 `==`、`&&`、`||`。
2. 不序列化 Kotlin/Java lambda 为可执行 predicate。
3. 第一阶段不重做所有集合 matcher。
4. 不要求常见场景必须切到顶层 `where { ... }` 才能表达。

## 3. 设计原则

### 3.1 `matcher { ... }` 仍然是隐式 `AND` 根节点

现有心智模型必须继续成立：

```kotlin
findMethod {
    matcher {
        name = "A"
        returnType = "int"
    }
}
```

仍然表示：

```text
name == "A" AND returnType == "int"
```

### 3.2 只在 matcher 节点内部增加局部逻辑组

不把常见场景强制改写为一个全新的顶层表达式 DSL，而是在 matcher 内增加：

- `allOf { ... }`
- `anyOf { ... }`
- `not { ... }`

例如：

```kotlin
findMethod {
    matcher {
        returnType = "int"

        anyOf {
            match { name = "A" }
            match { name = "B" }
        }

        not {
            usingStrings("debug", StringMatchType.Contains)
        }
    }
}
```

含义是：

```text
returnType == "int"
AND (name == "A" OR name == "B")
AND NOT(usingStrings contains "debug")
```

### 3.3 优先支持通用组合，不为每个字段做特化语法

第一阶段不建议引入：

- `nameAnyOf`
- `classNameAnyOf`
- `returnTypeAnyOf`

之类的字段特化接口。

原因：

- 通用逻辑组表达能力更强
- API 面更小
- 不需要为每个字段重复设计一套变体

### 3.4 元素级 matcher 也可能需要逻辑组合

逻辑组合不应只出现在 `MethodMatcher` / `ClassMatcher` / `FieldMatcher` 上。

例如：

> 同一个 using string 同时满足 `startWith("abc")` 和 `endWith("xyz")`

这个需求不是 method 级条件，而是单个 `StringMatcher` 的组合条件。

因此，设计应支持“描述单个对象的 matcher”具备局部逻辑组合能力，而不只是根级 method/class/field matcher。

## 4. 按 matcher 类型划分的建议

### 4.1 第一批建议支持局部逻辑组的 matcher

第一阶段推荐范围：

- `MethodMatcher`
- `ClassMatcher`
- `FieldMatcher`
- `StringMatcher`

第二阶段候选：

- `ParameterMatcher`
- `AnnotationMatcher`
- `AnnotationElementMatcher`
- `UsingFieldMatcher`

如后续确有需求，再考虑：

- `NumberEncodeValueMatcher`
- `AnnotationEncodeValueMatcher`

### 4.2 第一阶段不建议直接加局部逻辑组的 matcher

以下集合型 matcher 建议继续保持现有职责：

- `MethodsMatcher`
- `FieldsMatcher`
- `InterfacesMatcher`
- `ParametersMatcher`
- `AnnotationsMatcher`
- `AnnotationElementsMatcher`
- `AnnotationEncodeArrayMatcher`

这些类型本身承担的是集合语义，例如：

- contains
- equal
- count range

更合理的做法是：

- 让它们的子元素 matcher 变强
- 而不是第一阶段把集合 matcher 本身也改造成完整布尔表达式树

## 5. DSL 草案

### 5.1 Kotlin

#### 局部 `OR`

```kotlin
findMethod {
    matcher {
        anyOf {
            match { name = "A" }
            match { name = "B" }
        }
    }
}
```

#### 局部 `NOT`

```kotlin
findMethod {
    matcher {
        not {
            usingStrings("debug", StringMatchType.Contains)
        }
    }
}
```

#### 同一个 using string 需同时满足多个条件

```kotlin
findMethod {
    matcher {
        usingStrings {
            add {
                allOf {
                    match("abc", StringMatchType.StartWith)
                    match("xyz", StringMatchType.EndWith)
                }
            }
        }
    }
}
```

### 5.2 Java

Java 侧更适合显式 builder 风格：

```java
var matcher = MethodMatcher.create()
        .returnType("int")
        .anyOf(
                MethodMatcher.create().name("A"),
                MethodMatcher.create().name("B")
        )
        .not(
                MethodMatcher.create()
                        .usingStrings(List.of("debug"), StringMatchType.Contains, false)
        );
```

`StringMatcher` 示例：

```java
var stringMatcher = StringMatcher.create()
        .allOf(
                StringMatcher.create("abc", StringMatchType.StartWith, false),
                StringMatcher.create("xyz", StringMatchType.EndWith, false)
        );
```

## 6. 语义定义

### 6.1 节点级语义

每个支持 composite 的 matcher 节点按如下规则求值：

```text
match(node) =
    match_atom(node)
    AND match_all_of(node)
    AND match_any_of(node)
    AND match_none_of(node)
```

其中：

- `match_atom(node)`：当前节点原有普通字段条件
- `match_all_of(node)`：`all_of` 中全部子节点都匹配
- `match_any_of(node)`：`any_of` 为空时为 `true`，否则至少一个子节点匹配
- `match_none_of(node)`：`none_of` 中没有任何子节点匹配

等价写法：

```text
match(node) =
    atom(node)
    && all(child in all_of)
    && (any_of.empty || any(child in any_of))
    && none(child in none_of)
```

### 6.2 `not { ... }` 作为 DSL 语法糖

公开 DSL 可以暴露：

- `not { ... }`

内部序列化时可统一落到：

- `none_of` 且只有一个子节点

这样 schema 更统一，也便于未来扩展 `noneOf { ... }`。

### 6.3 集合语义保持不变

像 `MethodsMatcher`、`FieldsMatcher`、`usingStrings` 这类集合匹配器，第一阶段仍保持当前语义。

例如：

```kotlin
usingStrings {
    add("abc", StringMatchType.StartWith)
    add("xyz", StringMatchType.EndWith)
}
```

表示：

```text
存在一个 used string 以 "abc" 开头
AND
存在一个 used string 以 "xyz" 结尾
```

这两个条件可以由不同字符串分别满足，这与当前 DexKit 行为一致。

而：

```kotlin
usingStrings {
    add {
        allOf {
            match("abc", StringMatchType.StartWith)
            match("xyz", StringMatchType.EndWith)
        }
    }
}
```

表示：

```text
存在同一个 used string，同时满足两个子条件
```

这正是元素级组合必须存在的主要原因之一。

## 7. Schema 设计建议

### 7.1 选型方向

第一阶段建议在现有“单对象 matcher 表”上做增量扩展，而不是另起一套完全独立的表达式树根节点。

例如：

```fbs
table MethodMatcher {
    method_name: StringMatcher;
    access_flags: AccessFlagsMatcher;
    declaring_class: ClassMatcher;
    return_type: ClassMatcher;
    parameters: ParametersMatcher;
    annotations: AnnotationsMatcher;
    op_codes: OpCodesMatcher;
    using_strings: [StringMatcher];
    using_fields: [UsingFieldMatcher];
    using_numbers: [Number];
    invoking_methods: MethodsMatcher;
    method_callers: MethodsMatcher;
    proto_shorty: string;

    all_of: [MethodMatcher];
    any_of: [MethodMatcher];
    none_of: [MethodMatcher];
}
```

`ClassMatcher`、`FieldMatcher`、`StringMatcher` 也做类似扩展，例如：

```fbs
table StringMatcher {
    value: string;
    match_type: StringMatchType;
    ignore_case: bool;

    all_of: [StringMatcher];
    any_of: [StringMatcher];
    none_of: [StringMatcher];
}
```

### 7.2 这样设计的原因

优点：

1. 旧调用方天然兼容。
2. `matcher { ... }` 可以在现有风格上平滑扩展。
3. “普通字段 + 局部逻辑组”的模型与当前代码结构更接近。
4. 常见场景不需要强制切到顶层 `where`。

代价：

- 内部本质上仍然会演化成一棵表达式树，只是外部形态保持更贴近当前 matcher 类型。

### 7.3 第一阶段不优先采用的方案

另一种方案是引入单独的表达式树表，例如 `MethodMatchExpr`。

它的优点是结构更显式，但第一阶段不优先采用，原因是：

- API 改动更大
- 与当前 `matcher { ... }` 迁移距离更远
- Kotlin/Java 在常见场景下会更啰嗦

如果后续发现增量扩展方案变得过于别扭，再重新评估也不迟。

## 8. 执行模型与性能控制

### 8.1 核心要求

现有 atomic 查询不应因为支持 composite matcher 而承担明显的热路径开销。

### 8.2 atomic / composite 双路径

执行层建议拆成两条路径：

- atomic path：matcher 树中不存在 `all_of` / `any_of` / `none_of`
- composite path：matcher 树中至少存在一个逻辑组

入口只做一次轻量分派：

```text
if matcher is atomic:
    use current hot path
else:
    use composite evaluator
```

这样可以避免在现有所有热路径内部塞满额外判断。

### 8.3 atomic path

atomic path 尽量保持现有实现：

- 保持当前 `IsMethodMatched` / `IsClassMatched` / `IsFieldMatched` 逻辑
- 保持当前短路顺序
- 保持当前 cache 与 fast path 行为

### 8.4 composite path

composite path 的执行顺序建议为：

1. 先执行当前节点的 atomic 条件
2. 再递归判断 `all_of`
3. 再递归判断 `any_of`
4. 再递归判断 `none_of`
5. 任意一步可确定结果时立即短路

### 8.5 preprocess 归一化

进入执行阶段前可先做轻量归一化：

- 删除空的 `all_of` / `any_of` / `none_of`
- 单子节点的 `all_of` / `any_of` 在安全前提下折叠
- 展平嵌套的 `all_of(all_of(...))`
- 展平嵌套的 `any_of(any_of(...))`
- 预先缓存 `has_composite` 等廉价属性

这样运行时树更干净，递归层次也更浅。

### 8.6 第一阶段对 planner 保守处理

第一阶段对 composite query 的优化策略建议保守：

- atomic query 保持现有优化能力
- composite query 在有歧义时宁可保守退化，也不做不安全剪枝

这主要涉及：

- `Analyze(...)`
- `declare_class` 剪枝
- declared class fast path

因为：

- `AND` 场景往往仍能保留部分合取式优化
- `OR` / `NOT` 会破坏很多当前默认成立的推导前提

第一阶段优先级应为：

1. 语义正确
2. 旧查询不明显退化
3. 新查询性能可接受

而不是：

1. 第一版就把 composite planner 做到最强

### 8.7 当前已知瓶颈：atomic 与 composite 的执行形态不同

当前实现里，`usingStrings` 的 atomic query 与 composite query 并不走同一条热路径。

#### atomic `usingStrings`

以 `findMethod { matcher { usingStrings("A") } }` 为例，当前链路大致为：

1. `Analyze(...)` 标记 `kUsingString`
2. `EnterQueryExecution(kUsingString)` 保证 `method_using_string_ids` 已预热
3. `DexItem::FindMethod(...)` 逐 method 扫描
4. `DexItem::IsMethodMatched(...)` 进入 `IsMethodUsingStringsMatched(...)`
5. 若 `StringMatcher` 全为原子条件，则走 AC trie 快路径

此时一个 method 只需要执行一次 `usingStrings` 判断。

#### composite `usingStrings`

以：

```kotlin
anyOf {
    match { usingStrings("A") }
    match { usingStrings("B") }
}
```

为例，当前执行模型是“**按 matcher 节点递归求值**”，而不是“先把所有字符串条件合并后统一处理”。

对单个 method，当前更接近：

```text
先判断 child-1 的 usingStrings(A)
若失败，再判断 child-2 的 usingStrings(B)
```

因此，同一个 method 的 used strings 可能被重复扫描多次。
这正是当前 composite 在字符串场景下明显慢于 atomic 的主要原因。

### 8.8 `findFirst` 与 batch 大小的经验结论

当前 `findFirst` 的收益主要来自：

- worker 侧早停
- 调度优先级提升

但它**不会**消除命中前那一段已经发生的 composite evaluator 成本，因此：

- 对“第一条很快命中”的 atomic 查询，`findFirst` 也很难追平
- 对“第一条大概率 miss，整体接近全量扫描”的场景，composite 更容易接近 atomic fallback 的总耗时

另外，本地实验已经表明：**盲目降低 batch size 并不是有效优化**。
batch 过小会带来：

- 任务切分变多
- future / 调度开销上升
- 顺序扫描局部性变差

因此，当前不建议把“减小 batch”作为 composite 性能优化的主方向。

### 8.9 字符串类优化：适合使用 AC trie 做预过滤

对于 composite 树中的**正向字符串叶子条件**，可以考虑提取后合并为一次 AC trie 扫描。

例如：

```kotlin
anyOf {
    match { usingStrings("A") }
    match { usingStrings("B") }
}
```

可抽象为：

```text
method 至少命中 A / B 中的一个关键词
```

建议的优化方式不是“让 AC trie 直接承担整棵布尔树求值”，而是：

1. 从 composite 树中提取可安全合并的**正向原子字符串叶子**
2. 构建 union AC trie
3. 对一个 method 的 used strings 只扫描一次
4. 先做廉价预过滤
5. 仅对候选 method 再进入完整 composite evaluator

这样可以在不破坏语义的前提下，避免：

- 每个 `match { usingStrings(...) }` 都单独扫描一遍 method 的 used strings

#### 8.9.1 优先适用范围

第一批最值得做的范围：

- `MethodMatcher.usingStrings`
- `ClassMatcher.usingStrings`
- 仅限**正向**、**原子**、**可归一化**的字符串条件

例如：

- `Contains`
- `StartWith`
- `EndWith`
- `Equal`
- 可安全降级为上述关系的 `SimilarRegex`

#### 8.9.2 需要明确区分两种语义

**方法级 / 类级 usingStrings**

```kotlin
allOf {
    match { usingStrings("A") }
    match { usingStrings("B") }
}
```

其语义通常是：

```text
存在某个 used string 命中 A
AND
存在某个 used string 命中 B
```

这里 A 和 B 可以由**不同字符串**满足，因此很适合先做 method 级命中集合预过滤。

**元素级 StringMatcher**

```kotlin
usingStrings {
    add {
        allOf {
            match("abc", StringMatchType.StartWith)
            match("xyz", StringMatchType.EndWith)
        }
    }
}
```

其语义是：

```text
存在同一个 used string，同时满足多个子条件
```

这类场景不能只做 method 级关键词集合判断；若后续要优化，必须记录**每个字符串**命中了哪些叶子条件。
因此它适合作为后续阶段，而不是第一批性能优化目标。

### 8.10 非字符串类优化：不能依赖 AC trie

AC trie 只能优化字符串类叶子，不能直接处理：

- `accessFlags`
- `declaredClass`
- `returnType`
- `invokes`
- `usingFields`
- `annotations`
- `parameters`
- `opCodes`
- 数字类 matcher

但这不意味着非字符串条件无法优化。更适合的方向包括：

#### 8.10.1 query-local tri-state memo

适合多对一关系边，例如：

- `declaredClass`
- `returnType`
- `typeClass`

思路是缓存：

```text
(matcher node, entity id) -> true / false
```

从而避免同一个子树被大量外层实体重复求值。

#### 8.10.2 候选集 / 安全正向子树提取

对于可安全索引的正向精确条件，可考虑提取为：

- candidate class set
- candidate method set
- candidate type set

再在运行时先做廉价 membership check，最后回到原 evaluator 做精确判定。

#### 8.10.3 planner 继续保守

在 `OR` / `NOT` 参与时，很多当前 atomic query 中成立的推导都不再安全。
因此即使后续增强 planner，也应优先提取：

- 正向
- 精确
- 可证明安全

的子树，而不是试图一次性把整棵 composite 树静态编译成最优执行计划。

### 8.11 当前建议的优化优先级

若后续要继续推进 composite 性能，建议优先级如下：

1. **字符串类正向预过滤**
   - 先覆盖 `MethodMatcher` / `ClassMatcher` 的 `usingStrings`
   - 收益最大，风险最低
2. **多对一关系边的 query-local memo**
   - 优先 `declaredClass` / `returnType` / `typeClass`
3. **从 composite 树中提取安全的正向精确子树**
   - 作为 planner 的增量增强
4. **元素级 StringMatcher 的同一元素多条件优化**
   - 复杂度高，放后续阶段验证

换句话说，较合理的总体策略应当是：

> **字符串叶子走 AC trie 预过滤，跨关系边子树走 memo，可安全索引的正向子树走候选集，其余复杂条件继续保留递归精确求值。**

## 9. Analyze 与后续优化影响

### 9.1 `need_flags`

对 composite 树来说：

- `AND`：并集安全
- `OR`：做保守并集通常也安全
- `NOT`：为了保守预热，做并集通常也安全

因此 `need_flags` 第一阶段仍可采用保守并集思路。

### 9.2 `declare_class`

`declare_class` 要更谨慎。

例如：

```text
declaredClass == X OR declaredClass == Y
```

绝不能被解释成“当前 dex 必须同时声明 X 和 Y”。

因此第一阶段建议：

- 只有在明显纯合取场景下才继续传播 `declare_class`
- 遇到 `OR` / `NOT` 等复杂组合时，必要时直接放弃这类剪枝

### 9.3 未来优化方向

语义落地后，可考虑的后续优化包括：

- composite-aware `Analyze(...)`
- 从 composite 树中提取可安全索引的正向精确子树
- query-local compiled matcher 表示
- composite 子树重复求值 memo

## 10. 落地阶段建议

### 10.1 Phase 1

先支持：

- `MethodMatcher`
- `ClassMatcher`
- `FieldMatcher`
- `StringMatcher`

增加：

- `allOf`
- `anyOf`
- `not`

native 部分：

- atomic/composite 双路径
- 基础归一化
- composite 查询走保守 planner

### 10.2 Phase 2

再扩展到：

- `ParameterMatcher`
- `AnnotationMatcher`
- `AnnotationElementMatcher`
- `UsingFieldMatcher`

### 10.3 Phase 3

只有在明确存在需求时，再考虑：

- composite-aware planner 强化
- 更多 matcher 类型
- 公开 `noneOf`
- 对极复杂表达式提供可选顶层 `where`

## 11. 测试建议

建议补充以下测试：

1. 旧 atomic matcher 查询行为不变
2. root matcher 上的 `anyOf`
3. root matcher 上的 `not`
4. `ClassMatcher` / `MethodMatcher` 内部嵌套逻辑组
5. `StringMatcher` 的 composite 语义，尤其是“同一元素同时满足多个条件”
6. 明确区分：
   - 两个独立 `usingStrings` 条目
   - 一个 composite `StringMatcher` 条目
7. Java API 调用
8. matcher 序列化变化后，`equals` / `hashCode` / `hashKey` 是否仍符合预期

## 12. 结论

推荐方向如下：

1. 保留 `matcher { ... }` 作为主 DSL
2. 在“单对象 matcher”节点内部增加最小化逻辑组
3. 第一阶段不重做集合 matcher
4. 让 `StringMatcher` 这类元素 matcher 也具备组合能力，以表达“同一元素多条件”
5. 通过 atomic/composite 双路径尽量保持旧查询性能

这条路线能够在不破坏现有 DexKit 使用习惯的前提下，为绝大多数实用复杂查询提供表达能力。
