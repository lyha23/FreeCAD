# C5-M12 Part Workbench Surface Native Oracle Recovery 主线

C5-M12 承接 C5-M6 到 C5-M11 关闭后仍留在 Part Workbench surface family 里的精确 oracle / expected blockers。它不是新增产品面，也不是重开 C5 broad deferred；本包只处理已有 cad-core source-backed 或 diagnostic-backed surface helper / wrapper 能力中，因 FreeCADCmd native helper / wrapper oracle 尚未稳定而无法晋级 expected-backed 的代表场景。

## 目标

- 统一冻结 `part_workbench.loft`、`part_workbench.sweep`、`part_workbench.filling`、`part_workbench.geomplate` 的剩余 native oracle / expected blockers。
- 批量探测 FreeCADCmd native helper / wrapper 能否为当前 source-backed 场景提供稳定 `shape_summary`、`object_fields`、wrapper/helper metadata 或明确失败证据。
- 对 collectable 场景替换 expected，补 cad-core 实现或 collector schema，补 fixtures、focused tests、capability/docs。
- 对仍不可采集的场景保留精确 narrowed blocker，写清 FreeCAD / OCCT 错误、未采字段、下一批范围和不影响已收集 expected 的边界。

## 入口文件

- 方案：`6-22-02-02-C5-M12-PartWorkbenchSurfaceNativeOracleRecovery方案.md`
- scope 矩阵：`矩阵/c5m12_surface_native_oracle_recovery_scope.tsv`
- source / DTO / oracle 合同：`矩阵/c5m12_surface_native_oracle_recovery_source_dto_oracle_contract.tsv`
- fixture / oracle 矩阵：`矩阵/c5m12_surface_native_oracle_recovery_fixture_oracle_matrix.tsv`
- blocker 队列：`矩阵/c5m12_surface_native_oracle_recovery_blocker_queue.tsv`
- non-goal registry：`矩阵/c5m12_surface_native_oracle_recovery_non_goal_registry.tsv`
- validation 矩阵：`矩阵/c5m12_surface_native_oracle_recovery_validation_matrix.tsv`
- 工作步骤：`工作步骤细分/`

## 最小完整语义批次

| 批次 | 代表场景 | 产物 |
| --- | --- | --- |
| live gap guard | C5-M6/M7/M8/M11 当前 capability gap 与 checked-in expected | S0 冻结真实 remaining blockers、非目标和批量边界 |
| oracle probe matrix | FreeCADCmd wrapper/helper probes for Sweep, Loft, Filling, GeomPlate | S1 批量采集 probe 证据，确定 collectable / blocker 分流 |
| Sweep wrapper recovery | support valid representative, located profile, advanced combined | S2 收窄或替换 C5-M11 Sweep wrapper blockers |
| Loft complex profile | wire / face / vertex / whole sketch object profile family；sketch subelement native-hidden boundary | S3 已为 `part_workbench.loft.complex_profile_family` 代表 profile 建 expected / diagnostics |
| Filling + GeomPlate native oracle | Filling support/order/G2/params/non-boundary edge, GeomPlate G1/projected native oracle | S4 native helper expected 或 source-backed blocker 更新 |
| capability docs closeout | expected, tests, capability, C3/C5 docs | S5 收口 root/package matrices 和 capability wording |

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线/工作步骤细分 --format markdown
```

## 非目标

- 不声明完整 Part surface family。
- 不新增 GUI / TaskPanel / ViewProvider 能力。
- 不把 helper / wrapper expected 当成 upstream native DocumentObject executor。
- 不用 cad-core recompute 输出、bbox 或 fixture 名称反推 FreeCAD expected。
- 不把 C5.1 PartDesign exact blockers 重新混入 Part Workbench surface package。
