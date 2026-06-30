# C12-M11 S3 contract gap 分流裁决

## 目标

根据 S1/S2 evidence 裁决问题到底属于 backend response contract、frontend consumer sync、open wire product contract，还是 FreeCAD-grade stable geometry id follow-up。

## 必读文件

- `../README.md`
- `../矩阵/c12m11_sketch_edge_gap_classification.tsv`
- `../矩阵/c12m11_sketch_edge_contract_matrix.tsv`
- `../矩阵/c12m11_sketch_edge_non_goal_registry.tsv`

## 操作

1. 如果 closed internal profile 已返回对齐的 `InternalEdgeN` edgeSegments/subshapes/stableSubname，则 backend contract 初步裁为 `current_supported`。
2. 如果 edgeSegments 缺失、id 未加 object prefix、`indexed` 不对齐或 stableSubname 为空，裁为 `backend_implementation_required` 并定位落点。
3. 如果 backend response 正确但前端仍丢边，裁为 `frontend_consumer_required`，后续改动应在 `my-chili3d` 的 response consume / selection persistence 侧。
4. 如果 request-local `InternalEdgeN -> EdgeN` 成立但跨编辑重命名仍弱，裁为 `stable_geometry_id_followup_required`。
5. 如果 open wire raw `EdgeN` 需要 mesh edgeSegments，而当前契约未要求，裁为 `open_wire_product_contract_required`。
6. 将本 S3 step 文件重命名为带 `【已实现】` 的同名文件。

## 关闭条件

- `C12M11-BLOCKER-301` 关闭：每个 gap row 都有明确分流。
- S4 能根据 S3 结果写出最小完整语义批次或确认不需要 backend implementation。

## 非目标

- 不把所有分流都合并成一个 C++ 大改。
- 不允许 frontend prefix guessing。
- 不改变 C12-M10 CopyOnChange 口径。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M11-SketchInternalEdgeSubshapeMeshContract批次/矩阵/*.tsv
git diff --check
```
