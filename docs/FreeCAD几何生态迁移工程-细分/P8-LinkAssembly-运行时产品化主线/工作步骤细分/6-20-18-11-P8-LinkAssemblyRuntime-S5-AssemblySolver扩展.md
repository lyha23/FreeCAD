# P8-LinkAssemblyRuntime S5 Assembly Solver 扩展

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
