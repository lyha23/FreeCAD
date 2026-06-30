# C12-M11 S4 后续最小语义批次

## 目标

根据 S3 裁决，定义后续最小完整语义批次：前端同步、FreeCAD-grade stable geometry id follow-up、open wire product contract。S3 已确认 closed internal profile 后端 response 为 current-supported，S4 默认不打开 closed profile backend C++ 大改。

## 必读文件

- `../README.md`
- `../7-1-02-57-C12-M11-SketchInternalEdgeSubshapeMeshContract批次方案.md`
- `../矩阵/c12m11_sketch_edge_gap_classification.tsv`
- `../矩阵/c12m11_sketch_edge_validation_matrix.tsv`

## 操作

1. 保留 S3 no-backend-implementation 裁决：closed `p5/sketch-internal-face` 的 edgeSegments/subshapes/stableSubname contract 已 current-supported，除非新 evidence 推翻 S2/S3，否则不定义 closed profile backend C++ package。
2. 前端同步最小批次必须指向 `my-chili3d` response consume、selection token persistence、草图提交结果写回和 pick token 保存，不允许前端发明长期 topo identity 或做 prefix guessing。
3. stable geometry id follow-up 最小批次必须先设计 FreeCAD-style geometry id / mapped name 账本，再决定是否升级 stableSubname；不得只靠 `EdgeN` 顺序声称 FreeCAD-grade 稳定。
4. open wire product contract 最小批次必须先裁决 open sketch 是否应该发布 raw `EdgeN` mesh edgeSegments；不能把 closed profile `InternalEdgeN` mesh 规则直接套到 `mesh=null` open wire 上。
5. 更新 gap / validation matrix，写清下一包名称建议和验收命令。
6. 将本 S4 step 文件重命名为带 `【已实现】` 的同名文件。

## 关闭条件

- `C12M11-BLOCKER-401` 关闭：后续最小完整语义批次已定义。
- validation matrix 分成本轮短跑、前端同步验证、stable-id follow-up 设计验证、open-wire 产品契约验证；不把 S3 current-supported closed profile 重开为 backend implementation。

## 非目标

- 不在本步骤直接实现。
- 不写 fixture-specific patch。
- 不新增 persistent geometry cache。
- 不把 S3 current-supported closed profile backend contract 改写成 C++ implementation package。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M11-SketchInternalEdgeSubshapeMeshContract批次/工作步骤细分 --format markdown
git diff --check
```
