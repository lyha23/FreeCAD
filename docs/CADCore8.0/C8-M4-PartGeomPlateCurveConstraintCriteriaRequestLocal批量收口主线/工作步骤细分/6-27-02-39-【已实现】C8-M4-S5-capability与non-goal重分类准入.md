# 【已实现】C8-M4-S5 capability 与 non-goal 重分类准入

## 目标

根据 S3/S4 的结论同步 capability、diagnostics、non-goal 和 reopen condition 口径。S5 不落代码，但必须给 S6 明确发布策略。

## live 基线

本步骤已记录：

- `pwd=/home/user/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`3bbc588f8a`
- `git log -1 --oneline`：`3bbc588f8a docs: 完成 C8-M4 S4 criteria 准入裁决`
- `git status --short -uall`：无输出，开始工作区干净。
- S5 执行前 C8-M4 队列首项为 `6-27-02-39-C8-M4-S5-capability与non-goal重分类准入.md`。

## 发布策略

S4 已打开 `open_S6_implementation_gate`，因此 S5 裁决为：

- S6 实现后，`cad-core` request-local CurveConstraint `G0Criterion` / `G1Criterion` / `G2Criterion` 应发布为 `part_workbench.geomplate` 的 product contract / supported capability。
- `curve_constraint_criteria_setters_not_implemented` 不得作为 active `remaining_gaps` 或 request-local runtime blocker。若继续保留，只能作为 FreeCAD native non-parity / narrowed historical evidence，并必须带 delete / reopen condition。
- FreeCAD native setter blocked 只代表 upstream/native parity blocked，不否定 `cad-core` request-local contract。
- 合法 finite-number G0 / G1 / G2 输入不应继续产生 `unsupported_curve_criteria`。
- invalid criteria 类型、缺失数值或非 finite-number 输入应走显式 invalid finite-number diagnostic；`unsupported_curve_criteria` 不再覆盖合法 request-local 正例。

若后续 S6 实现中发现 DTO、parser、OCCT apply 或 fixture 无法同时覆盖 G0 / G1 / G2，必须重新打开 S4/S5 的 gate 结论，而不是把 native setter blocked 重新写成 cad-core active gap。

## 已同步口径

- `c8m4_geomplate_criteria_non_goal_registry.tsv`：每行都有 delete / reopen condition；native setter blocked 行明确为 CAD Core non-goal / native historical evidence，不阻塞 S6 request-local implementation。
- `c8m4_geomplate_criteria_backend_gap_classification.tsv`：S6 publication strategy 已写明 valid criteria supported、invalid criteria explicit diagnostic、native setter historical evidence 三者分离。
- `c8m4_geomplate_criteria_validation_matrix.tsv`：S6 build / focused tests / capability smoke 期望已写明，包含 positive source evidence、capability publication 和 invalid criteria diagnostic。
- `c8m4_geomplate_criteria_blocker_queue.tsv`：关闭 `C8M4-BLOCKER-501`，下一 owner 为 S6。

## non-goal registry 已覆盖

- FreeCAD native `CurveConstraintPy` criteria setter blocked：保留为 upstream/native parity blocked 与 historical evidence，不作为 cad-core active remaining gap。
- GUI / TaskPanel / ViewProvider：保持 non-goal。
- `PlateSurface.Curves` wrapper lifecycle：保持 non-goal。
- 跨请求 BREP / TopoDS_Shape / NamedShape / ElementMap cache：保持 non-goal。
- 下游 Rust / frontend 持久化调整：S6 发布稳定 `cad-core` capability 后可由下游单独消费，但不是 C8-M4 必须完成项。

## capability / tests 需要 S6 同步的点

- `cad-core/src/runtime/capability_contract.cpp` 中 `part_workbench.geomplate` 的 status / covered / narrowed gaps / diagnostics / non_goals：合法 CurveConstraint criteria 支持发布为 product contract；native setter blocked 只保留为 historical/native non-parity evidence。
- `cad-core/tests/test_adapters.py` 中 capability smoke：断言 active `remaining_gaps` 不包含 `curve_constraint_criteria_setters_not_implemented`，request-local criteria 出现在 supported/product contract 口径。
- `cad-core/tests/test_p8_features.py` 中 positive / diagnostic focused tests：正例 source evidence 包含 `g0_criterion` / `g1_criterion` / `g2_criterion` 且无 `unsupported_curve_criteria`；负例保留 invalid finite-number diagnostic。
- `cad-core/tests/test_diagnostics.py` 中 diagnostic vocabulary：只有 fixture diagnostic code 变化时修改，不能把合法 criteria 正例继续列为 `unsupported_curve_criteria`。

## 验收标准

- `C8M4-BLOCKER-501` 关闭。
- `c8m4_geomplate_criteria_non_goal_registry.tsv` 每行有 delete / reopen condition。
- `c8m4_geomplate_criteria_backend_gap_classification.tsv` 与 capability 发布策略一致。
- 下一 pending 是 S6。

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'part_workbench|geomplate|curve_constraint_criteria|unsupported_curve_criteria|narrowed_gaps|non_goals' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py cad-core/tests/test_p8_features.py cad-core/tests/test_diagnostics.py docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线/矩阵/*.tsv
git diff --check
```

## 非目标

- 不在 S5 直接修改 capability C++。
- 不删除 native setter blocked 证据。
- 不把 downstream work 写成本轮必须完成。
- 不新增 fixture / expected / tests，不运行 build。
