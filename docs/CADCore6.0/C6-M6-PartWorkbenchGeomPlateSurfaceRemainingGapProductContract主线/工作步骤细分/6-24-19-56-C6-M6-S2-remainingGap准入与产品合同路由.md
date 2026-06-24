# C6-M6-S2 remainingGap 准入与产品合同路由

## 目标

把 S0/S1 的 4 个 remaining gap 路由为可执行范围。S2 的核心是决定每项能否进入 CAD Core request-local product contract；不能实现的项必须变成更窄的 diagnostic / nativeOracleBlocked / nonGoal，而不是继续保留模糊 gap。

## 准入规则

| 状态 | 准入条件 | 下一步 |
| --- | --- | --- |
| `implementationReady` | CAD Core 已有 DTO/执行路径，FreeCAD source 支持 request-local 语义，fixture 可表达输入输出 | S3 / S4 C++ |
| `nativeOracleBlocked` | FreeCADCmd helper 或 wrapper 仍不能稳定采 expected，但 CAD Core 可保留 source-backed evidence | S3 / S4 收窄 blocker |
| `diagnosticOnly` | FreeCAD wrapper 明确 `NotImplementedError` 或不可执行 | S4 诊断 fixture |
| `nonGoal` | 需要 persistent wrapper、GUI 或 native DocumentObject | S4 / S5 non-goal freeze |
| `releaseGate` | 代码已实现，只待 capability / regression 发布 | S5 / S6 |

## 产物

- 更新 `矩阵/c6m6_geomplate_remaining_gap_scope_review_matrix.tsv`。
- 更新 `矩阵/c6m6_geomplate_remaining_gap_backend_gap_classification.tsv`。
- 更新 `矩阵/c6m6_geomplate_remaining_gap_input_contract_matrix.tsv`。
- 更新 `矩阵/c6m6_geomplate_remaining_gap_oracle_fixture_matrix.tsv`。
- 明确 S3 批次和 S4 批次，不把单个 fixture 当作整个语义批次。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线/矩阵/*.tsv
rg -n 'implementationReady|nativeOracleBlocked|diagnosticOnly|nonGoal|releaseGate|C6M6' docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线
git diff --check -- docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线
```

验收通过后，将本文重命名为 `6-24-19-56-【已实现】C6-M6-S2-remainingGap准入与产品合同路由.md`。

## 非目标

- 不因为字段存在就删除 capability `remaining_gaps`。
- 不采集或改写 expected。
- 不实现 GUI / native DocumentObject / persistent wrapper 生命周期。
