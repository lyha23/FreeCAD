# C7-M2 PartDesign Fillet Chamfer 复杂参数引用恢复收口主线总入口

## 结论

C7-M2 的下一步不是直接扩大 `feature_dress_up.cpp`，而是先把 Fillet / Chamfer 的复杂参数和引用恢复 gap 做成可裁决的批量收口。S0/S1 冻结 live 证据和 FreeCAD 调用链，S2 才决定每一类 row 是 already closed、需要 oracle、需要实现、publication-only，还是 non-goal。

## FreeCAD 调用链

- Fillet：`src/Mod/PartDesign/App/FeatureFillet.cpp::Fillet::execute()` 读取 `Radius` / `UseAllEdges`，从 `getBaseTopoShape()` 获取 base shape，按 `UseAllEdges ? getSubTopoShapes(TopAbs_EDGE) : getContinuousEdges(baseShape)` 取边，调用 `shape.makeElementFillet(baseShape, edges, Radius, Radius)`。
- Chamfer：`src/Mod/PartDesign/App/FeatureChamfer.cpp::Chamfer::execute()` 读取 `ChamferType` / `Size` / `Size2` / `Angle` / `FlipDirection` / `UseAllEdges`，`Distance and Angle` 路径把 `size2` 改为 `angle`，再调用 `shape.makeElementChamfer(...)`。
- DressUp：`src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getContinuousEdges()` 负责选边扩展；`DressUp::getAddSubShape()` 在 `SupportTransform=true` 时跳过连续 DressUp，回到前一个 `FeatureAddSub` support。

## cad-core 落点

- `cad-core/src/part_design/feature_dress_up.cpp`：Fillet / Chamfer executor、Base LinkSub 解析、UseAllEdges / selected edges、Face selection expansion、RefineModel、DressUp AddSubShape cache 和 SupportTransform source 解析。
- `cad-core/src/part_design/feature_transformed.cpp`：只在 SupportTransform / transformed family 消费 DressUp slot history 时作为回归边界。
- `cad-core/src/topo/` 与 `cad-core/src/part/topo_shape.cpp`：如果 S2 裁决为稳定恢复 backend gap，必须优先补正式 naming/history 能力，不允许输出端猜测。
- `cad-core/tests/test_p7_features.py` 与 `cad-core/fixtures/p7`：作为现有 basic / diagnostic / SupportTransform expected-backed 基线。

## S0 live 基线

- S0 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=b5767e2391`（`b5767e2391 docs: 新增 C7-M2 Fillet Chamfer 收口方案`），开始时 `git status --short -uall` 无输出。
- C7-M1 live 队列为空；C7-M2 live 队列从 S0-S5 pending 起步，S0/S1 已关闭，下一步为 S2。
- P7 supported baseline：基础 Edge / Face Base、连续边过滤、OCCT fillet/chamfer maker、replacement solid、RefineModel、DressUp AddSubShape cache、slot 级 `NamedShape`、`SupportTransform=true` 和链式 DressUp consumed by transformed family 已有文档、fixtures、expected 与 focused tests。
- P7 remaining gap 保持为 `Fillet / Chamfer 复杂参数组合、复杂引用变更后的完整稳定恢复`；S0 不裁决 complex parameter、UseAllEdges、FlipDirection 或复杂引用恢复是否 closed。
- S0 代表 fixture / expected：`p7/fillet-pad-edge`、`p7/chamfer-pad-edge`、`p7/fillet-refine-true`、`p7/chamfer-refine-true`、`p7/mirrored-fillet-support-transform`、`p7/mirrored-dressup-chain-support-transform`；诊断 fixture：`p7/fillet-missing-edge`、`p7/chamfer-invalid-size`；相邻 C3-M5 证据：`chamfer-two-distances-edge`、`chamfer-distance-angle-edge`、`fillet-face-selection-history`、`chained-dressup-pattern-history`。
- S0 focused tests：`test_p7_fillet_replaces_body_tip_shape`、`test_p7_chamfer_replaces_body_tip_shape`、`test_c3m5_chamfer_parameter_variants_build`、`test_p7_dressup_refine_true_uses_refinemodel_path`、`test_p7_dressup_base_diagnostics_are_structured`、`test_c3m5_dressup_face_selection_records_expanded_edge_history`、`test_p7_mirrored_features_mode_consumes_dressup_support_transform_cache`、`test_p7_mirrored_features_mode_consumes_chained_dressup_support_transform_cache`、`test_c3m5_chained_dressup_pattern_history_keeps_support_transform_slot`。

## 最小完整语义批次

| 批次 | 代表项 | 初始判断 |
| --- | --- | --- |
| live baseline | `fillet-pad-edge`、`chamfer-pad-edge`、`fillet-refine-true`、`chamfer-refine-true`、SupportTransform fixtures | S0 已冻结，expected-backed，不重开 |
| Fillet 选择 | `Radius`、`UseAllEdges`、multi-edge、continuous edge expansion | S1 已补源码/cad-core 落点；multi-edge/UseAllEdges 仍需 S2 裁决是否采 oracle |
| Chamfer 参数 | `Two distances`、`Distance and Angle`、`FlipDirection` | S1 确认 Two distances / Distance and Angle 为既有 c3m5 expected-backed；FlipDirection=true 仍需 S2 裁决 |
| 引用恢复 | Body/DressUp chain Base、`StableSubList` / `FullSubList`、ReferenceShadow 证据 | S1 已记录链式/diagnostic evidence；S2 决定 oracle、diagnostic 或 topo/history 实现 |
| 发布闭环 | fixtures、expected、focused tests、capability/docs | S4/S5 收口 |

## 非目标

- 不做 GUI、TaskPanel、交互选择器。
- 不把 Draft / Thickness 等 full DressUp universe 纳入本包。
- 不把 full MapperHistory / 任意拓扑命名承诺为本包必须完成。
- 不在 executor、adapter 或输出 JSON 里靠 fixture 名称、边编号或 split 形态猜测引用恢复。

## 步骤队列

1. S0：已冻结 live baseline、P7 remaining gap、现有 fixtures/tests/capability。
2. S1：已阅读 FreeCAD 源码和 cad-core fixtures/tests，补完 source / oracle / input contract 矩阵。
3. S2：按 route 裁决 complex params 与引用恢复是否需要实现。
4. S3：如果 S2 授权，实现或 no-code diagnostic boundary 收口。
5. S4：同步 fixtures、tests、capability 和发布文档。
6. S5：阶段回归与 release gate，队列清空后才能声明完成。

## 验收入口

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线
git diff --check
```
