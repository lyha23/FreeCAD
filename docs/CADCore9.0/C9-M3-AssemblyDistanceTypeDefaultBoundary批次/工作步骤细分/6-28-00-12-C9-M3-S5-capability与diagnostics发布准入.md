# C9-M3 S5 capability 与 diagnostics 发布准入

## 目标

在 S3/S4 裁决后，审查 capability 和 diagnostics 发布是否诚实：accepted DistanceType 必须出现在 supported / request-local boundary 中；未采集或未接受的 DistanceType 必须继续以 default/todo、diagnostic 或 non-goal 形式可见。

## 检查面

| 面 | 文件 | 检查点 |
| --- | --- | --- |
| capability | `cad-core/src/runtime/capability_contract.cpp` | `distance_type_extended_geometry.supported`、`default_or_todo_boundaries`、`deferred_diagnostic_cases`、`non_goals`。 |
| adapter tests | `cad-core/tests/test_adapters.py` | supported 列表、PointCurve / default branch non-goal 删除或保留、remaining gaps。 |
| runtime diagnostics | `cad-core/tests/test_p8_features.py` | unsupported JointType、missing grounded、missing marker、not collected default boundary 仍有 diagnostics。 |
| expected metadata | `cad-core/fixtures/c3m6/expected` | accepted expected 不再携带 stale `DTE-NG-003`，retained diagnostic expected 保留 delete condition。 |

## 必须回写的矩阵行

- `C9M3-SCOPE-301..304`
- `C9M3-BLOCKER-501`
- `C9M3-BG-301..501`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'distance_type_extended_geometry|PointCurve|default_or_todo_boundaries|deferred_diagnostic_cases|default_or_todo_branch_support|unsupported_assembly_solver|missing_grounded_part|invalid_assembly_solver_result' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py cad-core/tests/test_p8_features.py cad-core/fixtures/c3m6 docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次/矩阵/*.tsv
git diff --check
```

## 验收标准

- S5 明确哪些 capability 字段由 S6 改，哪些保持 diagnostic/non-goal。
- `default_or_todo_branch_support` 不得在未实现 / 未采 expected 前从 non-goals 消失。
- diagnostics guard 不因 `PointCurve` 或 default branch 支持而静默消失。
- 若 capability 只需发布同步，必须说明没有 C++ backend gap 的证据。

## 非目标

- 不在 adapter 层伪造支持状态。
- 不把 `known_gaps=[]` 当成隐藏 diagnostic 的理由。
- 不把 missing oracle case 写成 supported。
