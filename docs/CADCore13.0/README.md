# CADCore13.0

CADCore13.0 用来收口 `topoNamingState` 输出发布与 expected 对齐主线。C13-M1 已完成输出发布闭环；C13-M2 进入 FreeCAD raw mapped-name、child map key、mapper history id 字节级 parity 的最小完整语义批次。C13-M2 S4 暴露出 producer-side mapped-name ledger 缺口后，C13-M3 作为前置实现批次承接该账本补齐。C13-M4 承接 FreeCADCmd ledger sidecar 裁剪原则，把 checked-in expected 账本闭包与 runtime public projection parity 作为独立门禁。

当前批次：

| 批次 | 状态 | 入口 |
| --- | --- | --- |
| C13-M1 TopoNamingState 输出发布闭环 | completed / 已完成 | [C13-M1-TopoNamingState输出发布闭环批次](C13-M1-TopoNamingState输出发布闭环批次/README.md) |
| C13-M2 FreeCAD MappedName Parity | active / S4 blocked by producer ledger | [C13-M2-FreeCADMappedNameParity实现批次](C13-M2-FreeCADMappedNameParity实现批次/README.md) |
| C13-M3 MappedName Producer Ledger 前置实现 | active / planned | [C13-M3-MappedNameProducerLedger前置实现批次](C13-M3-MappedNameProducerLedger前置实现批次/README.md) |
| C13-M4 FreeCADExpectedLedger TopoState 投影闭环 | completed / 已完成 | [C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次](C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次/README.md) |

- C13-M2 工作步骤总入口已关闭：`C13-M2-FreeCADMappedNameParity实现批次/工作步骤细分/7-8-20-16-【已实现】C13-M2工作步骤总入口.md` 已确认包结构、S0-S6 初始队列和 8 个 TSV 字段数；入口关闭后队列从 S0 继续。
- C13-M2 S0-S3 已关闭，S4 `mappedName codec 实现` 因缺少 FreeCAD-equivalent `TopoShape.Tag` / `ElementMap::encodeElementName()` producer ledger 暂停，不继续用 expected 字符串或 fixture 分支硬凑。
- C13-M3 已建包：`C13-M3-MappedNameProducerLedger前置实现批次/`，目标是先在 `NamedShape` 生产阶段携带 tag / sourceTag / op / raw mapped-name provenance，再回流 C13-M2 S4。
- C13-M4 已完成：`C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次/` 关闭 `c4m6` public projection 闭环；ledger validator 9/9 green，focused topoNamingState runtime 14 tests OK，S1 projection 已发布。C13-M4 没有新增 C13-M2/C13-M3 回流 blocker，既有 C13-M2 S4 producer ledger blocker 与 C13-M3 前置实现边界保持不变。

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
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M2-FreeCADMappedNameParity实现批次/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M3-MappedNameProducerLedger前置实现批次/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次/矩阵/*.tsv
git diff --check
```
