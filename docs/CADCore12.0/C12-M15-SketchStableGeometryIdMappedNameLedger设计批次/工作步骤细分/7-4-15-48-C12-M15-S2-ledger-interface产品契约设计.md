# C12-M15 S2 ledger interface 产品契约设计

## 目标

发布 `SketchGeometryIdentityLedger` 的 request-local interface、字段契约、fallback / diagnostic 规则和前后端消费边界。

## 必读文件

- `../README.md`
- `../7-4-15-44-C12-M15-SketchStableGeometryIdMappedNameLedger设计批次方案.md`
- `../矩阵/c12m15_sketch_geometry_id_contract_matrix.tsv`
- `../矩阵/c12m15_sketch_geometry_id_scope_matrix.tsv`
- `../矩阵/c12m15_sketch_geometry_id_non_goal_registry.tsv`
- S1 已实现后的 step 文档和矩阵更新。

## 操作

1. 定义 ledger interface：输入、输出、invariants、fallback、diagnostics、response fields。
2. 明确 `stable`、`index_fallback`、`deleted`、`geometry_kind_changed`、`split_requires_reselect` 等状态。
3. 明确 `mesh.edgeSegments[]`、`subshapes[]`、`rawSketchEdgeIdentity`、`elementReferenceUpdates` 必须共享同一账本来源。
4. 明确 my-chili3d 只消费字段，不发明长期 topology identity。
5. 更新 contract / scope / non-goal / blocker / validation 矩阵。
6. 将本步骤重命名为 `【已实现】`。

## 关闭条件

- ledger interface 可以作为后续 C++ 实现 seam。
- 所有 fallback 和 diagnostic 状态都有产品语义。
- 不把 `EdgeN` 顺序稳定性伪装成 FreeCAD-grade stable id。

## 非目标

- 不实现 C++。
- 不处理 open wire mesh 产品契约。
- 不处理前端 consumer sync。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次 docs/CADCore12.0/README.md
git diff --check
```
