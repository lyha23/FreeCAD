# C8-M4-S4 cad-core 请求内 criteria 落点与 fixture 准入

## 目标

裁决 `cad-core` 是否可以打开 request-local CurveConstraint criteria implementation gate。S4 不落代码，但必须把 S6 要改的文件、fixture 覆盖和 tests 写清楚。

## 准入问题

1. `GeomPlateCurveConstraintSource` 是否已经有 G0 / G1 / G2 字段。
2. `readCurveConstraints()` 当前是否只因 policy diagnostic 阻断 criteria 字段。
3. `addCurveConstraint()` 是否已把三项 criteria 应用到 OCCT `GeomPlate_CurveConstraint`。
4. source evidence 是否能把 applied criteria 写入 recompute result，供 tests 断言。
5. fixture 是否能覆盖三项 criteria，而不是单字段。
6. 若 native FreeCAD setter blocked，fixture expected 该作为 request-local product contract，而不是 native expected。

## 允许的 S6 代码路径

- 删除或缩小 `readCurveConstraints()` 中对 `G0Criterion` / `G1Criterion` / `G2Criterion` 的 `unsupported_curve_criteria` 阻断。
- 复用已有 `addCurveConstraint()` criteria apply path。
- 增加 request-local fixture，至少覆盖同一 CurveConstraint 中 G0 / G1 / G2 全部字段。
- 增加 focused tests，断言三项 criteria 进入 source evidence，且不再产生 `unsupported_curve_criteria`。
- 更新 capability / diagnostics，使 active narrowed gap 与实际结果一致。

## 禁止的 S6 代码路径

- 不在 adapter 层吞掉 `unsupported_curve_criteria`。
- 不靠 fixture 名或 JSON 输出后处理判定 criteria。
- 不把 native setter blocked 写成 runtime supported。
- 不扩展无关 GeomPlate surface family。

## fixture 准入

至少一组 request-local fixture 必须满足：

- CurveConstraint source curve 可定位。
- 同一 constraint 同时包含 `G0Criterion`、`G1Criterion`、`G2Criterion`。
- recompute result 可检查 criteria source evidence。
- 若新增负例，必须保持 invalid criteria 类型 / 非数值输入仍有显式 diagnostic。

可选扩展：

- Curve2dOnSurface 或 ProjectedCurve 使用同一 parser path 时，可作为第二 fixture；若触发额外 FreeCAD wrapper lifecycle 风险，必须拆分到下一包。

## 验收标准

- `C8M4-BLOCKER-401` 关闭时，`c8m4_geomplate_criteria_backend_gap_classification.tsv` 必须明确 `open_S6_implementation_gate` 或 `no_code_release_gate`。
- 若打开 implementation gate，列出 S6 必改文件、fixtures、tests、capability rows。
- 若不开 gate，说明保留 `unsupported_curve_criteria` 的当前证据。

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'GeomPlateCurveConstraintSource|presentCriterionFields|readCurveConstraints|unsupported_curve_criteria|SetG0Criterion|SetG1Criterion|SetG2Criterion|source_evidence' cad-core/include/cad_core/part/part_geomplate.h cad-core/src/part/part_geomplate.cpp cad-core/tests
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线/矩阵/*.tsv
git diff --check
```

## 非目标

- 不落 C++。
- 不新增 fixtures。
- 不重写 GeomPlate builder。
