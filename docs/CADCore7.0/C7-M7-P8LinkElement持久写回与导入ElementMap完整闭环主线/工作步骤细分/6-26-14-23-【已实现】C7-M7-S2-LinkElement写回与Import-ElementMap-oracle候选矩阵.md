# 【已实现】C7-M7 S2 LinkElement 写回与 Import ElementMap oracle 候选矩阵

## 目标

把 S1 的 source / coverage 复核结果裁成最小完整语义批次，明确哪些进入 native oracle 采集，哪些保持 already covered，哪些是 diagnostic non-goal。S2 不采 oracle，不改 C++。

## 必读文件

- S1 完成后的 C7-M7 README、方案和矩阵。
- `docs/CADCore方案/细化方案/11-P8-Part导入导出与Assembly后续.md`
- `cad-core/fixtures/p8` 与 relevant expected。
- `cad-core/tests/test_p8_features.py`
- `cad-core/tools/collect_freecad_expected.py`

## 执行要点

1. 记录 live baseline 和 C7-M7 queue。
2. 形成 candidate 分类：`already_covered`、`oracle_candidate`、`oracle_blocker`、`backend_gap_candidate`、`diagnostic_non_goal`。
3. 对每个 `oracle_candidate` 写清 fixture 输入、FreeCAD source authority、expected 字段、collector 命令和 focused test。
4. 明确哪些 P8 baseline rows 不得重开，哪些 GUI / cross-request / frontend-only 路径不得采 native golden。
5. 更新 oracle plan、scope、backend gate、blocker queue、validation matrix 和方案 S2 小节。
6. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S3。

## 候选批次要求

- 同一 FreeCAD 调用链和同一 ElementMap / LinkSub / writeback 账本能覆盖的 case 应批量推进，不要只挑单 fixture。
- 缺 native lifecycle 的 GUI / cross-request backend state / front-end-only protocol 不能进入 implementation gate。
- 如果只能拆小批次，必须写清下一批次范围和拆分理由。

## S2 结论

- live 基线：执行时 `pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=d6f62daad5`（`d6f62daad5 文档：完成 C7-M7 S1 源码覆盖复核`），`git status --short -uall` 无输出；队列显示 S2-S6 pending。
- `already_covered`：现有 p8/c3m2 coverage 已覆盖基础 Link / LinkSub / LinkGroup / LinkElement display 与 alias、`FullSubList` / mapped postfix、terminal / merge history、BREP / STEP / IGES `history_partial` import ElementMap、`app-link-imported-element-map-chain` imported Link chain、ShowElement request-local `documentObjectUpdates` 和 cross-document request-graph diagnostics。这些 rows 不得在 S3/S4 被重开为 active backend gap。
- `oracle_candidate`：三组最小完整语义批次进入 S3：1）BREP / STEP / IGES complete imported-shape `ElementMap` / stable reference lifecycle；2）ShowElement `LinkElement` / `LinkGroup` persistent writeback transaction；3）复杂多层 `LinkSub` / cross-document hash-postfix save/restore lifecycle。每组的 fixture/probe 输入、FreeCAD authority、expected 字段、S3 命令和 focused tests 已写入 `oracle_plan.tsv`。
- `oracle_blocker`：STL complete Part-style ElementMap 暂不采 native oracle。FreeCAD `Mesh::Import::execute()` 只把 mesh 写入 `Mesh.setValuePtr(...)`，现有 `mesh-import-stl.freecad.json` 固定 `element_map_status=indexed_only`；除非后续开 mesh-specific oracle 包，否则不把它作为 Part `PropertyPartShape` ElementMap 缺口。
- `backend_gap_candidate`：只保留为 S4 parity 后的候选状态。S2 不设置 `backend_gap_requires_implementation`，不打开 S5 code gate。
- `diagnostic_non_goal`：GUI / ViewProvider / Workbench、frontend sync protocol、cross-request backend cache / persistent BREP、Worker / WASM / Web 产品化继续排除。
- S2 已关闭 `C7M7-BLOCKER-201` 与 S2 分类 gate；未采 native oracle，未新增或修改 fixture/expected/test，未改 C++。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/工作步骤细分 --format markdown
rg -n 'oracle_candidate|already_covered|backend_gap_candidate|diagnostic_non_goal|LinkElement|LinkGroup|ElementMap|FullSubList|LinkSub|documentObjectUpdates|elementReferenceUpdates' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/矩阵/*.tsv
git diff --check
```

## 完成标准

- S3 有明确 oracle 采集清单或 blocker 清单。
- S2 没有修改 C++、fixtures、expected 或 tests。
- 队列推进到 S3。
