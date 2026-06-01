# WireJoiner 自作聪明 fallback 实现反思

时间：2026-05-25 07:46。

## 结论

之前“先导出完整 fallback open wires，再按 source closed loop / tight-bound 语义删除已消费 fragment”的实现，本质上是用结果形态去补一个内部账本缺口。

它短期能让 fixture 变绿，但方向是错的：FreeCAD 的语义不是“导出后再猜哪些 fragment 应该删”，而是 `WireJoinerP::build()` 在同一份 `EdgeInfo` list 上持续改写 `wireInfo`、`wireInfo2`、`iteration`、`iteration2`，最后直接按 final `EdgeInfo` 状态导出。

正确方向已经在当前实现里体现：

```text
split fragments
  -> stable FinalEdgeId
  -> EdgeRuntimeState / final ownership snapshot
  -> findClosedWires / findTightBound / exhaustTightBound 写同一份 ownership
  -> open export 只看 final EdgeInfo 条件
```

这个流程比 fallback 清楚，是因为它把 FreeCAD 的状态机搬过来了，而不是在输出端猜 FreeCAD 的意图。

## 当时怎么走偏的

### 1. 从 fixture 差异出发，而不是从 FreeCAD 状态机出发

当时看到的是 open fragment extra / missing，于是自然地把问题理解成“哪些 open edge 应该导出”。这个问题表面上在导出层，实际根因在更早的 WireJoiner ownership 层。

正确问题应该是：

```text
这条 final EdgeInfo 在 FreeCAD 里最后的 wireInfo / wireInfo2 / iteration 是什么？
Rust 有没有等价账本能表达这个状态？
```

错误问题是：

```text
这个 source fragment 看起来被 closed loop 或 tight-bound 消费了吗？
```

一旦问成第二个问题，就会走向 source closed loop、chord、duplicate-chain、endpoint network 这些结果形态判断。

### 2. 把“临时诊断手段”升级成了主路径

fallback open wires 最初可以作为诊断工具：先吐全量 open set，观察哪些 fragment 被多吐了。

问题在于后续没有及时停下，把它降级回 probe，而是继续在这个结果集上加 pruning 规则。这导致实现越来越像：

```text
观察一个 case
  -> 加一个几何形态过滤
  -> 再观察下一个 case
  -> 再加一个例外
```

这和仓库要求的 FreeCAD parity 不一致。parity 不是把当前 fixture 输出凑齐，而是迁移 FreeCAD 做出这个输出的内部语义。

### 3. 把“几何等价”误当成“语义等价”

source closed loop / tight-bound pruning 可能在某些 case 上导出同样的边集合，但它没有保留这些信息：

- 哪个 `EdgeInfo` 是 live list 节点；
- `superEdge` 继承的是哪个原始 `EdgeInfo` 身份；
- `wireInfo` 和 `wireInfo2` 是 live pointer 还是 per-fragment mapper；
- `iteration == -3`、`iteration >= 0 && !wireInfo`、`iteration == -1` 分别意味着什么；
- `exhaustTightBoundWithAdjacent()` 在 candidate done 后为什么要 `ENSURE(edgeInfo->wireInfo != nullptr)`。

这些信息缺失时，即使输出边集合暂时相似，也不是可维护的 FreeCAD 迁移。

### 4. 没有在第一次出现“内部账本缺失”时强制补账本

当发现 Rust 没有 FreeCAD 等价的 `EdgeInfo` identity、`WireInfo` pointer、`wireInfo2` secondary ownership 时，正确动作应该是暂停输出端修补，先补内部状态机。

当时的错误动作是：

```text
账本不完整
  -> 先靠 graph/source/split 关系推断
  -> 用 pruning 修掉当前差异
```

正确动作应该是：

```text
账本不完整
  -> 回 FreeCAD 源码确认字段生命周期
  -> 补 FinalEdgeId / WireInfoId / iteration / ownership 写入点
  -> 再让 final ownership 驱动导出
```

### 5. 过度相信 fixture 变绿

fixture 通过只能说明当前样本没有暴露问题，不能证明实现语义正确。对于 WireJoiner 这种内部状态机，尤其不能只看最终 shape。

当实现包含下面这些信号时，即使 fixture 变绿，也应该判定为高风险：

- helper 名称描述的是 source 几何形态，而不是 FreeCAD 字段或函数；
- 判断条件依赖 closed-loop、chord、dangling endpoint、source index，而 FreeCAD 源码没有对应字段；
- 修复点在导出之后，而 FreeCAD 决策发生在 `buildClosedWire()` / `exhaustTightBound()` 中；
- 注释里只能解释“这个 case 为什么对”，不能解释“FreeCAD 为什么这样做”；
- 新规则不能自然说明 `NamedShape` / `ElementMap` / `MapperHistory(aHistory)` 应该如何消费。

## 正确实现为什么清楚

当前实现清楚，是因为每个判断都有 FreeCAD 落点：

```text
FinalEdgeId
  对应 FreeCAD std::list<EdgeInfo> 的节点身份。

EdgeRuntimeState.primary / secondary
  对应 EdgeInfo::wireInfo / EdgeInfo::wireInfo2。

iteration / iteration2
  对应 EdgeInfo::iteration / EdgeInfo::iteration2。

open export 条件
  对应 WireJoinerP::build() 里的 final edge 条件：
  iteration == -3 || (!wireInfo && iteration >= 0)

MissingPrimaryWireInfoDuringExhaust
  对应 WireJoinerP::exhaustTightBoundWithAdjacent()
  在 candidate wire done 后执行的 ENSURE(edgeInfo->wireInfo != nullptr)。
```

