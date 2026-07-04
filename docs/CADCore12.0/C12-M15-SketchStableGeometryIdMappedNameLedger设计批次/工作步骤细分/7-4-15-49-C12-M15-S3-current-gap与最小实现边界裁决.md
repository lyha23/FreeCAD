# C12-M15 S3 current gap 与最小实现边界裁决

## 目标

对照 S2 ledger 契约和当前 cad-core coverage，裁决是否需要后续 C++ implementation package，以及最小完整语义批次应覆盖哪些场景。

## 必读文件

- `../README.md`
- `../7-4-15-44-C12-M15-SketchStableGeometryIdMappedNameLedger设计批次方案.md`
- `../矩阵/c12m15_sketch_geometry_id_contract_matrix.tsv`
- `../矩阵/c12m15_sketch_geometry_id_scope_matrix.tsv`
- `../矩阵/c12m15_sketch_geometry_id_blocker_queue.tsv`
- S1 / S2 已实现后的 step 文档和矩阵更新。

## 操作

1. 检查当前 cad-core 是否已覆盖合法 geometry id 发布、fallback、invalid/duplicate id diagnostic 和 reference resolution。
2. 对每个 contract row 判定 `current_supported`、`implementation_needed`、`design_only` 或 `blocked`。
3. 如果需要实现，定义后续最小完整语义批次：reorder、insert/delete、kind drift、missing id fallback、internal edge source mapping。
4. 明确不把前端 consumer sync 或 open wire mesh contract 塞进本实现包。
5. 更新 scope / contract / blocker / validation 矩阵。
6. 将本步骤重命名为 `【已实现】`。

## 关闭条件

- 每个 contract row 有 current gap 裁决。
- 后续实现包若被授权，已有明确 C++ 落点、fixtures/tests 和非目标。
- 若 no-code，必须证明 current coverage 已满足 S2 契约。

## 非目标

- 不直接实现后续 C++。
- 不扩展到所有 sketch solver / constraint identity。
- 不运行全量 FreeCAD build。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p5_features
cd ..
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次/矩阵/*.tsv
git diff --check
```
