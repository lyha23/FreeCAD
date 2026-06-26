# 【已实现】C7-M7 S4 cad-core parity 与 implementation gate

## 目标

基于 S3 native oracle / blocker 结果，对当前 `cad-core` 做 parity 或 diagnostics 分类，裁决 S5 是否打开 C++ implementation gate。S4 默认不改 C++。

## S4 结论

- live 基线：执行时 `pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=24b7649fa5`（`24b7649fa5 文档：完成 C7-M7 S3 native oracle 采集`），`git status --short -uall` 无输出；队列显示 S4-S6 pending。
- S3 没有产生可比较的 source-backed native expected：ORACLE-202、ORACLE-302、ORACLE-402 均为 `native_oracle_blocked`，ORACLE-203 仍是 STL mesh-specific `oracle_blocker`。
- S4 裁决：ORACLE-202 / 302 / 402 只能发布为 `oracle_blocked`，不能写成 `backend_gap_requires_implementation`；ORACLE-203 继续保持 `oracle_blocker`。既有 request-local Link display / alias / `FullSubList` / mapped postfix / imported Link chain、ShowElement `documentObjectUpdates`、BREP / STEP / IGES `history_partial` ElementMap 和 STL `indexed_only` rows 保持 already-covered / already-closed。
- S5 implementation gate 关闭。S5 只能做 no-code publication closure；允许修改文件限于 `docs/CADCore7.0/README.md`、本包 `README.md`、主线总入口、方案、工作步骤总入口 / S5 / S6 文档和本包 `矩阵/*.tsv`。
- S5 不允许修改 `cad-core/src/app/*`、`cad-core/src/part/*`、`cad-core/src/runtime/*`、`cad-core/src/mesh/*`、`cad-core/src/adapters/*`、`cad-core/tests/*`、fixtures、expected、collector 或生成输出；不得从 current `cad-core` 输出倒推 expected。

## 必读文件

- S3 完成后的 C7-M7 README、方案和矩阵。
- S3 新增或更新的 fixture / expected / known_gap。
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_expected_fixtures.py`
- `cad-core/src/app/link.cpp`
- `cad-core/src/app/document_object.cpp`
- `cad-core/src/app/property_links.cpp`
- `cad-core/src/app/element_map.cpp`
- `cad-core/src/part/part_import.cpp`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/src/part/topo_shape_mapper.cpp`
- `cad-core/src/runtime/element_reference_update.cpp`
- `cad-core/src/mesh/feature_mesh_import.cpp`
- `cad-core/src/adapters/c_api/c_api.cpp`

## 执行要点

1. 记录 live baseline 和 C7-M7 queue。
2. 若 S3 是 native oracle，运行 current `cad-core` 并比较 ElementMap、NamedShape alias、LinkSub, diagnostics、documentObjectUpdates、elementReferenceUpdates、capability publication。
3. 若 S3 是 blocker，确认 focused test 保持 blocker，不打开 implementation gate。
4. 写入 route：`already_closed_expected_backed`、`backend_gap_requires_implementation`、`oracle_blocked` 或 `diagnostic_non_goal`。
5. 如果打开 implementation gate，列出 S5 允许修改的文件、FreeCAD 依据、non-goals 和 focused test 名称。
6. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S5。

## 裁决规则

- FreeCAD native oracle 可证明 + current `cad-core` 匹配：`already_closed_expected_backed`。
- FreeCAD native oracle 可证明 + current `cad-core` 不匹配：`backend_gap_requires_implementation`。
- FreeCAD native 证据不足：`oracle_blocked`。
- FreeCAD native 明确不支持或超出无状态 CAD Core 边界：`diagnostic_non_goal`。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/矩阵/*.tsv
git diff --check
```

## 完成标准

- S5 code gate 状态明确。
- 若打开 code gate，S5 范围足够窄且有 FreeCAD source authority。
- 队列推进到 S5。
