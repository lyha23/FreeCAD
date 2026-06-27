# 【已实现】C9-M3 S0 live 基线与 DistanceType 声明口径冻结

## 目标

冻结 C9-M3 起点：C9-M2 已关闭，当前 Assembly capability 没有 `remaining_gaps`，但 `DistanceType` 仍公开 `PointCurve` diagnostic 与 default-or-TODO boundary。S0 只做声明口径和矩阵 seed 复核，不采 oracle、不改 code、不改 expected。

## 输入

- `docs/CADCore9.0/README.md`
- `docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次/README.md`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_adapters.py`
- `cad-core/tests/test_p8_features.py`
- `cad-core/fixtures/c3m6/expected/assembly-distance-*-*.freecad.json`

## S0 live baseline

- 执行目录：`/home/user/Chili3DProject/FreeCAD`。
- S0 起始 HEAD：`04bdd2e561`（`04bdd2e561 docs: 新增 C9-M3 DistanceType default boundary 方案`），起始 `git -c core.quotepath=false status --short -uall` 无输出。
- C9-M2 关闭 handoff：`b981e84f68 feat(cad-core): 关闭C9-M2 S6 oracle发布闸门`；C9-M2 `step_goal_queue.py` 只输出表头，无 pending 行。
- current capability / adapter guard：`assembly.remaining_gaps=[]`、`assembly.unsupported_joint_matrix=[]`、`distance_type_extended_geometry.deferred_diagnostic_cases=["PointCurve"]`，`default_or_todo_boundaries` 仍包含 `PlaneCone`、`CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineCylinder`、`LineSphere`、`LineCone`、`LineTorus`、`CurvePlane`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus`、`Other`。
- checked-in expected 库存：`assembly-distance-point-curve-real-solver`、`assembly-distance-plane-cone-default-boundary`、`assembly-distance-line-cylinder-default-boundary`、`assembly-distance-curve-plane-default-boundary`、`assembly-distance-other-default-boundary` 均仍带 `DTE-NG-003` / known-gap diagnostic metadata；S0 未采集或修改 expected。

## 声明口径

| 项 | S0 口径 |
| --- | --- |
| C9-M2 | 已关闭，不重开 marker / offsetPlc / writeback / zero Angle。 |
| `PointCurve` | native expected candidate，仍需 S3 裁决，S0 不写 supported。 |
| default branch | oracle batch candidate，S4 才决定支持、backendGap 或 retained diagnostic。 |
| capability | 只记录 current publication，不在 S0 改能力状态。 |
| primitive frame | 仍是 non-goal，不进入 C9-M3。 |

## 必须回写的矩阵行

- `C9M3-BLOCKER-000`
- `C9M3-SCOPE-001`
- `C9M3-BG-501`
- `C9M3-NG-001..005`

## S0 关闭结论

- `C9M3-BLOCKER-000` 与 `C9M3-SCOPE-001` 已关闭为 baseline / claim freeze。
- `C9M3-BG-501` 只记录 S0 baseline 证据，仍是 S6 release gate，不能在 S0 关闭。
- `C9M3-NG-001..005` 已按 S0 冻结为 non-goal / forbidden claim；S2 只复核守卫，不把它们提升为 supported 或 backendGap。
- S1-S6 保持 pending；S0 未修改 cad-core C++、fixtures、expected 或 tests。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
git rev-parse --short=10 HEAD
git log -1 --oneline
git -c core.quotepath=false status --short -uall
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次/工作步骤细分 --format markdown
rg -n 'PointCurve|default_or_todo_boundaries|unsupported_joint_matrix|remaining_gaps' docs/CADCore9.0 cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py cad-core/tests/test_p8_features.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次/矩阵/*.tsv
git diff --check
```

## 验收标准

- README 和矩阵记录 live HEAD、C9-M2 queue-empty 事实、current capability 中 `PointCurve` / `default_or_todo_boundaries` 的原始状态。
- S0 不新增 / 修改 cad-core C++、fixtures、expected 或 tests。
- `PointCurve`、default branch、primitive frame、GUI/session、persistent solver 的声明口径无互相覆盖。

## 非目标

- 不采集 FreeCAD expected。
- 不把 diagnostic expected 转 supported。
- 不执行 cad-core build 或 focused tests。
