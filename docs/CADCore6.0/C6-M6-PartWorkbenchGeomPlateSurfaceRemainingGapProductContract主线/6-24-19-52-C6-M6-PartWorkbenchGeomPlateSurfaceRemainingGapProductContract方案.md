# C6-M6 Part Workbench GeomPlateSurface Remaining Gap Product Contract 方案

## 背景

C6-M5 已把 Filling 的 Surface、SupportOrder、ExplicitParams 与 non-boundary support/order product contract 发布完成。下一批应进入 `part_workbench.geomplate`，因为当前 capability 仍保留 4 个 active `remaining_gaps`，且它们都归属 `Part.GeomPlate.BuildPlateSurface` / `Part::GeomPlateSurface` helper 的同一能力面。

## 本轮做什么

- S0：复核 live capability、C6-M1 到 C6-M5 队列状态和旧 C5-M7 / C5-M13 GeomPlate 结论，冻结 4 个 active `remaining_gaps`。
- S1：复核 FreeCAD source 与 wrapper oracle 候选，明确哪些能力来自 `BuildPlateSurface`、`CurveConstraint`、`PlateSurface` 或 `Geometry` wrapper。
- S2：把每个 gap 路由成 `implementationReady`、`diagnosticOnly`、`nativeOracleBlocked`、`nonGoal` 或 `releaseGate`；不能凭 C++ API 名称直接删除 blocker。
- S3：批量处理 G1 curve-on-surface 与无 InitialSurface 的 ProjectedCurve2d。若能形成 request-local product contract，补 fixtures / tests / capability evidence；否则保留更窄 diagnostics 和 delete condition。
- S4：批量处理 curve criteria setter 与 `Part.PlateSurface.Curves` wrapper lifecycle。只在 FreeCAD/runtime 证据支持时进入产品合同，否则冻结为 explicit non-goal 或 diagnostic boundary。
- S5：发布 fixtures、focused tests、capability、docs 与矩阵；只删除已有代码、fixture、focused tests 和 capability 同步证明的 `remaining_gaps`。
- S6：执行阶段回归和 heavy release gate；不再引入新语义，只审计发布状态。

## 关键边界

- `Part::GeomPlateSurface` 仍是 CAD Core source-backed geometry helper request type，不声明 FreeCAD parity。
- 不创建 GUI GeomPlate feature、不伪造原生 `Part::GeomPlate` DocumentObject、不引入 persistent `PlateSurface` wrapper state。
- 不把 `Filling`、full Part surface family 或 upstream FreeCAD source 修复混入本包。
- 不用 fixture 名称、bbox、面积、几何类型猜测或 adapter 层修补来关闭 gap。
- FreeCADCmd wrapper 的 `NotImplementedError`、`RuntimeError: Geom_RectangularTrimmedSurface::V1==V2`、SIGSEGV 等证据必须原样进入矩阵，不能被写成实现完成。

## 代码落点

| 方向 | 文件 |
| --- | --- |
| DTO / executor | `cad-core/include/cad_core/part/part_geomplate.h`、`cad-core/src/part/part_geomplate.cpp` |
| fixture / expected collector | `cad-core/tools/collect_freecad_expected.py` |
| capability | `cad-core/src/runtime/capability_contract.cpp` |
| tests | `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_expected_fixtures.py`、`cad-core/tests/test_adapters.py` |
| fixture | `cad-core/fixtures/c6m6` |

## FreeCAD 依据

| 方向 | FreeCAD source |
| --- | --- |
| BuildPlateSurface helper | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp::makeSurface()` |
| Python wrapper | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp` |
| curve constraints | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp` |
| point constraints | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/PointConstraintPyImp.cpp` |
| PlateSurface wrapper | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PlateSurfacePyImp.cpp` |
| result wrapper | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp` |

## 验收分层

- 本轮短跑：本步骤相关 `rg`、focused unittest、TSV 字段数检查、`git diff --check`。
- 阶段回归：`cmake --build build` 加 GeomPlate / expected / adapter focused suites。
- 重型收口：S6 或发布前补跑 P8 / expected / adapter / topology 相关 suites；不能把历史 known_gap 的存在当作失败。

## 结论

推荐立即进入 C6-M6。这个包不追求完整 GeomPlate parity，而是把当前 4 个明确 `remaining_gaps` 分批转成 CAD Core product contract、diagnostic boundary 或 non-goal freeze，并用 capability、fixtures、tests 和矩阵同步发布。
