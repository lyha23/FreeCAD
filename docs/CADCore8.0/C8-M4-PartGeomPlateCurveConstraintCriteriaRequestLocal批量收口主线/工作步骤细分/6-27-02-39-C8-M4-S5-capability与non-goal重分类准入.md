# C8-M4-S5 capability 与 non-goal 重分类准入

## 目标

根据 S3/S4 的结论同步 capability、diagnostics、non-goal 和 reopen condition 口径。S5 不落代码，但必须给 S6 明确发布策略。

## 发布策略

如果 S4 打开 request-local implementation gate：

- `curve_constraint_criteria_setters_not_implemented` 不应继续作为 active `remaining_gaps`。
- capability 应表达 `cad-core` request-local criteria supported 或 product contract supported。
- native FreeCAD setter blocked 应转为 native non-parity / reopen condition，不应混入 request-local runtime active gap。
- `unsupported_curve_criteria` 应删除、缩小为 invalid input diagnostic，或改为 native-only diagnostic。

如果 S4 不打开 implementation gate：

- 保留 `unsupported_curve_criteria`。
- `curve_constraint_criteria_setters_not_implemented` 保持 narrowed gap。
- 必须写清 blockers：DTO 不足、OCCT apply 不足、fixture 不足、native oracle 不能替代 request-local contract，或其它具体原因。

## non-goal registry 必须覆盖

- FreeCAD native `CurveConstraintPy` criteria setter blocked。
- GUI / TaskPanel / ViewProvider。
- `PlateSurface.Curves` wrapper lifecycle。
- 跨请求 BREP / TopoDS_Shape / NamedShape / ElementMap cache。
- 下游 Rust / frontend 持久化调整。

## capability / tests 需要 S6 同步的点

- `cad-core/src/runtime/capability_contract.cpp` 中 `part_workbench.geomplate` 的 status / narrowed gaps / diagnostics / non-goals。
- `cad-core/tests/test_adapters.py` 中 capability smoke。
- `cad-core/tests/test_p8_features.py` 中 positive / diagnostic focused tests。
- `cad-core/tests/test_diagnostics.py` 中 diagnostic vocabulary，只有 diagnostic 发生变化时修改。

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
