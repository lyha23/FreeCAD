# C7-M2 PartDesign Fillet Chamfer 复杂参数引用恢复收口主线

本目录是 CADCore7.0 的第二条主线。它不重开基础 Fillet / Chamfer 支持，而是专门收口 P7 文档里仍保留的 known gap：`Fillet / Chamfer 复杂参数组合、复杂引用变更后的完整稳定恢复`。

当前 P7 基线已经支持基础 Edge / Face Base、连续边过滤、OCCT fillet/chamfer maker、replacement solid、RefineModel、DressUp AddSubShape cache、slot 级 `NamedShape`、`SupportTransform=true` 和链式 DressUp 被 transformed family 消费。C7-M2 的价值是把复杂 Chamfer 参数、Fillet/Chamfer 多边选择、UseAllEdges、Body/DressUp 链式 Base 引用恢复和 expected/capability 发布口径放进同一批次裁决，避免在 executor、adapter 或输出层继续追加 fixture 特判。

## 入口

- 主线总入口：`6-25-16-20-C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线总入口.md`
- 方案：`6-25-16-20-C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- 方案创建基线：`HEAD=6ba500ea32`（`6ba500ea32 文档：完成 C7-M1 S5 发布闸门`）。
- S0 live 基线已冻结：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=b5767e2391`（`b5767e2391 docs: 新增 C7-M2 Fillet Chamfer 收口方案`），开始时 `git status --short -uall` 无输出；C7-M1 队列为空，C7-M2 队列从 S0-S5 pending 起步。
- S0 冻结的 P7 supported baseline：`docs/CADCore方案/细化方案/10-P7-PartDesign常用生态.md` 已记录基础 Edge / Face Base、连续边过滤、OCCT fillet/chamfer maker、replacement solid、RefineModel、DressUp AddSubShape cache、slot 级 `NamedShape`、`SupportTransform=true` 和链式 DressUp 被 transformed family 消费；`cad-core/src/runtime/capability_contract.cpp` 中 `producer_matrix.dressup.status=done_first_slice`，covered 包含 `addsubshape_slot`、`multi_selection_history`、`chamfer_parameter_variants`、`failure_diagnostics`、`chain_dressup_pattern_history`，remaining 为空。
- S0 冻结的 P7 known gap 仍是：`Fillet / Chamfer 复杂参数组合、复杂引用变更后的完整稳定恢复`；S1 已补证据但不做最终裁决，complex parameter、UseAllEdges、FlipDirection 或引用恢复是否发布/采 oracle/实现交给 S2。
- S0 冻结的 fixture / expected baseline：`cad-core/fixtures/p7/{fillet-pad-edge,chamfer-pad-edge,fillet-refine-true,chamfer-refine-true,mirrored-fillet-support-transform,mirrored-dressup-chain-support-transform}.json` 与同名 `expected/*.freecad.json` 已存在；诊断 fixture 包含 `fillet-missing-edge.json`、`chamfer-invalid-size.json`。相邻历史证据还包括 `cad-core/fixtures/c3m5/{chamfer-two-distances-edge,chamfer-distance-angle-edge,fillet-face-selection-history,chained-dressup-pattern-history}.json` 与同名 expected。
- S0 冻结的 focused tests：`test_p7_fillet_replaces_body_tip_shape`、`test_p7_chamfer_replaces_body_tip_shape`、`test_c3m5_chamfer_parameter_variants_build`、`test_p7_dressup_refine_true_uses_refinemodel_path`、`test_p7_dressup_base_diagnostics_are_structured`、`test_c3m5_dressup_face_selection_records_expanded_edge_history`、`test_p7_mirrored_features_mode_consumes_dressup_support_transform_cache`、`test_p7_mirrored_features_mode_consumes_chained_dressup_support_transform_cache`、`test_c3m5_chained_dressup_pattern_history_keeps_support_transform_slot`。
- S1 已完成：FreeCAD 源码证据已拆到 `FeatureFillet.cpp::Fillet::execute`、`FeatureChamfer.cpp::Chamfer::execute/updateProperties/migrateFlippedProperties`、`FeatureDressUp.cpp::getContinuousEdges/getFaces/getAddSubShape`；cad-core 落点已拆到 `feature_fillet.cpp`、`feature_chamfer.cpp`、`feature_dress_up.cpp`、`feature_transformed.cpp`。
- S2 已完成：live baseline 为 `pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=00c035d224`（`00c035d224 docs: 补齐 C7-M2 S1 源码与 oracle 矩阵`），开始时 `git status --short -uall` 无输出。
- S2 route 结论：Chamfer Two distances 与 Distance and Angle 为 `already_closed_expected_backed`，继承 c3m5 fixture/expected/focused test；Fillet multi-edge/UseAllEdges、Chamfer FlipDirection=true、DressUp chain stale ReferenceShadow/Base recovery 为 `oracle_pending_collect`，不能声明 supported，也没有 oracle 证据证明 active backend gap；SupportTransform mirrored / chain baseline 为 `already_closed_expected_backed`；publication drift 为 `publication_closure_only`；GUI、full DressUp universe、full MapperHistory 和输出端猜测为 `diagnostic_non_goal`。
- S2 未产生任何 `backend_gap_requires_implementation`。Code edit gate 保持关闭；下一步 S3 只能做 no-code diagnostic/publication boundary，不改 C++、fixtures、expected 或 tests。
- 当前源码候选包括 `src/Mod/PartDesign/App/FeatureFillet.cpp`、`src/Mod/PartDesign/App/FeatureChamfer.cpp`、`src/Mod/PartDesign/App/FeatureDressUp.cpp`、`cad-core/src/part_design/feature_fillet.cpp`、`cad-core/src/part_design/feature_chamfer.cpp`、`cad-core/src/part_design/feature_dress_up.cpp`、`cad-core/src/part_design/feature_transformed.cpp`、`cad-core/tests/test_p7_features.py` 和 `cad-core/fixtures/p7`。

## 收口边界

- Fillet：`Radius`、`UseAllEdges`、Base LinkSub、连续边扩展、multi-edge 选择和 replacement refine。
- Chamfer：`ChamferType` 的 `Equal distance`、`Two distances`、`Distance and Angle`，以及 `Size`、`Size2`、`Angle`、`FlipDirection`、`UseAllEdges`。
- DressUp 引用恢复：Body member Base、前序 Fillet/Chamfer Base、`StableSubList` / `FullSubList` 输入、`SupportTransform` AddSubShape cache、链式 DressUp source base。
- 发布口径：fixtures、expected、focused tests、capability/docs 必须一致。S2 已裁决为 no backend gap；S3/S4 只能发布 inherited expected-backed、oracle pending 与 non-goal 边界，不能把 oracle pending 写成 supported capability。

## 非目标

- 不实现 GUI Fillet / Chamfer task panel 或交互选择器。
- 不扩展到 full DressUp universe，例如 Draft、Thickness 或全部 dress-up 类型。
- 不把任意拓扑命名和完整 MapperHistory 作为本包必须完成的目标。
- 不允许在 adapter、executor 输出端或 fixture 名称上做引用恢复猜测。
- 不覆盖 transformed family 全部复杂参数，只回归 `SupportTransform` / DressUp 链路中与 Fillet / Chamfer 相关的发布边界。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线
git diff --check
```
