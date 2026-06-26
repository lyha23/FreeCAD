# 【已实现】C8-M4-S4 cad-core 请求内 criteria 落点与 fixture 准入

## 目标

裁决 `cad-core` 是否可以打开 request-local CurveConstraint criteria implementation gate。S4 不落代码、不新增 fixture，但必须把 S6 要改的文件、fixture 覆盖和 tests 写清楚。

## live 基线

本步骤已记录：

- `pwd=/home/user/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`c2b9038d0e`
- `git log -1 --oneline`：`c2b9038d0e docs: 完成 C8-M4 S3 原生边界复核`
- `git status --short -uall`：无输出，开始工作区干净。
- S4 执行前 C8-M4 队列首项为 `6-27-02-38-C8-M4-S4-cad-core请求内criteria落点与fixture准入.md`。

## live source 结论

- `cad-core/include/cad_core/part/part_geomplate.h::GeomPlateCurveConstraintSource` 当前没有 `g0Criterion` / `g1Criterion` / `g2Criterion` 字段；`GeomPlatePointConstraintSource` 与 `GeomPlateSourceEvidence` 已有 optional G0 / G1 / G2 字段。
- `cad-core/src/part/part_geomplate.cpp::presentCriterionFields()` 会定位 `G0Criterion` / `G1Criterion` / `G2Criterion`；`readCurveConstraints()` 在创建 `GeomPlateCurveConstraintSource` 前发布 `unsupported_curve_criteria` 并返回，因此正例当前不能进入 Curve source。
- `addCurveConstraint()` 当前只构造 3D `GeomPlate_CurveConstraint` 并 `builder.Add()`，没有调用 `SetG0Criterion()` / `SetG1Criterion()` / `SetG2Criterion()`；这些 OCCT setter 当前只在 `addPointConstraint()` path。
- `sourceEvidenceJson()` 已能序列化 `g0_criterion` / `g1_criterion` / `g2_criterion`，S6 可以复用 `GeomPlateSourceEvidence` 作为 result 断言落点。
- `cad-core/fixtures/c5m7/part-geomplate-curve-criteria-diagnostic.json` 与 expected 是现有 diagnostic boundary：它只覆盖 `G0Criterion` 触发 `unsupported_curve_criteria`，不是 request-local 正例 fixture。

## S4 裁决

打开 `open_S6_implementation_gate`。

理由是 G0 / G1 / G2 同属一个 request-local CurveConstraint DTO / parser / 3D curve apply / evidence 边界，且 live source 已给出明确落点；不开 gate 只能继续保留 `unsupported_curve_criteria`，但当前证据显示该 diagnostic 是实现缺口边界，不是不可实现的产品边界。FreeCAD native `CurveConstraintPy` setter blocked 仍保持 `native_oracle_blocked`，但不否定 `cad-core` request-local product contract。

## S6 必改文件

- `cad-core/include/cad_core/part/part_geomplate.h`：给 `GeomPlateCurveConstraintSource` 增加 `g0Criterion` / `g1Criterion` / `g2Criterion` optional 字段，并标注 FreeCAD / OCCT criteria 依据。
- `cad-core/src/part/part_geomplate.cpp`：让 `readCurveConstraints()` 对 Curve criteria 做 finite-number validation，不再让合法数值正例报 `unsupported_curve_criteria`；3D Curve `addCurveConstraint()` path 对 `GeomPlate_CurveConstraint` 调用 `SetG0Criterion()` / `SetG1Criterion()` / `SetG2Criterion()`，并把三项写入 `GeomPlateSourceEvidence`。
- `cad-core/fixtures/c8m4/`：新增 request-local fixture，同一 `CurveConstraints.SubSet[]` 同时覆盖 `G0Criterion` / `G1Criterion` / `G2Criterion`。
- `cad-core/tests/test_p8_features.py`：新增或调整 focused tests，断言 source evidence 中有 `g0_criterion` / `g1_criterion` / `g2_criterion`，且正例不再产生 `unsupported_curve_criteria`；保留 invalid criteria 的显式 diagnostic 负例。
- `cad-core/tests/test_adapters.py`：按 S5 publication 口径同步 capability smoke。
- `cad-core/tests/test_diagnostics.py`：只有 diagnostic vocabulary 或 fixture diagnostics 发生变化时同步。
- `cad-core/src/runtime/capability_contract.cpp`：由 S5/S6 根据本 gate 更新 capability / diagnostics publication，不在 S4 修改。

## fixture 准入

至少一组 request-local fixture 必须满足：

- CurveConstraint source curve 可定位。
- 同一 constraint 同时包含 `G0Criterion`、`G1Criterion`、`G2Criterion`，不得只补单字段。
- recompute result 可检查 criteria source evidence。
- 正例不能再产生 `unsupported_curve_criteria`。
- invalid criteria 类型 / 非 finite-number 输入仍必须有显式 diagnostic，不能被正例路径吞掉。

可选扩展：

- Curve2dOnSurface 或 ProjectedCurve 不作为 S6 必须项；若后续证明共享同一 Curve criteria parser / apply 边界，可另开批次评估，避免把 wrapper lifecycle 或 surface-family 风险混入本轮。

## 已回写的矩阵

- `c8m4_geomplate_criteria_backend_gap_classification.tsv`
- `c8m4_geomplate_criteria_oracle_plan.tsv`
- `c8m4_geomplate_criteria_validation_matrix.tsv`
- `c8m4_geomplate_criteria_blocker_queue.tsv`

## 验收标准

- `C8M4-BLOCKER-401` 关闭为 `open_S6_implementation_gate`。
- S6 必改文件、fixture、tests、capability publication 落点已写清楚。
- 不修改 C++、不新增 fixtures / tests / expected，不运行 build。
- 队列首项前移到 S5。

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'GeomPlateCurveConstraintSource|presentCriterionFields|readCurveConstraints|unsupported_curve_criteria|SetG0Criterion|SetG1Criterion|SetG2Criterion|source_evidence' cad-core/include/cad_core/part/part_geomplate.h cad-core/src/part/part_geomplate.cpp cad-core/tests
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线/矩阵/*.tsv
git diff --check
```

## 非目标

- 不落 C++。
- 不新增 fixtures / tests / expected。
- 不把 native FreeCAD setter blocked 写成 cad-core gate closed。
- 不扩展 wrapper lifecycle、GUI、下游或无关 GeomPlate surface family。
