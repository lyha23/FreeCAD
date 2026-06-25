# C6-M8 S2 准入路由与 ProjectOnSurface 裁决

## 目标

把 S1 的 candidates 路由成可执行状态。S2 是 C6-M8 的关键裁决点：不能把 ProjectOnSurface active/non-goal overlap 留给后续实现者猜。

## 路由词典

| route | 含义 | 后续 |
| --- | --- | --- |
| `expected_backed_closed` | 已有 FreeCAD expected / focused tests 支撑，发布口径只需保护。 | S4 publication guard |
| `cad_core_product_contract_non_parity` | CAD Core stateless 产品合同成立，但不是 FreeCAD native expected。 | S3 批量实现或补 fixture/test |
| `historical_narrowed_gap` | 有 native-hidden / crash / timeout / notCollected 证据，应保留 delete condition。 | S4 capability/docs |
| `non_goal_frozen` | GUI、session、persistent wrapper、full parity 等不属于 CAD Core。 | S4 移出 active gap |
| `backend_gap_requires_implementation` | 产品合同清楚且 cad-core 未实现。 | S3 code + fixtures + tests |

## 动作

1. 对 `project_on_surface.gui_projection_task_panel` 判定是否只能是 `non_goal_frozen`。
2. 对 `project_on_surface.unverified_advanced_branches` 拆成可验证子项：若只是 GUI / native mapper hidden / broad unverified wording，冻结为 non-goal 或 narrowed boundary；若包含 stateless product candidate，写出同一 DTO/API 批量实现范围。
3. 对 `ruled_surface`、`loft`、`sweep`、`filling`、`geomplate` 复核 `remaining_gaps=[]` 是否与 fixtures、narrowed_gaps、non_goals 一致。
4. 更新 backend classification、blocker queue、non-goal registry 和 validation matrix。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'gui_projection_task_panel|unverified_advanced_branches|remaining_gaps|non_goals|narrowed_gaps|cad_core_product_contract_non_parity' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/矩阵/*.tsv
git diff --check -- docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线
```

## 通过条件

- 每个 active gap / non-goal overlap 都有唯一 route。
- 如果存在 `backend_gap_requires_implementation`，S3 范围必须包含 code、fixtures、focused tests、capability 和 docs。
- 如果全部是发布口径冲突，S3/S4 仍必须更新 adapter assertion 和矩阵，不能只改 README。
- S2 文件名和标题标记为 `【已实现】` 后，队列推进到 S3。

