# P6 复杂引用恢复 oracle-first 收口方案

本文是 `P6-MakerHistory-ShapeFix-DressUp-Taper收敛主线` 的后续小包，只处理 `P6MH-SCOPE-005`：ShapeFix / DressUp / taper 这些 producer 造成 split / deleted 后，旧稳定引用如何恢复。

## 当前结论

- 旧 P6 主线已经把 ShapeFix、DressUp / Refine / transformed、taper ThruSections 的 producer history 发布为 supported。
- P6CR-S4 发布结论：`P6CR-CAND-004` DressUp / Refine 与 `P6CR-CAND-006` two-sided taper 已有 checked-in FreeCAD expected，且当前 cad-core expected / topology / capability focused tests 通过，分类为 `supported`。
- `P6CR-CAND-003` ShapeFix 仍为 `notCollected` / `collectorGap`：当前没有 checked-in ShapeFix expected，只有 c3m1 simple-schema 采集入口缺口，不能升级为 backendGap。
- 当前没有新的 C++ backendGap 证据；S4 只同步 P6CR 与旧 P6 文档 / 矩阵，不修改产品 C++、adapter capability 或 expected 输出端。

## S0 live 基线

- 2026-06-19 复核基线为 `6b1962d5a0`（`assembly: 完成P8扩展DistanceType S6发布闸门`）。
- 启动时 dirty 边界：`AGENTS.md` 已修改；P8 DistanceTypeExtendedGeometry 包有既有文档 / 矩阵修改；本 P6CR 包是既有未跟踪文件。本次 S0 只允许改 P6CR 包内文档和矩阵。
- 旧 P6 矩阵状态：`P6MH-SCOPE-002` ShapeFix、`P6MH-SCOPE-003` DressUp / Refine / transformed、`P6MH-SCOPE-004` taper 和 `P6MH-SCOPE-006` publication 均为 `supported`；`P6MH-SCOPE-005` 仍为 `notCollected`；没有 active `backendGap`。
- 当前 C++ capability 状态：`shape_fix` 为 `covered_no_generated_producer` 且 `remaining=[]`，`dressup` 为 `done_first_slice` 且 `remaining=[]`，`transformed` 为 `covered` 且 `remaining=[]`，`taper_history` 为 `covered_full` 且 `remaining_gaps=[]`，`known_gaps=[]`。
- 因此 `P6CR-BLOCK-001` 与 `P6CR-BLOCK-002` 在 S0 关闭；后续 S1-S3 仍只能先采 oracle，不能把 `notCollected` 直接升级为 C++。

## 范围

本包覆盖三个代表 producer 族：

1. ShapeFix / ReShape：例如小边删除、wire / face 被 ShapeFix 修改后，旧 `EdgeN` / `FaceN` 引用是否 deleted、split 或唯一恢复。
2. DressUp / Refine：例如 Fillet / Chamfer / Draft / Thickness 生成或删除边面后，下游 `Base` / `Support` / `SubShapeBinder` / ExternalGeometry 旧引用如何处理。
3. Taper / ThruSections：例如 tapered Pad / Pocket / Part::Extrusion 生成的 face / edge 经过参数变化或后续引用后，stable subname 是否还能从 maker history 恢复。

## 非目标

- 不实现完整 FreeCAD GUI 选择、TaskPanel、交互重选或跨请求几何缓存。
- 不把 `ReferenceShadow.brep` 扩大成完整对象 BREP 或建模输入；它仍只能作为单个旧 subshape snapshot 证据。
- 不按面积、长度、bbox、fixture 名、输出顺序或几何类型猜唯一恢复。
- 不把 ambiguous split 自动改成 resolved；无法唯一证明时必须保持稳定 diagnostic。
- 不重开已 supported 的 ShapeFix / DressUp / taper producer baseline，除非新 oracle 证明当前实现不一致。

## FreeCAD 依据

