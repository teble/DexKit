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
