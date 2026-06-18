# 【已实现】P8 ParallelPerpendicularJoint S4 NativeOracle 与 placement 写回专项复审

## 目标

关闭 `PPJ-BLOCK-002` 和 `PPJ-BLOCK-003`：为 Parallel / Perpendicular 增加 native FreeCADCmd expected，并证明 cad-core request-local writeback 与 expected 一致。

## FreeCAD 依据

- `AssemblyObject::solve()`：`mbdAssembly->runPreDrag()` 后 `setNewPlacements()`。
- `makeMbdJointOfType()`：Parallel / Perpendicular 直接创建对应 ASMT joint。
- `makeMbdJoint()`：通用 Reference1 / Reference2 marker path。

## scope 表

| scope | 复审结果 |
| --- | --- |
| `PPJ-SCOPE-004` | 已新增 `assembly-grounded-parallel-joint-real-solver` 和 `assembly-grounded-perpendicular-joint-real-solver` fixture / native expected；两个 expected 均由 `FreeCADCmd 1.2.0 revision 20260519` collector 生成并通过 `--check` |
| `PPJ-SCOPE-002` | Parallel expected 包含 `native_solver.return_code=0`、`solver_adapter.mode=real_ondsel_solver`、`solver_joints[0].joint_type=Parallel`、`unsupported_joints=[]`、`placement_updates=[]` |
| `PPJ-SCOPE-003` | Perpendicular expected 包含 `native_solver.return_code=0`、`solver_adapter.mode=real_ondsel_solver`、`solver_joints[0].joint_type=Perpendicular`、`unsupported_joints=[]`、`placement_updates=[]` |

## 产物

| 类型 | 路径 |
| --- | --- |
| Parallel fixture | `cad-core/fixtures/c3m6/assembly-grounded-parallel-joint-real-solver.json` |
| Parallel expected | `cad-core/fixtures/c3m6/expected/assembly-grounded-parallel-joint-real-solver.freecad.json` |
| Perpendicular fixture | `cad-core/fixtures/c3m6/assembly-grounded-perpendicular-joint-real-solver.json` |
| Perpendicular expected | `cad-core/fixtures/c3m6/expected/assembly-grounded-perpendicular-joint-real-solver.freecad.json` |
| Focused test | `cad-core/tests/test_p8_features.py::CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver` |

## fixture 设计结论

- Parallel 复用已有 c3m6 grounded two-component object-level reference 结构；native oracle 和 cad-core runtime 都保持无 placement writeback。
- Perpendicular 不依赖 GUI `preventParallel()`；fixture 通过 `ComponentB.Placement` 的 90 度 Y 轴旋转，让默认 JCS 轴线在请求数据中满足 perpendicular 关系。
- 当前 collector 对 Joint `Placement1/Placement2` 这类非对象 `Placement` 属性没有通用写入路径；本步不改 collector，也不手写 expected。
- 两个 expected 均无 `known_gap`，也没有 fixture-name special casing。

## 必须回写的矩阵行

- `PPJ-CAND-008`：focused grounded matrix 已覆盖 Parallel / Perpendicular。
- `PPJ-CAND-009`：c3m6 fixture / expected 已入库并通过 collector `--check`。
- `PPJ-SCOPE-004`：native expected 从 `notCollected` 转为 S4 已关闭。
- `PPJ-BLOCK-002`：Parallel native expected 关闭。
- `PPJ-BLOCK-003`：Perpendicular native expected 关闭。

## 验收结果

- 通过：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
FREECADCMD=FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-parallel-joint-real-solver.json --check
FREECADCMD=FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-perpendicular-joint-real-solver.json --check
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver
```

- 组合验收命令中的 `CadCoreExpectedFixtureTest` 已运行；S4 新增 c3m6 expected 参与执行且未 skip，但完整用例当前在既有 `p7/hole-supported-model-thread-counterbore` 上失败：volume `434.05358560604475` 与 expected `434.05359569539525` 差 `1.0089350496400584e-05`，略超该 fixture 的 `1e-05` delta。该失败不属于本包允许产物，未刷新无关 expected。

## 非目标

- 不刷新无关 c3m6 expected。
- 不把 current machine OCCT drift 当 expected 修正依据。
- 不声明 GUI forbidden / preventParallel lifecycle。
- 不修改 C ABI capability 或 `tests/test_adapters.py`，留给 S5。
