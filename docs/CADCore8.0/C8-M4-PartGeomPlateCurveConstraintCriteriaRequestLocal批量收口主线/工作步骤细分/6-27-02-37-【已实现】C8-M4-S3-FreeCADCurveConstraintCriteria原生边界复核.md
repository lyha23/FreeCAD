# 【已实现】C8-M4-S3 FreeCAD CurveConstraint Criteria 原生边界复核

## 目标

裁决 FreeCAD native `CurveConstraintPy` 的 G0 / G1 / G2 setter 是否可用。S3 的结论只影响 native parity 与 oracle 采集路线；不得用 native setter blocked 直接否定 `cad-core` request-local DTO 支持。

## live 基线

本步骤已记录：

- `pwd=/home/user/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`340ecaf864`
- `git log -1 --oneline`：`340ecaf864 docs: 完成 C8-M4 S2 scope blocker 分类`
- `git status --short -uall`：无输出，开始工作区干净。
- S3 执行前 C8-M4 队列首项为 `6-27-02-37-C8-M4-S3-FreeCADCurveConstraintCriteria原生边界复核.md`。

## S3 结论

- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp::CurveConstraintPy::PyInit()` 从 `GeometryCurvePy` 的 `Boundary` 构造 wrapped `GeomPlate_CurveConstraint`，并把 `Order`、`NbPts`、`TolDist`、`TolAng`、`TolCurv` 传入底层对象。
- `CurveConstraintPy::G0Criterion(u)` / `G1Criterion(u)` / `G2Criterion(u)` 分别调用 `getGeomPlate_CurveConstraintPtr()->G0Criterion(u)` / `G1Criterion(u)` / `G2Criterion(u)`。这证明 wrapped `GeomPlate_CurveConstraint` 有 criteria read state，但 getter 不能证明 Python setter 支持。
- `CurveConstraintPy::setG0Criterion()` / `setG1Criterion()` / `setG2Criterion()` 当前仍直接 `PyErr_SetString(PyExc_NotImplementedError, "Not yet implemented")` 并返回 `nullptr`。Native CurveConstraint setter parity 因此保持 `native_oracle_blocked`。
- 本机 `/home/user/.local/bin/freecadcmd --version` 返回 `FreeCAD 1.2.0 Revision: 20260519 (Git shallow)`；`freecadcmd -c` 最小命令可执行，`CurveConstraint` 构造探针可返回 `C8M4_CONSTRUCT_OK 0`。
- 同一 FreeCADCmd 环境中，setter-only 和 getter-only 探针均直接返回 `Application unexpectedly terminated`，没有稳定 JSON 结果；该 runtime probe 记为 `environment_probe_blocked`，不当作 setter 语义失败，也不覆盖源码中的 `PyExc_NotImplementedError` 结论。
- `C8M4-ORACLE-101` 回写为 `native_oracle_blocked`；`C8M4-ORACLE-102` 回写为 `environment_probe_blocked`，同时保留 getter read state 的源码证据。
- Native setter blocked 只影响 native parity / oracle route，不否定 S4 对 `cad-core` request-local DTO / parser / OCCT apply gate 的独立评估。

## 已回写的矩阵

- `c8m4_geomplate_criteria_oracle_plan.tsv`
- `c8m4_geomplate_criteria_non_goal_registry.tsv`
- `c8m4_geomplate_criteria_blocker_queue.tsv`

## 复核路径

1. 读取 `CurveConstraintPyImp.cpp::setG0Criterion/setG1Criterion/setG2Criterion`，确认当前实现和异常类型。
2. 读取 getter 和 `PyInit()`，确认底层 `GeomPlate_CurveConstraint` 是否存在 criteria 状态。
3. 如果本机非 sandbox FreeCADCmd 可用，可运行最小 native probe：构造 curve constraint 后调用三个 setter，并记录是否抛 `NotImplementedError`。
4. 如果 FreeCADCmd 因 Qt / processor / environment 失败，只记录 environment blocker，不把它当作 setter semantic failure。
5. 如果 native setter 仍 blocked，`C8M4-ORACLE-101` 路由为 `native_oracle_blocked`，但 S4 继续评估 request-local `cad-core` product contract。

## 输出要求

- 在 `c8m4_geomplate_criteria_oracle_plan.tsv` 回写 native probe 结论。
- 在 `c8m4_geomplate_criteria_non_goal_registry.tsv` 回写 native setter blocked / reopen condition。
- 在 `c8m4_geomplate_criteria_blocker_queue.tsv` 关闭 `C8M4-BLOCKER-301`。

## native probe 参考

如果环境允许，可用现有 FreeCADCmd 触发方式创建临时脚本，最小化验证三个 setter。脚本必须只用于探针，不提交到仓库；如果无法稳定触发 FreeCAD CLI，则回到源码证据。

需要记录：

- FreeCAD version / revision。
- 每个 setter 的实际结果：`passed`、`NotImplementedError`、其它异常或 environment failure。
- 是否能读取 getter 返回值。

## 验收标准

- Native setter 结论明确为 `native_supported`、`native_oracle_blocked` 或 `environment_probe_blocked`。
- 若 native setter blocked，文档明确这不是 `cad-core` request-local implementation 的否决理由。
- 不新增 expected，不修改 FreeCAD upstream，不修改 `cad-core`。

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'setG0Criterion|setG1Criterion|setG2Criterion|getG0Criterion|getG1Criterion|getG2Criterion|PyExc_NotImplementedError|NotImplemented' src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线/矩阵/*.tsv
git diff --check
```

## 非目标

- 不改 FreeCAD upstream。
- 不以普通 Python `__main__` 误判 FreeCADCmd probe。
- 不把 PointConstraint setter probe 当作 CurveConstraint setter probe。
