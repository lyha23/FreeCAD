# CADCore13.0

CADCore13.0 用来收口 `topoNamingState` 输出发布与 expected 对齐主线。C13-M1 已完成输出发布闭环；C13-M2 进入 FreeCAD raw mapped-name、child map key、mapper history id 字节级 parity 的最小完整语义批次。C13-M2 S4 暴露出 producer-side mapped-name ledger 缺口后，C13-M3 已作为前置实现批次补齐该账本，C13-M2 队列可从 S4 恢复执行。C13-M4 承接 FreeCADCmd ledger sidecar 裁剪原则，把 checked-in expected 账本闭包与 runtime public projection parity 作为独立门禁。C13-M5 在此基础上以 fixture-role manifest、v2 parity report、精确 divergence registry 与 live release gate 收口当前 binary 的 expected 对齐证据。

当前批次：

| 批次 | 状态 | 入口 |
| --- | --- | --- |
| C13-M1 TopoNamingState 输出发布闭环 | completed / 已完成 | [C13-M1-TopoNamingState输出发布闭环批次](C13-M1-TopoNamingState输出发布闭环批次/README.md) |
| C13-M2 FreeCAD MappedName Parity | active / S4 resume-ready; S5-S6 pending | [C13-M2-FreeCADMappedNameParity实现批次](C13-M2-FreeCADMappedNameParity实现批次/README.md) |
| C13-M3 MappedName Producer Ledger 前置实现 | completed / 已完成 | [C13-M3-MappedNameProducerLedger前置实现批次](C13-M3-MappedNameProducerLedger前置实现批次/README.md) |
| C13-M4 FreeCADExpectedLedger TopoState 投影闭环 | completed / 已完成 | [C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次](C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次/README.md) |
| C13-M5 FreeCADExpected 发布对齐 | completed / 已完成；S4 family known gaps 仍按矩阵跟踪 | [C13-M5-FreeCADExpected发布对齐批次](C13-M5-FreeCADExpected发布对齐批次/README.md) |

- C13-M2 工作步骤总入口已关闭：`C13-M2-FreeCADMappedNameParity实现批次/工作步骤细分/7-8-20-16-【已实现】C13-M2工作步骤总入口.md` 已确认包结构、S0-S6 初始队列和 8 个 TSV 字段数；入口关闭后队列从 S0 继续。
- C13-M2 S0-S3 已关闭；S4 `mappedName codec 实现` 先前因缺少 FreeCAD-equivalent `TopoShape.Tag` / `ElementMap::encodeElementName()` producer ledger 暂停。C13-M3 S1-S4 已解除这个前置 producer-ledger blocker，C13-M2 队列仍从 S4/S5/S6 继续，本页不替 C13-M2 执行实现步骤。
- C13-M3 已完成：`C13-M3-MappedNameProducerLedger前置实现批次/` 关闭 `C13M3-BLOCKER-501`，S5 发布闸门确认 C13-M2 S4 可恢复；`tests.test_topo_naming_state_response` 为 `Ran 15 OK` 且无 expectedFailure，adapter channel 单测为 `Ran 1 OK`。
- C13-M4 已完成：`C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次/` 关闭 public projection / ledger 边界；当前 c4m6 native role 是 9 个 ledger pair，HistoryProbe 已在 C13-M5 迁为 protocol-only。C13-M4 没有新增 C13-M2/C13-M3 回流 blocker。
- C13-M5 已完成：`freecad_expected_parity` 用 fixture-role manifest 统一 native discovery、collector、generator 与 test；v2 report 分开 exact / semantic / release verdict，`--release-gate` 验证 live binary、strict ledger preflight、registry audit 与 current freshness。
- C13-M5 c4m6：schema、producer、hash、encoding 与 foreign top-level owner 都在 recompute 前 diagnostics-only hard fail；CompoundLink 有 native semantic result；HistoryProbe 为 protocol-only；仅五个精确 transport selector 可形成 `protocol_divergence`，绝不等于 exact green。
- C13-M5 S4 representative family 仍是 red classified known-gap surface；C13-M5 关闭的是可审计 gate，而不是把 `c3m1`、`c10m1`、`c12m12`、`c3m5`、`c3m6` 宣称为 green。

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