| 主题 | FreeCAD 源码入口 | 需要确认的语义 |
| --- | --- | --- |
| ShapeFix history | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShape.h::MapperHistory`; `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::ShapeFixModule::removeSmallEdges()` | `ShapeBuild_ReShape` / `ShapeFix_Root` 如何产出 modified / generated / deleted history |
| ElementMap 消费 | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap()` | `MapperHistory` / `MapperMaker` 如何转成 stable subname 和 terminal diagnostics |
| DressUp support | `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getAddSubShape()` | DressUp cache、RefineModel 和下游 SupportTransform 如何传播旧引用 |
| Fillet / Chamfer | `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureFillet.cpp`; `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureChamfer.cpp` | selected edge / face 经过 dress-up 后是否 modified、deleted 或 split |
| taper ThruSections | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/ExtrusionHelper.cpp`; `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::MapperThruSections` | first / last section 与 generated faces 如何进入 mapper history |

## cad-core 落点

| 层 | 路径 | 本包职责 |
| --- | --- | --- |
| topo | `cad-core/src/part/topo_shape.cpp`; `cad-core/include/cad_core/part/topo_shape.h` | 消费 producer history，形成 split / deleted / resolved / ambiguous terminal evidence |
| ShapeFix | `cad-core/src/part/shape_fix.cpp`; `cad-core/include/cad_core/part/shape_fix.h` | 必要时补 ReShape history 到 ElementMap 的传播证据 |
| DressUp | `cad-core/src/part_design/feature_dress_up.cpp`; `cad-core/src/part_design/feature_dress_up_support.h` | 必要时补 Base stable subname 的 split / deleted recovery 消费 |
| taper | `cad-core/src/part/extrusion_helper.cpp`; `cad-core/src/part_design/feature_extrude.cpp`; `cad-core/src/part/part_extrusion.cpp` | 必要时补 ThruSections generated / modified relation 到 stable reference recovery |
| runtime | `cad-core/src/runtime/recompute.cpp` | 输出 `elementReferenceUpdates`、diagnostics 和 request-local recovery 建议 |
| expected | `cad-core/tools/collect_freecad_expected.py`; `cad-core/fixtures/p6/expected` | 采集或校验 FreeCAD oracle，不用 cad-core 当前输出倒推 expected |

## 实施步骤

1. S0 live 基线：复核旧 P6 矩阵、capabilities、已有 fixtures 和当前 git 状态，确认本包只消费 `P6MH-SCOPE-005`。
2. S1 oracle 候选：从 ShapeFix、DressUp / Refine、taper 三类 producer 中各选 1 个最小可复现场景，写入候选矩阵；不能一开始只挑一个最容易的 fixture。
3. S2 FreeCAD oracle：优先用现有 collector 路径采集 expected；若 collector 不支持该场景，先补 collector / probe，而不是写 C++。
4. S3 cad-core mismatch 分类：把 expected 与 cad-core 输出对比，分成 `supported`、`diagnostic_expected`、`backendGap` 或 `nonGoal`。
5. S4 实现与发布：只有 `backendGap` 行进入 C++；本轮 S3 产生 0 个 backendGap 行，因此 S4 是 publication / old-P6 sync closeout，不修改 C++。

## 最小完整语义批次

本包的最小完整批次是“三类 producer 都过一遍 oracle 决策”：

- ShapeFix 删除或修改一个被旧稳定引用指向的 edge / face。
- DressUp / Refine 让旧引用进入 split / deleted / transformed propagation。
- taper ThruSections 让旧引用经过 generated / modified face history。

如果某类 oracle 在 FreeCAD 侧无法稳定采集，必须在矩阵中保留 `notCollected` 并写明失败原因、下一次采集入口和不能直接实现的理由。

## 验收命令

本轮短跑：

```bash
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P6-ComplexReferenceRecovery-ShapeFix-DressUp-Taper收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/FreeCAD几何生态迁移工程-细分/P6-ComplexReferenceRecovery-ShapeFix-DressUp-Taper收口主线/矩阵/*.tsv
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P6-ComplexReferenceRecovery-ShapeFix-DressUp-Taper收口主线/工作步骤细分 --format markdown
```

实现阶段 focused 验收按实际 touched 范围选择：

```bash
cd cad-core
cmake --build build
python3 -m unittest tests.test_p6_topology.CadCoreP6TopologyTest
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest -k "dressup or taper"
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest
```

需要刷新 FreeCAD expected 时，使用和现有 expected 同源的 `FreeCADCmd` / LibPack / OCCT 环境；若只能在另一套 OCCT 上 smoke test，结论只能标为兼容性探测。

## 产物索引

| 类型 | 路径 |
| --- | --- |
| 工作步骤 | `工作步骤细分/` |
| scope 矩阵 | `矩阵/p6_complex_reference_recovery_scope_review_matrix.tsv` |
| blocker 队列 | `矩阵/p6_complex_reference_recovery_blocker_queue.tsv` |
| source / oracle 候选矩阵 | `矩阵/p6_complex_reference_recovery_source_candidates.tsv` |
| nonGoal 注册 | `矩阵/p6_complex_reference_recovery_non_goal_registry.tsv` |
