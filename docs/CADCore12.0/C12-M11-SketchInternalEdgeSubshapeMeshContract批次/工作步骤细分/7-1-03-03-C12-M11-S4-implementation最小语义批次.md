# C12-M11 S4 implementation 最小语义批次

## 目标

根据 S3 裁决，定义后续最小完整语义批次：backend implementation、frontend sync、open wire product contract 或 stable geometry id follow-up。

## 必读文件

- `../README.md`
- `../7-1-02-57-C12-M11-SketchInternalEdgeSubshapeMeshContract批次方案.md`
- `../矩阵/c12m11_sketch_edge_gap_classification.tsv`
- `../矩阵/c12m11_sketch_edge_validation_matrix.tsv`

## 操作

1. 若 backend implementation required，最小批次必须同时覆盖 `sketch_internal_result`、`shape_exporter`、`property_topo_shape`、`topo_shape`、`runtime/recompute` 和 focused tests。
2. 若 frontend consumer required，最小批次必须指向 response consume、selection token persistence、草图提交结果写回和 pick token 保存，不允许前端发明长期 topo identity。
3. 若 stable geometry id follow-up required，最小批次必须先设计 FreeCAD-style geometry id / mapped name 账本，再切换 stableSubname，不得只靠 `EdgeN` 顺序。
4. 若 open wire product contract required，必须先裁决 open sketch 是否应该发布 raw `EdgeN` mesh edgeSegments，不能把 closed profile internal mesh 规则直接套上去。
5. 更新 gap / validation matrix，写清下一包名称建议和验收命令。
6. 将本 S4 step 文件重命名为带 `【已实现】` 的同名文件。

## 关闭条件

- `C12M11-BLOCKER-401` 关闭：后续最小完整语义批次已定义。
- validation matrix 分成本轮短跑、focused response contract、阶段回归或前端同步验证。

## 非目标

- 不在本步骤直接实现。
- 不写 fixture-specific patch。
- 不新增 persistent geometry cache。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M11-SketchInternalEdgeSubshapeMeshContract批次/工作步骤细分 --format markdown
git diff --check
```
