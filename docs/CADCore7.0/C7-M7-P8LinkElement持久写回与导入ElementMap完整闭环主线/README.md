# C7-M7 P8 LinkElement 持久写回与导入 ElementMap 完整闭环主线

本目录承接 C7-M6 release gate 之后的 P8 引用闭环方向。C7-M6 已确认 Assembly Joint 非 identity marker chain 为 expected-backed closed，zero Angle fallback class evidence 与 bundled `offsetPlc` lifecycle 继续 `oracle_blocked`，没有 `backend_gap_requires_implementation`。C7-M7 不继续扩 Assembly Joint，而是转向 P8 中仍未迁移的 Link / ElementMap / stable reference 生命周期。

C7-M7 的目标是围绕导入 shape 完整 `ElementMap`、`ShowElement=true` 的 `LinkElement` / `LinkGroup` 持久写回事务、cross-document 文档哈希 / postfix 生命周期和复杂多层 `LinkSub` 链，先以 FreeCAD source 和 checked-in native expected 证明候选范围，再决定是否打开 `cad-core` implementation gate。S4 已关闭 implementation gate，S5 已按 no-code publication closure 发布 blocker / non-goal 口径。

## 入口

- 主线总入口：`6-26-14-20-C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线总入口.md`
- 方案：`6-26-14-20-C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环方案.md`
- 工作步骤总入口索引：`工作步骤细分/6-26-14-20-【已实现】C7-M7工作步骤总入口.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- 创建基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=7be2d4e937`（`7be2d4e937 docs: 完成 C7-M6 S6 发布闸门`），创建前 `git status --short -uall` 无输出。
- C7-M1 到 C7-M6 队列均为空；C7-M6 final route 为 `expected-backed / oracle_blocked / no backendGap`。
- P8 live 文档仍明确未迁移：导入 shape 完整 `ElementMap`、完整 FreeCAD Link 账本、`ShowElement=true` LinkElement / LinkGroup 持久写回事务生命周期、完整 cross-document 文档哈希生命周期和更复杂 LinkSub 链。
- 当前 Link 能力只覆盖请求内 display、拾取、alias retag、terminal / merge history 传播和前端图更新建议；`documentObjectUpdates` 是前端更新 graph 的建议，不是后端持久化状态。
- C7-M7 初始队列为 S0-S6 pending；S0/S1 只允许文档和矩阵，S2 形成 oracle 候选批次，S3 才允许采 native oracle 或 blocker，S4 裁决 implementation gate，S5 只有在 S4 打开 `backend_gap_requires_implementation` 时才改 C++。
- S0 live 基线已冻结：执行时 `pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=7be2d4e937`（`7be2d4e937 docs: 完成 C7-M6 S6 发布闸门`），开始状态只包含 `docs/CADCore7.0/README.md` modified 和本 C7-M7 文档包 untracked 文件；C7-M1 到 C7-M6 队列均为空，C7-M7 初始队列从 S0 开始。
- S0 P8 边界已冻结：already-covered 包括基础 Link / LinkSub / LinkGroup / LinkElement display、ElementList / ElementCount / ShowElement 请求内 `documentObjectUpdates` 建议、FullSubList / mapped postfix / cross-document alias 更新、Link retag terminal / merge history、plain group 展开、BREP / STEP / IGES import `history_partial` ElementMap；remaining 包括完整导入 ElementMap、STL import `indexed_only` 后续、完整 FreeCAD Link 持久账本、ShowElement 持久写回事务、复杂多层 LinkSub 生命周期和 cross-document hash / postfix 生命周期；diagnostic / non-goal 包括 GUI / ViewProvider / Workbench、前端同步协议、跨请求 backend cache / persistent BREP、Worker / WASM / Web 产品化和 Assembly Joint blocked rows。
- S1 source / coverage 复核已完成：执行时 `HEAD=7d014c588e`（`7d014c588e 文档：完成 C7-M7 S0 基线冻结`），开始状态干净。FreeCAD source authority 已定位到 `LinkBaseExtension::update()`、`DocumentObject::getSubObject()`、`PropertyXLink*`、Part import、`PropertyPartShape` 和 `TopoShapeMapper`；current `cad-core` 覆盖已定位到 `cad-core/src/app`、`cad-core/src/part`、`cad-core/src/runtime`、`cad-core/src/mesh`、`tests/test_p8_features.py` 和 `fixtures/p8`，不是旧路径 `cad-core/src/features` / `cad-core/src/document` / `cad-core/src/topo`。
- S1 已关闭 `C7M7-BLOCKER-101` / `C7M7-GATE-101`，没有采 oracle、没有新增 fixture/expected/test、没有改 C++；交给 S2 的输入池是完整 imported-shape `ElementMap`、ShowElement LinkElement / LinkGroup persistent writeback transaction、复杂多层 LinkSub / cross-document hash-postfix lifecycle。
- S2 oracle 候选矩阵已完成：执行时 `HEAD=d6f62daad5`（`d6f62daad5 文档：完成 C7-M7 S1 源码覆盖复核`），开始状态干净。S2 将既有 P8 Link display / alias / `FullSubList` / mapped postfix / imported Link chain / ShowElement request-local `documentObjectUpdates` 锁定为 `already_covered`，把完整 BREP / STEP / IGES imported-shape `ElementMap`、ShowElement `LinkElement` / `LinkGroup` persistent writeback transaction、复杂多层 `LinkSub` / cross-document hash-postfix save/restore lifecycle 裁为 `oracle_candidate`，把 STL complete Part ElementMap 裁为 `oracle_blocker`，把 GUI / frontend sync / Worker / cross-request backend state 裁为 `diagnostic_non_goal`。
- S2 已关闭 `C7M7-BLOCKER-201`，并关闭 S2 下的分类 gate；`C7M7-GATE-601` 仍只是 `backend_gap_candidate` 待 S4 裁决，没有打开 S5 implementation gate。S2 未采 native oracle，未新增或修改 fixture/expected/test，未改 C++；下一队列项是 S3 native oracle 采集与 expected 固化。
- S3 native oracle 采集已完成：执行时 `HEAD=7e7a99627e`（`7e7a99627e 文档：完成 C7-M7 S2 oracle 候选矩阵`），开始状态干净。ORACLE-202 的 BREP / STEP / IGES / imported-link-chain collector payload 只有 shape summary，没有完整 `ElementMap` / reference-update evidence；ORACLE-302 的 ShowElement collector 要么因 `ElementList` 只读失败，要么只返回 shape / `object_fields`，没有持久事务字段；ORACLE-402 的 collector/save-restore probe 不能观察 file/stamp/hash、DocMap、restored `FullSubList`、ReferenceShadow 或 mapped postfix lifecycle，`multilevel-label` native shape 仍 broken。STL ORACLE-203 保持 mesh-specific `oracle_blocker`。S3 未新增或修改 fixture/expected/test，未改 C++；下一队列项是 S4 parity 与 implementation gate 裁决。
- S4 cad-core parity / implementation gate 已完成：执行时 `HEAD=24b7649fa5`（`24b7649fa5 文档：完成 C7-M7 S3 native oracle 采集`），开始状态干净。由于 S3 没有新增 checked-in native expected，S4 不做 runtime parity 实现裁决；ORACLE-202 / 302 / 402 均发布为 `oracle_blocked`，ORACLE-203 继续 STL `oracle_blocker`。`C7M7-BLOCKER-401` / `C7M7-GATE-601` 已关闭为 no-code gate，S5 只能做 README / 方案 / 总入口 / 工作步骤 / 矩阵发布收口，不允许改 C++、fixtures、expected、tests、collector 或生成输出。
- S5 no-code 发布收口已完成：执行时 `HEAD=5080d31d76`（`5080d31d76 文档：完成 C7-M7 S4 准入裁决`），开始状态干净。already-covered rows 继续关闭；完整 imported ElementMap、ShowElement persistent writeback、complex LinkSub hash / postfix lifecycle 发布为 `oracle_blocked`；STL Part-style ElementMap 发布为 `oracle_blocker`；GUI / frontend / cache / Worker 保持 `diagnostic_non_goal`。S5 未改 C++、fixtures、expected、tests、collector、capability 或生成输出；`C7M7-BLOCKER-501` 已关闭，`C7M7-GATE-601` 发布为 no-code closure，S6 只做 release gate。

## 收口边界

- 先证明 FreeCAD source authority 和 native oracle，再比较 current `cad-core`；不得从 current `cad-core` 输出倒推 expected。
- 只处理 P8 Link / imported-shape stable reference 闭环：导入 shape ElementMap、LinkElement / LinkGroup 持久写回、cross-document hash / postfix、复杂 LinkSub 解析和 `elementReferenceUpdates` / `documentObjectUpdates` 语义。
- 不处理 GUI / ViewProvider / Workbench、前端状态同步协议、跨请求后端缓存、BREP 持久状态、Worker / WASM / Web 产品化或 Assembly Joint blocked rows。
- 如果 FreeCAD native lifecycle 不可采、source authority 不足或 current `cad-core` 已匹配，必须发布为 `oracle_blocked`、`diagnostic_non_goal` 或 `already_closed_expected_backed`，不打开 C++ 实现。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线 docs/CADCore7.0/README.md
git diff --check
```
