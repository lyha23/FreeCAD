# 【已实现】P8-LinkAssemblyRuntime S5 Assembly Solver 扩展

## 目标

在 S2-S4 稳定的 Link graph、subshape reference 和 placement chain 上，扩展 Assembly solver 剩余 JointType、完整 Joint placement / constraint、underconstrained / contradictory diagnostics 和 placement writeback stress。

## 必读

- S0-S4 的已实现结论和矩阵。
- `src/Mod/Assembly/App/AssemblyObject.cpp`
- `src/Mod/Assembly/App/AssemblyUtils.cpp`
- `src/Mod/Assembly/App/AssemblyLink.cpp`
- `src/Mod/Assembly/App/JointGroup.cpp`
- `src/Mod/Assembly/JointObject.py`
- `cad-core/include/cad_core/assembly/joint_solver.h`
- `cad-core/src/assembly/joint_solver.cpp`
- `cad-core/fixtures/c3m6/assembly-*.json`
- `cad-core/tests/test_p8_features.py`

## 实现要求

- 只发布 FreeCAD / Ondsel 路径明确、oracle 可采、前端产品需要的 JointType。
- representative fallback 必须继续标记为 fallback，不得声明 full solver。
- placement writeback 必须证明应用到下一次 request graph 后 no-op 稳定。
- non-grounded、underconstrained、contradictory、partial failure 必须有稳定 diagnostics。

## 非目标

- 不实现跨请求 solver session。
- 不迁移 GUI Assembly commands、drag UI 或 ViewProvider。
- 不绕过 Link graph，直接按当前 fixture object order 写 placement。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_adapters tests.test_expected_fixtures
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线 cad-core
```

## S5 审计结论

S5 未发现新的产品必需、FreeCAD/Ondsel 路径清楚、且已具备 checked-in oracle 的 Assembly solver 实现缺口。本轮不修改 `cad-core` 代码、fixtures 或测试，只把 S2-S4 Link graph 稳定后的 Assembly solver 状态关闭为回归基线加 future oracleFirst gate。

当前可发布的 Assembly solver 能力已经由 live capability 和 tests 约束：

- `cad-core/src/adapters/c_api/c_api.cpp::capabilitiesJson()` 的 `assembly.supported_joint_matrix` 覆盖 13 个 FreeCAD JointType：`Fixed`、`Revolute`、`Cylindrical`、`Slider`、`Ball`、`Distance`、`Parallel`、`Perpendicular`、`Angle`、`Gears`、`Belt`、`RackPinion`、`Screw`；`unsupported_joint_matrix` 为空。
- `cad-core/fixtures/c3m6/expected/assembly-*.freecad.json` 共有 50 个 Assembly expected，其中 45 个是 active assertion，5 个保留 `known_gap`：`CurvePlane`、`LineCylinder`、`Other`、`PlaneCone` 和 `PointCurve`。这些 default/TODO 或 PointCurve 分支只能作为 future oracleFirst gate，不能由 S5 发布为 supported。
- `cad-core/tests/test_p8_features.py` 已覆盖 real Ondsel adapter、Screw/RackPinion sliding precondition、RackPinion marker rewrite、subshape marker placement、multi-component writeback、partial writeback、unsupported RackPinion diagnostic、missing grounded diagnostic、PointCurve unsupported diagnostic，以及 `documentObjectUpdates.action=assembly_set_placement` 应用到下一次 request graph 后 no-op。
- `cad-core/tests/test_adapters.py::test_c_api_capabilities_exposes_web_contract_facts` 已锁定 representative fallback 为 `fallback_metadata_only` 且 `available=false`，同时锁定 `placement_writeback.status=covered_full`、`remaining_gaps=[]`、supported / unsupported joint matrix 和 diagnostic code publication。

S5 不声明新的 underconstrained / contradictory solver 语义。当前稳定诊断发布口径仅限 checked fixtures/tests 已证明的边界：`missing_grounded_part`、`unsupported_assembly_solver`、`ondsel_solver_failed`、`invalid_assembly_solver_result`、PointCurve/default DistanceType future gate，以及 partial writeback / apply-next-request no-op。若后续产品要支持新的 JointType、default/TODO DistanceType、PointCurve 或更完整 underconstrained / contradictory 诊断，必须重新走 FreeCAD/Ondsel source authority、checked-in oracle、fixture、focused tests、capability docs 和 fallback 边界同步，不得直接续跑旧 AssemblySolver / MarkerPlacement / DistanceType 队列。

S6 仍是本主线唯一剩余 blocker：冻结 Worker / WASM / Web runtime 合同和最终 capability 发布口径。
