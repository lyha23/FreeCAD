# C7-M4 DressUp ReferenceShadow 原生恢复证据与实现准入主线总入口

## 结论

C7-M4 是 C7-M3 后续的单一 blocker 处理包。C7-M3 已证明 Fillet / Chamfer 参数能力 expected-backed；本包只处理 stale `ReferenceShadow` / Base recovery 是否有 FreeCAD native 恢复证据，以及该证据是否足以打开 `cad-core` implementation gate。

当前结论仍是 gate closed：S2 已执行 native FCStd/XML restore probe，但 `Base.getShadowSubs()` / `getSubValues(false/true)` 在 FreeCADCmd Python 层不可观察，route=`native_oracle_blocked`。S3 没有新增能删除该 blocker 的 native oracle 证据，已裁决 route=`oracle_blocked`。S4 已按该裁决完成 no-code blocked publication closure；S5 release gate 已关闭 `C7M4-BLOCKER-501` / `C7M4-GATE-501` 并清空 C7-M4 队列。没有 native oracle 前，不实现 C++，不发布 supported，不做 output-side guessing，StableSubList-fed geometry 负控不能删除 blocker。

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
3. S2：已执行 native oracle probe / collector 证据补齐，route=`native_oracle_blocked`。
4. S3：已基于 S2 blocker 裁决 route=`oracle_blocked`，implementation gate closed。
5. S4：已完成 no-code blocked 发布收口；未实现 ReferenceRecovery，未修改 C++ 主路径。
6. S5：已完成 release gate，复核 README / 矩阵 / P7 口径并清空队列。

## S1 完成状态

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=d2f530072c`（`d2f530072c 文档：完成 C7-M4 S0 基线冻结`），开始状态 `git status --short -uall` 无输出。
- 已确认 `PropertyXLink::Restore()` 和 `PropertyLinkSub::Restore()` 都支持从 XML `sub` / `Sub value`、`shadowed`、`shadow` 恢复 shadow pair，并通过 `afterRestore()` / `onContainerRestored()` / `updateElementReference()` 进入 element reference 生命周期。
- 已确认 `DressUp::getContinuousEdges()` 使用 `Base.getShadowSubs()` 且优先 `newName`；`DressUp::onChanged()` 保留 `Base.getSubValues(false)` 和 shadow subs；`DressUp::getAddSubShape()` 的 `SupportTransform` traversal 跳过连续 DressUp。
- S2 probe 应 patch FCStd `Chamfer.Base` 的实际 `LinkSub` tag 为 `Sub value="OldFilletEdge1" shadow="Edge1"`，或等价 `XLink sub="OldFilletEdge1" shadow="Edge1"`，reopen 后记录 Base state、`ReferenceShadow` sidecar 和 shape summary。现有 `collect_freecad_expected.py::link_sub_value()` StableSubList-fed 输出只能作为负控。

## S2 完成状态

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=dc041901a7`（`dc041901a7 文档：完成 C7-M4 S1 native probe 设计`），开始状态 `git status --short -uall` 无输出。
- 曾新增并运行 C7-M4 ReferenceShadow native probe（脚本现已移除）：baseline FCStd 由现有 fixture 构造，`Document.xml` 中 `Chamfer.Base` patch 为 `LinkSub/Sub value=OldFilletEdge1 shadow=Edge1`，reopen/recompute `returncode=0`。
- 证据写入 `cad-core/fixtures/c3m5/dressup-reference-shadow-base-recovery.native-probe.evidence.json`：XML patch、FreeCAD version、ReferenceShadow sidecar、Python-visible `Chamfer.Base`、`dumpPropertyContent()`、`Chamfer` / `Body` shape summary 和命令 returncode 已记录。
- route=`native_oracle_blocked`：阻塞层不是 FreeCADCmd 或 geometry，而是 FreeCAD Python property API 只能观察 tuple / property dump，不能观察 `Base.getShadowSubs()`、`getSubValues(false)`、`getSubValues(true)`；StableSubList-fed 负控命令 `returncode=0`，但只能证明 Edge1-fed geometry。

## S3 完成状态

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=01aeef0217`（`01aeef0217 证据：补齐 C7-M4 S2 native probe`），开始状态 `git status --short -uall` 无输出。
- route=`oracle_blocked`：S3 只读取 S2 fixture / expected / evidence、P7/P6 focused tests、`cad-core/src/app`、`cad-core/src/part` 和 `cad-core/src/part_design/feature_dress_up.cpp`，没有新的 native oracle 证据能观察 `Base.getShadowSubs()` / `getSubValues(false/true)`。
- implementation gate closed：`dressup-reference-shadow-base-recovery` expected 继续保持 `known_gap.kind=dressup_reference_shadow_base_recovery_native_oracle_blocked`；StableSubList-fed geometry 输出不能替代 native restore evidence。
- S4 边界：只允许更新 no-code blocked / diagnostic 发布口径和矩阵；不改 C++、collector/probe、fixtures/expected，不实现 `ReferenceRecovery`，不扩大到 full DressUp / MapperHistory。

## S4 完成状态

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=381d56ef9e`（`381d56ef9e 文档：完成 C7-M4 S3 准入裁决`），开始状态 `git status --short -uall` 无输出。
- route=`oracle_blocked` 继续保留：S2 native probe `returncode=0` 只证明 FCStd/XML restore 可运行且 shape summary 成功；由于 FreeCAD Python API 不能观察 `Base.getShadowSubs()` / `getSubValues(false/true)`，它仍不能证明 `ShadowSub` / `ReferenceShadow` 原生恢复生命周期。
- no-code publication closure 已应用：`C7M4-BLOCKER-401` / `C7M4-GATE-401` 关闭，`dressup-reference-shadow-base-recovery` 继续发布为 `oracle_blocked`；未改 C++、collector/probe、fixtures/expected/tests，现有 focused blocker test 保留，不新增测试。
- 后续重开 gate 的条件只有两个：新增 native introspection 能观察上述 Base property 生命周期，或上游 scope 明确接受当前 FreeCAD Python API 限制；StableSubList-fed geometry 负控不能作为删除 blocker 的依据。

## S5 完成状态

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=d7337b49f1`（`d7337b49f1 文档：完成 C7-M4 S4 no-code 发布收口`），开始状态 `git status --short -uall` 无输出。
- release gate 已完成：focused blocker unittest 1 test OK，C7-M4 queue 为空，TSV 列数检查、trailing whitespace 检查和 `git diff --check` 通过。
- final route=`oracle_blocked`：S2 native probe `returncode=0` 仍不能观察 `Base.getShadowSubs()` / `getSubValues(false/true)`，StableSubList-fed geometry 负控不能删除 blocker。
- `C7M4-BLOCKER-501` / `C7M4-GATE-501` 已关闭；未改 C++、collector/probe、fixtures/expected/tests，未运行 FreeCADCmd、cmake build 或全量 FreeCAD build。

## 验收入口

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线 docs/CADCore7.0/README.md docs/CADCore方案/细化方案/10-P7-PartDesign常用生态.md
git diff --check
```
