# C12-M1 CAD Core capability implementation candidate 盘点批次

## 当前定位

C12-M1 是 CADCore12.0 的候选筛选闸门，不是直接代码实现包。它先把 live capability、C9-M5 / C10-M4 CopyOnChange retained gap、C11-M1 / C11-M2 no-code parity 复开结果和 current adapter assertions 汇总成可执行矩阵，再决定下一包是否有资格进入 C++。

S0 live 冻结结论：唯一 active `remaining_gaps` 是 SubShapeBinder CopyOnChange full temporary-document cache，但它仍是 `known_gap_diagnostic` / `oracle_blocked`。C11-M1 / C11-M2 队列均为空，closed line 不自动重开。S1 已复核 capability/test/FreeCAD source/current landing evidence 并关闭 source authority blocker。S2 已把其他 Part Workbench 和 Assembly 行准入为 active remaining gap、representative subset、historical narrowed / non-parity 或 non-goal boundary，并把 `implementation_candidate` 保留为 S6-only placeholder。S3 已复审 CopyOnChange：stable native copied-object expected、产品批准 request-local DTO、current cad-core mismatch 三项没有同时成立，`C12M1-SCOPE-101` 继续 retained known gap / oracle blocked，不进入 implementation candidate。S6 只有在某行同时满足 stable native/request-local evidence、current cad-core mismatch 和产品边界时，才允许创建下一轮 implementation package。

## 入口

- 总入口：`6-29-16-26-C12-M1-CADCoreCapabilityImplementationCandidate盘点批次总入口.md`
- 方案：`6-29-16-26-C12-M1-CADCoreCapabilityImplementationCandidate盘点批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 工作步骤

- S0：live 能力基线与候选声明口径冻结（已实现）。
- S1：FreeCAD 源码与 capability 候选矩阵复核（已实现）。
- S2：范围准入与 blocker 矩阵（已实现）。
- S3：CopyOnChange active remaining gap 复审（已实现，retained known gap）。
- S4：代表子集、产品边界和 Assembly 候选复审。
- S5：历史 non-parity / narrowed evidence 复审。
- S6：next-batch 发布闸门与代码授权。

## 当前禁止声明

- 不把 CopyOnChange full temporary-document cache 写成 supported。
- 不把 C11-M1 / C11-M2 的 `notCollected` 或 no-code release gate 写成 backend gap。
- 不把 `covered_representative`、`covered_representative_subset` 或 `narrowed_gaps` 自动升级为 implementation row。
- 不新增 backend session、persistent temporary document、cross-request BREP / TopoDS / NamedShape / ElementMap cache。
- 不用 fixture 名称、bbox、面积、输出排序、adapter repair 或 frontend mock 代替 cad-core 语义。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次 docs/CADCore12.0/README.md
git diff --check
```
