# C9-M4 S0 live 基线与缺 oracle 声明口径冻结

## 目标

冻结 C9-M4 起点：C9-M3 已关闭，当前 Assembly capability 没有 `remaining_gaps`，但 `DistanceType` 仍公开 13 个缺 input / expected 的 `default_or_todo_boundaries`。S0 只做声明口径和矩阵 seed 复核，不采 oracle、不改 code、不改 expected。

## 输入

- `docs/CADCore9.0/README.md`
- `docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次/README.md`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_adapters.py`
- `cad-core/tests/test_p8_features.py`
- `cad-core/fixtures/c3m6`

## 声明口径

| 项 | S0 口径 |
| --- | --- |
| C9-M3 | 已关闭，不重开 `PointCurve`、四条 accepted default rows 或 capability publication。 |
| 缺 oracle default rows | native oracle candidate，仍需 S3-S5 裁决，S0 不写 supported。 |
| default branch | 只有 native expected 与 current mismatch 同时存在，S6 才可写 backendGap / implementation。 |
| capability | 只记录 current publication，不在 S0 改能力状态。 |
| primitive frame / GUI / session | 仍是 non-goal，不进入 C9-M4。 |

## 关闭证据

- live baseline：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=435f3f26b9`，`git log -1 --oneline` 为 `435f3f26b9 feat(cad-core): 关闭 C9-M3 S6 距离类型发布闸门`。
- S0 起始 status：`docs/CADCore9.0/README.md` 修改和未提交的 `docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次/**` seed 文档 / 矩阵 / step 文件；这些是本轮上下文，未发现本步需处理的无关脏区。
- C9-M3 `工作步骤细分` queue 为空，C9-M4 queue 在 S0 执行前首项为本文件。
- current capability：`assembly.remaining_gaps=[]`、`assembly.unsupported_joint_matrix=[]`、`distance_type_extended_geometry.native_expected_count=18`、`deferred_diagnostic_cases=[]`。
- C9-M3 accepted rows 仍是 supported：`PointCurve`、`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other`。
- C9-M4 missing oracle rows 仍公开在 `default_or_todo_boundaries`，共 13 个：`CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus`。`cad-core/fixtures/c3m6` 对这些名称无 input / expected 命中。
- S0 未采集 FreeCAD expected，未修改 `cad-core` C++、fixtures、expected 或 tests，未运行 build / focused tests。
- forbidden claims 已写入 README、总入口、工作步骤总入口和矩阵：缺 oracle rows 不得写 supported/backendGap，不得继承 C9-M3 accepted rows，不得用 fixture-name/bbox/几何排序/adapter 文案/输出修剪隐藏缺口；persistent solver state、cross-request placement cache、non-AssemblyLink primitive frame generalization 仍是 non-goal。

## 必须回写的矩阵行

- `C9M4-BLOCKER-000`
- `C9M4-SCOPE-001`
- `C9M4-BG-701`
- `C9M4-NG-001..006`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
git rev-parse --short=10 HEAD
git log -1 --oneline
git -c core.quotepath=false status --short -uall
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次/工作步骤细分 --format markdown
rg -n 'default_or_todo_boundaries|CylinderCone|ConeCone|ConeTorus|ConeSphere|PointCone|PointTorus|LineSphere|LineCone|LineTorus|CurveCylinder|CurveSphere|CurveCone|CurveTorus|remaining_gaps|unsupported_joint_matrix' docs/CADCore9.0 cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次/矩阵/*.tsv
git diff --check
```

## 验收标准

- README 和矩阵记录 live HEAD、C9-M3 queue-empty 事实、current capability 中 13 个缺 oracle default rows。
- S0 不新增 / 修改 cad-core C++、fixtures、expected 或 tests。
- `notCollected`、`native_oracle_required`、supported、backendGap、non-goal 的声明口径无互相覆盖。

## 非目标

- 不采集 FreeCAD expected。
- 不把缺 oracle rows 转 supported。
- 不执行 cad-core build 或 focused tests。
