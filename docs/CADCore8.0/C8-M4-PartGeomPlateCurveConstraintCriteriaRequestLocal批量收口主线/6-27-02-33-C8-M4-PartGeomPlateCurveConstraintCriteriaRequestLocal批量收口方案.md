# C8-M4 Part GeomPlate CurveConstraint Criteria Request-Local 批量收口方案

## 目标

把 `Part.GeomPlate` 的 CurveConstraint criteria 缺口按同一 DTO / OCCT setter 链路批量收口。实现目标不是单独删除 `curve_constraint_criteria_setters_not_implemented` 字符串，而是用 FreeCAD source、当前 `cad-core` parser / builder、fixtures、tests 和 capability 共同裁决：

- `G0Criterion` / `G1Criterion` / `G2Criterion` 是否能作为 request-local CAD Core 输入被接受。
- FreeCAD native Python setter 仍为 `NotImplemented` 时，capability 该如何表达 native non-parity。
- 当前 `unsupported_curve_criteria` diagnostic 是否应删除、重命名或保留为 native-only blocker。

## 非目标

- 不实现 FreeCAD GUI / TaskPanel。
- 不扩展 `PlateSurface.Curves` wrapper 生命周期。
- 不把 FreeCAD native `CurveConstraintPy` setter 写成已支持，除非 S3 原生探针证明 setter 不再抛 `NotImplementedError`。
- 不引入跨请求 BREP、TopoDS_Shape、NamedShape 或 ElementMap cache。
- 不用 fixture 名、bbox、曲线数量或输出 JSON 后处理推断 criteria 行为。

## 当前证据

- S1 复核确认 `cad-core/include/cad_core/part/part_geomplate.h::GeomPlateCurveConstraintSource` 当前没有 `g0Criterion/g1Criterion/g2Criterion`；这些 optional 字段只存在于 `GeomPlatePointConstraintSource` 和 `GeomPlateSourceEvidence`。
- `cad-core/src/part/part_geomplate.cpp::readCurveConstraints()` 当前检测到 CurveConstraint criteria 字段后发布 `unsupported_curve_criteria`，并在创建 Curve source 前返回。
- S1 复核确认 `cad-core/src/part/part_geomplate.cpp::addCurveConstraint()`、`addCurveOnSurfaceConstraint()` 和 `addCurve2dConstraint()` 当前没有对 CurveConstraint 调用 OCCT `SetG0Criterion` / `SetG1Criterion` / `SetG2Criterion`；`SetG*Criterion` 调用只在 PointConstraint path。
- `cad-core/tests/test_p8_features.py::test_c5m7_part_geomplate_curve_criteria_are_locatable_diagnostics` 当前把 curve criteria 视为可定位 diagnostic。
- `PointConstraintPyImp.cpp` 的 criteria setter 已实现，现有 point criteria expected-backed tests 可作为同族 criteria 证据；但不得直接替代 CurveConstraint setter parity。

## 实施节奏

1. S0 冻结 live baseline、批量范围、状态词典和禁止声明。
2. S1 复核 FreeCAD source authority 与 current `cad-core` coverage。
3. S2 将每个 scope row 分类为 `already_supported`、`request_local_backend_gap_candidate`、`native_oracle_blocked`、`capability_publication_gap`、`non_goal` 或 `split_required`。
4. S3 用 source 和可选 FreeCADCmd probe 裁决 native `CurveConstraintPy` setter 边界。
5. S4 裁决 `cad-core` 是否能打开 request-local implementation gate，并给出 fixture / focused tests 准入标准。
6. S5 同步 capability 和 non-goal publication 口径，避免 active gap 与实际能力不一致。
7. S6 如果打开代码闸门，则落 C++ / fixtures / tests / capability；如果不打开，则必须给出 no-code release gate 证据。

## S6 允许的代码落点

- `cad-core/src/part/part_geomplate.cpp`
- `cad-core/include/cad_core/part/part_geomplate.h`，仅当 S4 证明需要新增 evidence 或结构字段。
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_adapters.py`
- `cad-core/tests/test_diagnostics.py`
- `cad-core/fixtures/c8m4/`，仅当 S4 允许 request-local fixture 固化。

## S6 禁止落点

- `cad-core/src/features/sketch_object.cpp`
- adapter 输出端修剪或诊断后处理。
- 下游 Rust / 前端。
- FreeCAD 上游 `src/`。

## 分层验收

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口主线 docs/CADCore8.0/README.md
git diff --check
```

代码实现短跑，只有 S6 打开 implementation gate 时执行：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_adapters tests.test_diagnostics
```

阶段回归，只有修改 shared GeomPlate builder、collector 或 capability schema 时执行：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters tests.test_diagnostics
```
