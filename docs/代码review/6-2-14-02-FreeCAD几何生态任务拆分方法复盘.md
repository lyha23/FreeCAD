# FreeCAD 几何生态任务拆分方法复盘

时间：2026-06-02 14:02。

## 结论

`5-27-14-10-PartDesignBody生态迁移方案.md` 原来承载的是一个过大的生态级任务：Body、FeatureBase、PartDesign executor、Part shape acquisition、App::Link、Attachment、topo naming/history 和 fixture/oracle 全部压在同一篇方案里。

这类任务不能按“继续补某个 feature”推进。正确拆法是先承认它不是一个 Body 任务，而是 `PartDesign + Part + App + Attachment + topo naming/history` 的组合生态迁移，然后把它拆成几条可验收的责任层。现在 `docs/几何支持/FreeCAD几何生态迁移工程-细分/` 的 M0-M6 就是这个思路：每个 milestone 都有 FreeCAD 依据、Rust 落点、必收切片、边界、非目标和验收入口。

这种拆分让实现可验证的关键，不是文档变多，而是每个 milestone 都回答了一个清楚的问题：这一层到底负责什么，失败时不应该甩到哪一层，验收时看什么证据。

## 原大方案的问题

原入口文档的问题不是信息不足，而是语义混在一起：

- `Body::execute()` 的 Tip 输出和 previous solid 推导，属于 PartDesign 主链。
- `Part::Feature::_getTopoShape()`、`ResolveLink | Transform | NeedSubElement`，属于 Part shape acquisition。
- `App::Link`、Group、App::Part、Body child subobject，属于 App/Part 子对象路由。
- `AttachmentSupport`、Origin/LCS、datum placement，属于 placement 计算。
- `NamedShape`、ElementMap、MapperHistory，属于命名和历史传播。
- fixture/oracle 失败归因，属于验收流程，不属于任意一个 executor。

如果这些内容继续放在一个任务里，最容易出现的问题是：某个 fixture 失败后，为了让当前 feature 变绿，在 Pad/Pocket/Hole/DressUp/Transformed executor 里补几何猜测、Link 特判、placement 特判或 response 重命名。这样短期看起来推进了，实际是在把底层缺口扩散到上层。

## 当前拆分轴线

M0 是横向闸门，不是普通功能任务。它先处理 fallback：非 FreeCAD 语义的 fallback 删除或替换；FreeCAD 明确 policy 才能保留；临时绑定容错必须隔离并写清删除条件。这个闸门防止旧兼容路径污染后续 milestone。

M1 只收 PartDesign 主链：Body、FeatureBase、FeatureAddSub、Suppressed、AllowCompound、previous solid、Tip 输出和主要 feature family 的执行边界。M1 的价值是把“Body 负责什么”收住，同时明确 `_getTopoShape()`、Attachment、ElementMap/history 都不是 M1 的职责。

M2 收 Part shape acquisition：把 `Part::Feature::getTopoShape()` / `_getTopoShape()` 高频语义落到 Part 层，让 Profile、UpTo、FeatureBase、Part Extrusion/Revolution 等调用侧复用同一入口。它避免每个 feature executor 各写一套取形规则。

M3 收 `App::Link` 与 Group 子对象：把 LinkTransform、nested link、ElementList、SubList、linked plain group、request-local `_ChildCache`、App::Part placement-aware child path 和 Body child subobject 依赖补齐到 App/Part 层。它的边界是提供 shape、owner、`pmat` 输入，不负责最终 ElementMap/history。

M4 收 Attachment / Origin / LCS：让 Profile、UpTo、MirrorPlane、Pattern axis、Transformed support-local placement 消费同一套 attachment-aware placement。它解决的是 support 解析和 placement，不解决 shape acquisition 或命名历史。

M5 收 topo naming / history：把 M2/M3/M4 产生的 shape、owner、placement、mapper/history 消费成 stable subname 和 ElementMap。它禁止在 response 层按 fixture 输出顺序修剪、重命名或猜 source ownership。

M6 收 fixture / oracle 验收推进：它不直接实现功能，而是固定失败归因顺序、fixture 增量要求和验收表达。它保证新增用例是在验证语义，而不是继续把所有失败推给 executor。

## 可验证性的来源

每个 milestone 必须有独立的“能不能算完成”的判断：

- FreeCAD 依据：写清源文件、类/函数、关键字段或关键短句。
- Rust 落点：写清语义归属的模块和文件，避免把底层能力补到 executor。
- 必收切片：列出当前阶段必须覆盖的入口，不把整个 FreeCAD 完整 parity 当作一次性目标。
- 与其他 milestone 的边界：失败时能判断归 M2/M3/M4/M5，还是 feature 自身业务分支。
- 非目标：明确哪些东西现在不做，防止 scope 在实现时自动扩大。
- 验收：给出 fixture、语义单测或 `cargo test` 范围，验收结果能被复查。

这个结构让“已实现”不是主观状态，而是可以通过文档、测试和 diff 复核的状态。M1/M2/M3 已经重命名为 `【已实现】`，就是因为它们各自有清楚的验收边界；M4/M5/M6 仍保留为后续切片，说明剩余风险没有被隐藏。

## 失败归因规则

遇到 fixture 或 oracle 不一致时，先按层定位，而不是直接改 feature executor：

1. Oracle 是否可信，FreeCAD probe 是否真实执行。
2. Shape acquisition / subobject / placement 是否拿到了正确 raw shape。
3. Owner、`pmat`、ElementMap/history 是否正确。
4. Feature executor 的 FreeCAD 业务分支是否缺失。
5. 是否只是 stable subname 命名顺序差异。

这个顺序很重要。它把底层 `_getTopoShape()`、App::Link、Attachment、MapperHistory、ElementMap 缺口拦在正确层级，避免上层 executor 继续叠 fixture 形态规则。

## 以后同类方案的拆法

以后遇到“一个方案变成生态级迁移”的情况，不要直接继续往原文档里堆任务。先做四步：

1. 把入口文档降级为总入口：保留目标、FreeCAD 依据、当前判断和文档索引。
2. 按责任层拆 milestone：主执行链、输入取形、文档/Link 路由、placement、命名历史、fixture/oracle、fallback 闸门。
3. 每个 milestone 固定同一套模板：FreeCAD 依据、Rust 落点、必收切片、跨层边界、验收、非目标。
4. 已完成的 milestone 用验收结果关闭；未完成的 milestone 保留风险和下一步，不把过程流水账写进总路线图。

这个方法的核心是把“大方案”拆成可验证的语义边界，而不是拆成更小的待办清单。待办清单只能说明还有什么没做；milestone 边界能说明为什么应该在这一层做，以及做完后如何证明没有把问题补错地方。
