# CADCore13.0

CADCore13.0 用来收口 `topoNamingState` 输出发布与 expected 对齐主线。C13-M1 已完成输出发布闭环；C13-M2 进入 FreeCAD raw mapped-name、child map key、mapper history id 字节级 parity 的最小完整语义批次。C13-M2 S4 暴露出 producer-side mapped-name ledger 缺口后，C13-M3 已作为前置实现批次补齐该账本，C13-M2 队列可从 S4 恢复执行。C13-M4 承接 FreeCADCmd ledger sidecar 裁剪原则，把 checked-in expected 账本闭包与 runtime public projection parity 作为独立门禁。C13-M5 在此基础上建立面向所有 `fixtures/<phase>/expected/*.freecad.json` 的 release output 对齐方案。

当前批次：

| 批次 | 状态 | 入口 |
| --- | --- | --- |
| C13-M1 TopoNamingState 输出发布闭环 | completed / 已完成 | [C13-M1-TopoNamingState输出发布闭环批次](C13-M1-TopoNamingState输出发布闭环批次/README.md) |
| C13-M2 FreeCAD MappedName Parity | active / S4 resume-ready; S5-S6 pending | [C13-M2-FreeCADMappedNameParity实现批次](C13-M2-FreeCADMappedNameParity实现批次/README.md) |
| C13-M3 MappedName Producer Ledger 前置实现 | completed / 已完成 | [C13-M3-MappedNameProducerLedger前置实现批次](C13-M3-MappedNameProducerLedger前置实现批次/README.md) |
| C13-M4 FreeCADExpectedLedger TopoState 投影闭环 | completed / 已完成 | [C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次](C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次/README.md) |
| C13-M5 FreeCADExpected 发布对齐 | active / S3 已完成，S4 pending | [C13-M5-FreeCADExpected发布对齐批次](C13-M5-FreeCADExpected发布对齐批次/README.md) |

- C13-M2 工作步骤总入口已关闭：`C13-M2-FreeCADMappedNameParity实现批次/工作步骤细分/7-8-20-16-【已实现】C13-M2工作步骤总入口.md` 已确认包结构、S0-S6 初始队列和 8 个 TSV 字段数；入口关闭后队列从 S0 继续。
- C13-M2 S0-S3 已关闭；S4 `mappedName codec 实现` 先前因缺少 FreeCAD-equivalent `TopoShape.Tag` / `ElementMap::encodeElementName()` producer ledger 暂停。C13-M3 S1-S4 已解除这个前置 producer-ledger blocker，C13-M2 队列仍从 S4/S5/S6 继续，本页不替 C13-M2 执行实现步骤。
- C13-M3 已完成：`C13-M3-MappedNameProducerLedger前置实现批次/` 关闭 `C13M3-BLOCKER-501`，S5 发布闸门确认 C13-M2 S4 可恢复；`tests.test_topo_naming_state_response` 为 `Ran 15 OK` 且无 expectedFailure，adapter channel 单测为 `Ran 1 OK`。
- C13-M4 已完成：`C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次/` 关闭 `c4m6` public projection 闭环；ledger validator 9/9 green，focused topoNamingState runtime 14 tests OK，S1 projection 已发布。C13-M4 没有新增 C13-M2/C13-M3 回流 blocker。
- C13-M5 工作步骤总入口已关闭：`C13-M5-FreeCADExpected发布对齐批次/工作步骤细分/7-10-00-16-【已实现】C13-M5工作步骤总入口.md` 已确认 README、方案、总入口、S0-S5 和 6 个 TSV 矩阵齐备；S0 关闭后队列从 S1 继续。
- C13-M5 S0 已关闭：live discovery 冻结为 42 个 phase、475 个 `expected/*.freecad.json`，同名 input 与同名 cad-core-res 均齐备；首批 strict lane 固定为 `c4m6`，并明确排除 `*.expeted.json`、`.freecad.ledger.json` sidecar 和 cad-core-res extra。
- C13-M5 S1 已关闭：新增 strict comparator 与 cad-core-res 生成入口，`c4m6` 可生成 structured red report。
- C13-M5 S2 已关闭：`c4m6` strict red report 保持 9 cases、2 green / 7 red，每个 red diff 都有 owner/owner_step/decision/freecad_authority/next_action/close_condition；红灯基线记录 runtime publication、mapperHistory、hash mismatch 和 stableSubname diagnostic 策略缺口。
- C13-M5 S3 已关闭：`c4m6` strict report 中 topoNamingState publication、mapperHistory、hash mismatch 和 stableSubname diagnostic policy 缺口已清零；剩余 red 仅为 `intentional_protocol_divergence`=8，表示 cad-core 前端 transport metadata 与 native expected public semantic oracle 的登记分歧。
- C13-M5 已出方案：`C13-M5-FreeCADExpected发布对齐批次/` 建立 release output 对齐流程，目标是让 cad-core 当前输出按 phase 对齐 `fixtures/<phase>/expected/*.freecad.json`，后续从 S2 `c4m6` 红灯基线继续，再按 phase 家族扩展。

## 阶段边界

- 本阶段只处理 `cad-core` runtime response 中 `topoNamingState` 的收集、发布、消费回归和 fixture 对齐。
- C13-M2 只处理 FreeCAD mapped-name / child map key / mapper history id 的 focused parity，不把全量 expected fixture parity 或前端消费混进同一批次。
- 不从 `fixtures/<phase>/expected/*.freecad.json` 反推实现逻辑；expected 只作为 schema 和 oracle 对照，业务语义来源仍是 FreeCAD `TopoShape` / `ElementMap` / `PropertyLinks` 源码与 `collect_freecad_expected.py` 的 native oracle。

## 验收入口

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M2-FreeCADMappedNameParity实现批次/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M3-MappedNameProducerLedger前置实现批次/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M2-FreeCADMappedNameParity实现批次/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M3-MappedNameProducerLedger前置实现批次/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次/矩阵/*.tsv
git diff --check
```
