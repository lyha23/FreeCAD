# 【已实现】C12-M11 S3 contract gap 分流裁决

## 目标

根据 S1/S2 evidence 裁决问题到底属于 backend response contract、frontend consumer sync、open wire product contract，还是 FreeCAD-grade stable geometry id follow-up。

## 完成结论

- 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=c3ddb3d7b4`（`c3ddb3d7b4 文档：关闭 C12-M11 S2 response contract 复核`），起点 dirty boundary 为 `<clean>`。
- `C12M11-BLOCKER-301` 已关闭，`C12M11-VAL-301` 已记录为 passed；每个 gap row 均已有明确分流。
- closed `p5/sketch-internal-face` backend response 裁为 `current_supported`：当前 response 已发布 `Sketch:InternalEdge1..4` edgeSegments、同名 edge subshapes 和 request-local `stableSubname=Edge1..4`，不打开后端 C++ implementation 包。
- `mvp/rect-pad` edgeSegments/subshapes alignment 裁为 `mismatch_absent`：12 条 edgeSegments 与 12 条 Edge subshapes 对齐，mismatchCount=0。
- request-local `InternalEdgeN -> EdgeN` stableSubname 已 passed；FreeCAD-grade geometry id / mapped-name 跨编辑稳定性分流为 `followup_required_or_deferred`，作为 S4 后续输入，不在 S3 改 stableSubname 主路径。
- 如果后端 response 正确但前端仍丢边，分流到 `frontend_consumer_required_or_pending`：后续应在 `my-chili3d` response consume、selection persistence、草图提交写回与 pick token 保存侧处理，不允许 frontend prefix guessing。
- open `p5/sketch-open-wire-internal-empty` 分流为 `open_wire_product_contract_required_or_deferred`：raw `Sketch:Edge1..3` subshapes 可见且 `mesh=null` 是单独 open-wire 产品契约问题，不混入 closed profile backend gap。
- S4 输入因此收敛为前端同步、stable-id follow-up、open-wire 产品契约三个最小语义批次；除非未来证据推翻 S2/S3，本包不授权 closed profile backend C++ 大改。

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

- `C12M11-BLOCKER-301` 已关闭：每个 gap row 都有明确分流。
- S4 可根据 S3 结果写出前端同步、stable-id follow-up、open-wire 产品契约的最小完整语义批次；当前不需要 closed profile backend implementation。

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
