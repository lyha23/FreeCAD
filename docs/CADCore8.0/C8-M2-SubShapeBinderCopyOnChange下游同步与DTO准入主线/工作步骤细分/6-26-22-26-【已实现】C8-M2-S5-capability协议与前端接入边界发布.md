# 【已实现】C8-M2-S5 capability 协议与前端接入边界发布

## 目标

同步 FreeCAD `cad-core` capability、diagnostics、known_gap 和前端接入边界。S5 必须把 supported、sync-required、known_gap、oracle-blocked 和 non-goal 明确分开。

## live 基线

- `pwd=/home/user/Chili3DProject/FreeCAD`
- `HEAD=d1afdb460f`（`d1afdb460f docs: 完成 C8-M2 S4 下游同步契约`）
- S5 开始时 `git -c core.quotepath=false status --short -uall` 为空，未发现无关 dirty 文件。
- 队列首项是本 S5 文件；S6 仍 pending。

## 发布的能力口径

- `part_design.shape_binder.status`
- `part_design.sub_shape_binder.status`
- `part_design.sub_shape_binder.remaining_gaps`
- `part_design.sub_shape_binder.known_gaps.copy_on_change_full_temporary_document_cache`
- 下游同步合同中的 type ids / fixtures / diagnostics
- 前端不得持久保存 full BREP / NamedShape / ElementMap cache 的边界

## S5 结论

| 分类 | S5 发布口径 | 后续 |
| --- | --- | --- |
| supported | `part_design.shape_binder.status=supported_c8m1_expected_backed_request_local`，`remaining_gaps=[]`；`part_design.sub_shape_binder.status=supported_c8m1_expected_backed_request_local_with_copy_on_change_known_gap` | 保持 C8-M1 expected-backed request-local support |
| sync_required | `PartDesign::ShapeBinder`、`PartDesign::SubShapeBinder`、`PartDesign::SubShapeBinderPython`、C8-M1 12 个 input / 12 个 expected、diagnostics 和 ElementMap / NamedShape 输出合同已发布给下游 | 下游 `opencascade-rs` 另包实现，本仓库不改 Rust |
| known_gap | `copy_on_change_full_temporary_document_cache` 仍在 `part_design.sub_shape_binder.remaining_gaps` | 不因 property-state evidence 关闭 |
| oracle_blocked | known gap 的 `route=oracle_blocked`，diagnostic 为 `copy_on_change_full_temporary_document_cache_not_supported`，保留 delete/reopen condition | S6 只能做 no-code 或窄 DTO gate 裁决 |
| diagnostic_non_goal | GUI/session/persistent cache/Rust 下游，以及前端持久 full BREP/TopoDS_Shape/NamedShape/ElementMap/temporary cache 均为非目标 | 只允许消费 request-local mesh、subshapes、full subname、diagnostics 与 reference update evidence |

## 回写矩阵

- `c8m2_copyonchange_scope_review_matrix.tsv`：`C8M2-SCOPE-101..103` 发布为 `sync_required_published_S5`，`C8M2-SCOPE-201..203` 发布为候选 / oracle evidence / known_gap 的 S5 状态；`C8M2-SCOPE-301` 发布为 `diagnostic_non_goal_published_S5`。
- `c8m2_copyonchange_blocker_queue.tsv`：`C8M2-BLOCKER-501` 关闭为 `closed_S5_capability_known_gap_frontend_boundary_published_without_closing_S6`。
- `c8m2_copyonchange_backend_gap_classification.tsv`、`c8m2_downstream_sync_contract.tsv`、`c8m2_copyonchange_non_goal_registry.tsv`、`c8m2_copyonchange_validation_matrix.tsv`：同步 S5 发布状态和前端不得持久保存 full BREP / TopoDS_Shape / NamedShape / ElementMap / temporary cache 的边界。

## 验证结果

- `cd cad-core && ./cad-core capabilities > /tmp/c8m2-capabilities.json`：通过。
- `cd cad-core && python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics`：通过，18 个测试 OK。
- S5 只改文档和矩阵；`cad-core/src/runtime/capability_contract.cpp` 与 focused tests 未修改。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
./cad-core capabilities > /tmp/c8m2-capabilities.json
python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics
```

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'shape_binder|sub_shape_binder|copy_on_change_full_temporary_document_cache|known_gap|oracle_blocked|sync_required|diagnostic_non_goal' cad-core/src/runtime/capability_contract.cpp cad-core/tests docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/矩阵/*.tsv
git diff --check
```

本文件已按原时间前缀重命名为 `6-26-22-26-【已实现】C8-M2-S5-capability协议与前端接入边界发布.md`，索引链接已同步更新。

## 非目标

- 不把 oracle-blocked lifecycle 写成 supported。
- 不用 docs-only 发布替代 focused tests。
- 不修改下游 Rust adapter。
- 不关闭 S6 implementation / no-code gate。
