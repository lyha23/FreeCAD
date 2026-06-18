# P8 GearsBeltJoint S4 NativeOracle 与半径符号专项复审

## 目标

关闭 `GBJ-BLOCK-003` 和 `GBJ-BLOCK-004`：为 Gears / Belt 增加 native FreeCADCmd expected，并证明 cad-core request-local solver output 与 expected 一致。

## FreeCAD 依据

- `AssemblyObject::solve()`：`mbdAssembly->runPreDrag()` 后 `setNewPlacements()`。
- `makeMbdJointOfType()`：Gears / Belt 直接创建 `ASMTGearJoint`。
- `makeMbdJoint()`：Gears / Belt 使用通用 Reference1 / Reference2 marker path。

## scope 表

| scope | 复审内容 |
| --- | --- |
| `GBJ-SCOPE-004` | 新增 `assembly-grounded-gears-joint-real-solver` 和 `assembly-grounded-belt-joint-real-solver` native expected |
| `GBJ-SCOPE-002` | Gears fixture 的 `solver_joints` / 半径字段 / placement updates 与 FreeCAD expected 对齐 |
| `GBJ-SCOPE-003` | Belt fixture 的 `radiusJ` 负号和 expected parity 对齐 |

## 必须回写的矩阵行

- `GBJ-CAND-010`
- `GBJ-CAND-011`
- `GBJ-BLOCK-003`
- `GBJ-BLOCK-004`

## 验收标准

- `cad-core/fixtures/c3m6/assembly-grounded-gears-joint-real-solver.json` 和 expected 入库。
- `cad-core/fixtures/c3m6/assembly-grounded-belt-joint-real-solver.json` 和 expected 入库。
- expected 文件包含 `native_solver.return_code=0`、`solver_adapter.mode=real_ondsel_solver` 和对应 `joint_type`。
- focused test 断言 Gears / Belt 进入 `solver_joints`，`unsupported_joints=[]`，且 Belt `radiusJ` 为负值。
- 检查命令：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
FREECADCMD=${FREECADCMD:-FreeCADCmd} python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-gears-joint-real-solver.json --check
FREECADCMD=${FREECADCMD:-FreeCADCmd} python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-belt-joint-real-solver.json --check
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver
```

## 非目标

- 不刷新无关 c3m6 expected。
- 不把 current machine OCCT drift 当 expected 修正依据。
- 不声明 GUI reverse / rotate lifecycle。