这类实现不需要猜“这条 fragment 看起来该不该删”。只要账本写对，导出条件就是 FreeCAD 的条件。

## 以后必须遵守的纠错规则

### 规则 1：遇到 FreeCAD 内部账本语义，禁止先做输出端 pruning

只要 FreeCAD 源码里存在这些结构或字段，就必须先迁移账本或明确写成短期 probe：

- `EdgeInfo`
- `WireInfo`
- `wireInfo`
- `wireInfo2`
- `iteration`
- `iteration2`
- `superEdge`
- `MapperHistory`
- `BRepTools_History`
- `myShapesToReturn`
- `myPreSplitHistory`
- `ElementMap`

不能用 source index、split index、几何类型、closed-loop 形态、endpoint degree 代替这些字段。

### 规则 2：fixture 只能验证语义，不能定义语义

实现前必须先写清：

```text
FreeCAD 文件：
FreeCAD 函数：
关键字段：
调用顺序：
Rust 落点：
验证方式：
```

如果只能写出“这个 fixture 下应该删除某些 fragment”，但写不出 FreeCAD 字段生命周期，就不能进入主路径实现。

### 规则 3：fallback 必须有删除条件，且不能继续叠规则

如果确实需要 fallback，只允许满足以下条件：

1. 相邻代码注释标明“临时 fallback”。
2. 文档写明适用边界和删除条件。
3. fallback 不得继续叠加第二个 fixture 形态规则。
4. 一旦发现需要第二个规则，必须停止，回到 FreeCAD 状态机。

这次问题正是第 3 点被突破了：从 full open fallback 走到 closed loop pruning，再走到 tight-bound consumed fragment，再走到 endpoint network failure heuristic。

### 规则 4：导出层只能消费 final ownership，不能重新推理 ownership

对于 open wires、internal edges、internal vertices、subname、history 映射，导出层只允许做这些事：

- 按 final ownership 遍历；
- 转换为输出结构；
- 保留 FreeCAD 顺序或稳定顺序；
- 附带 trace / diagnostic。

导出层不允许做这些事：

- 判断某条 source fragment 是否“看起来被消费”；
- 根据几何覆盖关系删除 fragment；
- 根据 fixture 名或几何组合补例外；
- 根据 split_count、endpoint degree、source loop 推断 FreeCAD ownership。

### 规则 5：如果实现开始变复杂，先检查是不是层错了

这次 fallback 变复杂的信号很明显：

```text
open export
  -> source closed loop
  -> chord fragment
  -> duplicate chain
  -> dangling endpoint
  -> result wire shape
```

这些概念都不是 FreeCAD open export 的核心字段。实现一旦开始引入这类横向概念，就应该怀疑自己在错误层补洞。

## 审查清单

之后 review 类似修改时，直接按这份清单拦截。

### A. FreeCAD 依据检查

- 是否标注了本地 FreeCAD 绝对路径。
- 是否标注了类、函数、字段名。
- 是否能说清调用顺序。
- 是否引用的是 FreeCAD 主路径，而不是 fixture 输出。
- 是否有 Rust 分层落点。

### B. 状态机检查

- FreeCAD 如果用 pointer / list node identity，Rust 是否有稳定 id。
- FreeCAD 如果改写字段，Rust 是否在同一阶段改写等价字段。
- FreeCAD 如果最后遍历 live list，Rust 是否也按 live order 遍历。
- FreeCAD 如果有异常边界，Rust 是否在同一语义位置失败。
- 是否把 mapper/history 状态和 live ownership 混为一谈。

### C. 反模式检查

出现以下内容，需要默认判定为可疑：

- `source_*_loop` 用来决定 final export。
- `dangling_*` 用来决定 FreeCAD failure。
- `chord_*` 用来删除 open fragment。
- `split_count` 用来猜 consumed fragment。
- `ellipse && bspline`、几何类型组合分支。
- fixture 名称或 case 形态进入实现。
- “先全量导出再删”成为主路径。

### D. 验证检查

- 验证不能只说 fixture 通过。
- 必须说明对应 FreeCAD 字段是否被表达。
- 必须说明 trace 是否能看到 final ownership。
- 必须说明 history / ElementMap 是否仍靠同一套 ownership。
- 如果只是命名顺序差异，要和几何/语义失败分开记录。

## 以后遇到同类问题的固定流程

1. 先定位 FreeCAD 调用链。
2. 列出 FreeCAD 的内部账本字段。
3. 判断 Rust 是否已有等价结构。
4. 没有等价结构时，先补结构，不改输出端。
5. 有等价结构后，把字段写入点迁到 FreeCAD 对应阶段。
6. 导出层只消费 final ownership。
7. 用 fixture 验证，但不从 fixture 反推逻辑。
8. 把新发现写回方案文档。

## 对这次错误的最终判断

之前的 fallback 不是完全没有价值：它帮助暴露了 open fragment 差异，也避免了在 `sketch.rs` 里继续加 fixture 特判。

但它停留太久，并且被扩大成主路径，这是错误。真正应该更早做的是承认 Rust 缺少 FreeCAD `EdgeInfo` 账本，然后补 `FinalEdgeId`、`WireInfoId`、`iteration`、`wireInfo/wireInfo2` lifecycle。

以后只要看到“输出端修修剪剪越来越多”，就应该把它视为流程告警：大概率不是还差一个过滤条件，而是缺少 FreeCAD 的内部状态机。
