# P8 ScrewRackPinionJoint S5 NativeOracle 与 Capability 专项复审

## 目标

关闭 `SRJ-BLOCK-002`、`SRJ-BLOCK-005` 和 `SRJ-BLOCK-006`：为 Screw / RackPinion 增加 native FreeCADCmd expected、focused runtime tests，并同步 C ABI capabilities 与既有 P8 AssemblySolver docs / TSV。

## FreeCAD 依据

- `AssemblyObject::solve()`：`mbdAssembly->runPreDrag()` 后 `setNewPlacements()`。
- `makeMbdJointOfType()`：Screw / RackPinion 直接创建对应 ASMT joint，但都依赖 shared sliding 前置。
- `makeMbdJoint()`：RackPinion 使用特殊 marker path；Screw 使用普通 marker path 但可能经历 `swapJCS()`。

## scope 表

| scope | 复审内容 |
| --- | --- |
| `SRJ-SCOPE-003` | Screw fixture 的 `solver_joints` / `pitch` / placement updates 与 FreeCAD expected 对齐 |
| `SRJ-SCOPE-004` | RackPinion fixture 的 `pitch_radius` / marker rewrite / expected parity 对齐 |
| `SRJ-SCOPE-005` | 新增 `assembly-grounded-screw-joint-real-solver` 和 `assembly-grounded-rackpinion-joint-real-solver` native expected |
| `SRJ-SCOPE-006` | `supported_joint_matrix` 增加 Screw / RackPinion，`unsupported_joint_matrix` 清空或按剩余 nonGoal 规则发布 |

## 必须回写的矩阵行

- `SRJ-CAND-010`
- `SRJ-CAND-012`
- `SRJ-CAND-013`
- `SRJ-BLOCK-002`
- `SRJ-BLOCK-005`
- `SRJ-BLOCK-006`

## 验收标准

- `cad-core/fixtures/c3m6/assembly-grounded-screw-joint-real-solver.json` 和 expected 入库。
- `cad-core/fixtures/c3m6/assembly-grounded-rackpinion-joint-real-solver.json` 和 expected 入库。
- expected 文件包含 `native_solver.return_code=0`、`solver_adapter.mode=real_ondsel_solver` 和对应 `joint_type`。
- focused test 断言 Screw / RackPinion 进入 `solver_joints`，`unsupported_joints=[]`，并锁定 `pitch` / `pitch_radius` 与 shared sliding evidence。
- `cad-core/src/adapters/c_api/c_api.cpp` 的 `ondselSolverCapabilityJson().covered` 增加 grounded Screw / RackPinion 覆盖项。
- `cad-core/tests/test_adapters.py` 和既有 P8 AssemblySolver 矩阵同步。
- 检查命令：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
FREECADCMD=${FREECADCMD:-FreeCADCmd} python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-screw-joint-real-solver.json --check
FREECADCMD=${FREECADCMD:-FreeCADCmd} python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-rackpinion-joint-real-solver.json --check
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

## 非目标

- 不刷新无关 c3m6 expected。
- 不把 current machine OCCT drift 当 expected 修正依据。
- 不声明 GUI drag / postDrag / Reverse UI lifecycle。
