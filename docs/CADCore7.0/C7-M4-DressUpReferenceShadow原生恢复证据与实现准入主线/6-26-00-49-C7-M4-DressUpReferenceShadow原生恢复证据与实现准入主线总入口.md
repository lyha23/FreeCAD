# C7-M4 DressUp ReferenceShadow 原生恢复证据与实现准入主线总入口

## 结论

C7-M4 是 C7-M3 后续的单一 blocker 处理包。C7-M3 已证明 Fillet / Chamfer 参数能力 expected-backed；本包只处理 stale `ReferenceShadow` / Base recovery 是否有 FreeCAD native 恢复证据，以及该证据是否足以打开 `cad-core` implementation gate。

当前默认结论是 gate closed：没有 native oracle 前，不实现 C++，不发布 supported，不做 output-side guessing。S3 才能把本包裁为 `already_closed_expected_backed`、`backend_gap_requires_implementation`、`oracle_blocked` 或 `diagnostic_non_goal`。

## 上游状态

- C7-M3 release gate 已完成，最终提交为 `edae0ef938 文档：完成 C7-M3 S5 发布闸门`。
- C7-M3 `C7M3-SCOPE-101` / `C7M3-SCOPE-102` 已 expected-backed；`C7M3-SCOPE-103` 保持 `oracle_blocked`。
- C7-M3 blocker expected：`cad-core/fixtures/c3m5/expected/dressup-reference-shadow-base-recovery.freecad.json`，`known_gap.kind=dressup_reference_shadow_base_recovery_native_oracle_blocked`。
- C7-M3 geometry-only probe 不能作为证据，因为当前 collector 会优先使用 `StableSubList`，绕过了 FreeCAD `ShadowSub` / `ReferenceShadow` 原生恢复。

## S0 基线

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=9bb2cd22af`（`9bb2cd22af docs: 收口 C7-M4 工作步骤总入口索引`），开始状态 `git status --short -uall` 无输出。
- 队列：C7-M1/C7-M2/C7-M3 均为空；C7-M4 从 S0 开始，S0 完成后推进到 S1。
- 继承 blocker：`C7M3-SCOPE-103` / `C7M3-GATE-103` / `C7M3-ORACLE-301` / `known_gap.kind=dressup_reference_shadow_base_recovery_native_oracle_blocked` 是 C7-M4 的唯一 active blocker 起点。
- S0 未采 oracle、未运行 FreeCADCmd、未改 C++、fixtures、expected 或 tests。

## FreeCAD 调用链

- `src/App/PropertyLinks.h::PropertyLinkBase::ShadowSub`：持久化 old/new element name 对。
- `src/App/ElementNamingUtils.h::ElementNamePair`：字段顺序是 `newName` / `oldName`。
- `src/App/PropertyLinks.cpp::PropertyLinkSub::Restore()`：`DressUp.Base` 的实际 property restore 入口，从 `LinkSub/Sub value`、`shadowed`、`shadow` 重建 sub 和 shadow，并 `setValue()`。
- `src/App/PropertyLinks.cpp::PropertyXLink::Restore()`：从 `sub` / `shadowed` / `shadow` XML 属性恢复 `_SubList` 与 `_ShadowSubList`。
- `src/App/PropertyLinks.cpp::PropertyXLink::afterRestore()` 与 `onContainerRestored()`：恢复 label reference 并注册 element reference。
- `src/App/PropertyLinks.cpp::PropertyXLink::updateElementReference()`：通过 `updateLinkReference()` 更新 `_SubList` / `_ShadowSubList`。
- `src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getContinuousEdges()`：直接遍历 `Base.getShadowSubs()`，优先使用 `newName`，否则使用 `oldName`。
- `src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::onChanged()`：在 BaseFeature 同步 Base 时保留 `Base.getSubValues(false)` 与 `Base.getShadowSubs()`。
- `src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getAddSubShape()`：`SupportTransform=true` 时跳过连续 DressUp，找到前一个 `FeatureAddSub` support。

## cad-core 落点

- `cad-core/tools/collect_freecad_expected.py::link_sub_value()`：当前 FreeCAD oracle collector 会用 `StableSubList` 替代 stale `SubList`，本包 S1/S2 必须先补 native recovery probe 或明确 blocker。
- `cad-core/fixtures/c3m5/dressup-reference-shadow-base-recovery.json`：C7-M3 已有候选 fixture，S0/S1 先复核是否足以驱动 native probe。
- `cad-core/src/app/`：DocumentObject / PropertyLinkSub JSON 输入、link 更新建议与 `documentObjectUpdates` / `elementReferenceUpdates` 口径。
- `cad-core/src/part/`：`ReferenceShadow.brep` 单 subshape snapshot 读取与校验，只能作为恢复证据，不能变成建模输入或完整对象 BREP。
- `cad-core/src/part_design/feature_dress_up.cpp`：DressUp Base、continuous edge、SupportTransform 和 AddSubShape cache 消费点。
- `cad-core/tests/test_p6_topology.py`、`cad-core/tests/test_p7_features.py`：已有 `StableSubList` / `ShadowSub` / `ReferenceShadow` update 断言和 C7-M3 blocked test，是 S3/S4 focused test 的最近落点。

## 步骤队列

1. S0：已冻结 live baseline、C7-M3 blocker 和当前 fixture / expected / test 状态。
2. S1：已复核 FreeCAD native restore / update 调用链，设计不绕过 `ShadowSub` / `ReferenceShadow` 的 FCStd/XML restore probe。
3. S2：执行 native oracle probe / collector 证据补齐；不能证明则写 native oracle blocker。
4. S3：用当前 `cad-core` 做 parity / diagnostics 分类，裁决 implementation gate。
5. S4：按 S3 route 实现正式 recovery 或 no-code blocked 发布收口。
6. S5：release gate，更新 README / 矩阵 / P7 口径并清空队列。

## S1 完成状态

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=d2f530072c`（`d2f530072c 文档：完成 C7-M4 S0 基线冻结`），开始状态 `git status --short -uall` 无输出。
- 已确认 `PropertyXLink::Restore()` 和 `PropertyLinkSub::Restore()` 都支持从 XML `sub` / `Sub value`、`shadowed`、`shadow` 恢复 shadow pair，并通过 `afterRestore()` / `onContainerRestored()` / `updateElementReference()` 进入 element reference 生命周期。
- 已确认 `DressUp::getContinuousEdges()` 使用 `Base.getShadowSubs()` 且优先 `newName`；`DressUp::onChanged()` 保留 `Base.getSubValues(false)` 和 shadow subs；`DressUp::getAddSubShape()` 的 `SupportTransform` traversal 跳过连续 DressUp。
- S2 probe 应 patch FCStd `Chamfer.Base` 的实际 `LinkSub` tag 为 `Sub value="OldFilletEdge1" shadow="Edge1"`，或等价 `XLink sub="OldFilletEdge1" shadow="Edge1"`，reopen 后记录 Base state、`ReferenceShadow` sidecar 和 shape summary。现有 `collect_freecad_expected.py::link_sub_value()` StableSubList-fed 输出只能作为负控。

## 验收入口

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线 docs/CADCore7.0/README.md
git diff --check
```
