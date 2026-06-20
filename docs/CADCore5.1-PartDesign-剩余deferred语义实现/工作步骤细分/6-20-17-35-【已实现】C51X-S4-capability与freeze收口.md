# C51X-S4 capability 与 freeze 收口

## 目标

把 C51X-S1..S3 的结果同步到 docs、capability、fixtures、tests 和 validation 矩阵，确保没有 broad deferred 回潮。

## 必读

- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/tests/test_adapters.py`
- `docs/CADCore5.1-PartDesign-剩余deferred语义实现/矩阵/cadcore51_followup_scope_review_matrix.tsv`
- `docs/CADCore5.1-PartDesign-剩余deferred语义实现/矩阵/cadcore51_followup_blocker_queue.tsv`
- `docs/CADCore5.1-PartDesign-剩余deferred语义实现/矩阵/cadcore51_followup_validation_matrix.tsv`

## 工作内容

- 更新 capability：supported、exact blockers、product extension 和 non-goals 必须与代码/tests 一致。
- 更新 README 或新增 freeze 总结，记录最终状态。
- 跑 matrix 列数检查、diff check、focused tests。
- 若有实现改动，跑阶段回归；若只复核 docs，说明没有构建必要。

## 完成记录

- `part_design.datum_attachment` capability 更新为 `supported_c51x_selected_attach_engine_with_datum_point_single_input`。
- `DatumPoint Vertex/OnEdge/CenterOfMass selected MapMode` 加入 supported 与 fixture list。
- `datum_attach_engine_remaining_modes` 中已移除 `Vertex`、`OnEdge`、`CenterOfMass`，其余 modes 保持 exact blocker。
- 新增 freeze 总结：`6-20-17-35-【已实现】C51X-exact-blocker后续freeze收口总结.md`。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.1-PartDesign-剩余deferred语义实现 cad-core
for f in docs/CADCore5.1-PartDesign-剩余deferred语义实现/矩阵/cadcore51_followup_*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR " has " NF " columns expected " n; bad=1} END{exit bad}' "$f"; done
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 完成条件

- C51X 队列关闭。
- Remaining blockers 只保留 exact blocker 或明确 product extension 后续项。
- Adapter 仍只发布 schema/capability/diagnostics，不承接几何业务。
