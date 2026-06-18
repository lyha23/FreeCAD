# 【已实现】P8 ScrewRackPinionJoint S5 NativeOracle 与 Capability 专项复审

## 目标

关闭 `SRJ-BLOCK-002`、`SRJ-BLOCK-005` 和 `SRJ-BLOCK-006`：为 Screw / RackPinion 增加 native FreeCADCmd expected、focused runtime tests，并同步 C ABI capabilities 与既有 P8 AssemblySolver docs / TSV。

## S5 复核结论

- Screw 已进入 request-local real Ondsel 路径：`cad-core` 读取 scalar `Distance`，在 `solver_joints` 暴露 `pitch`，`makeOndselJointOfType()` 创建 `MbD::ASMTScrewJoint` 并设置 `pitch`。
- Screw / RackPinion 共用 S3 前置：`sliding_part_index != 0` 才可转换；side=2 时只交换本次 solver DTO，不写回前端 `DocumentObject graph`。
- RackPinion 保持 S4 marker rewrite 语义：`pitch_radius=Distance`，rack 侧被放到 solver DTO 的 I 端，并在 Ondsel marker 创建前重写 rack marker placement。
- 新增 grounded Screw / RackPinion c3m6 fixtures 与 FreeCADCmd expected；两个 expected 均包含 `native_solver.return_code=0`、`solver_adapter.mode=real_ondsel_solver` 和对应 `joint_type`。
- C ABI `covered` 增加 `grounded_screw_joint` / `grounded_rackpinion_joint`；`supported_joint_matrix` 增加 `RackPinion` / `Screw`；`unsupported_joint_matrix` 为空。
- 上游 `P8ASM-SCOPE-007` 已同步为 scalar Screw / RackPinion supported，同时保留 complex Distance geometry `notCollected`。

## 代码落点

| 落点 | S5 行为 |
| --- | --- |
| `cad-core/include/cad_core/assembly/joint_solver.h` | `JointConstraint` 新增 request-local `pitch` 字段 |
| `cad-core/src/assembly/joint_solver.cpp` | 引入 `ASMTScrewJoint`，实现 Screw `Distance -> pitch` conversion、sliding convertible gate 和 supported predicate |
| `cad-core/src/assembly/assembly_utils.cpp` | `solver_joints` JSON 输出 `pitch` |
| `cad-core/tools/collect_freecad_expected.py` | 支持 Joint `Placement1` / `Placement2` expected 采集，并在 native expected 中镜像 request-local sliding DTO 顺序和 scalar fields |
| `cad-core/fixtures/c3m6` | 新增 grounded Screw / RackPinion fixtures 与 expected |
| `cad-core/tests/test_p8_features.py` | grounded joint matrix 增加 Screw / RackPinion，并新增 S5 focused test |
| `cad-core/src/adapters/c_api/c_api.cpp`、`cad-core/tests/test_adapters.py` | 同步 C ABI covered / supported / unsupported publication |
| `docs/.../矩阵/*.tsv` 与上游 P8ASM matrix | 关闭 `SRJ-BLOCK-002/005/006`，同步支持边界 |

## FreeCAD 依据

| 语义 | FreeCAD 源码 | 验证点 |
| --- | --- | --- |
| Screw MBD mapping | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `Screw` 先检查 `slidingPartIndex()`，必要时 `swapJCS()`，再创建 `ASMTScrewJoint` 并设置 `pitch=getJointDistance(joint)` |
| RackPinion MBD mapping | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `RackPinion` 创建 `ASMTRackPinionJoint` 并设置 `pitchRadius=getJointDistance(joint)` |
| RackPinion marker path | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJoint()`、`getRackPinionMarkers()` | 只有 RackPinion 走 rack / pinion marker rewrite；Screw 使用普通 marker path |
| Shared sliding | `src/Mod/Assembly/App/AssemblyObject.cpp::slidingPartIndex()` | 同一 Assembly graph 内查找 Slider joint，比较 JCS pitch / roll，返回 1 / 2 / 0 |
| Ondsel fields | `src/3rdParty/OndselSolver/OndselSolver/ASMTScrewJoint.h`、`ASMTRackPinionJoint.h` | 分别暴露 `pitch` / `pitchRadius` |

## scope / blocker 结果

| 项 | S5 结果 |
| --- | --- |
| `SRJ-SCOPE-003` / `SRJ-BLOCK-002` | supported：Screw conversion、`pitch` JSON、supported predicate 与 focused test 已完成 |
| `SRJ-SCOPE-004` | supported：RackPinion S4 runtime evidence 已有，S5 补 native expected 与 capability publication |
| `SRJ-SCOPE-005` / `SRJ-BLOCK-005` | supported：两个 expected 已入库并通过 `/home/user/.local/bin/FreeCADCmd --check` |
| `SRJ-SCOPE-006` / `SRJ-BLOCK-006` | supported：C ABI、tests、本包 TSV 和上游 P8ASM matrix 已同步 |
| `SRJ-SCOPE-007` / `SRJ-BLOCK-007` | 保持 `notCollected` / boundary：不声明 complex Distance geometry |
| `SRJ-SCOPE-008` | 保持 `nonGoal`：不声明 GUI drag/postDrag/Reverse UI/session lifecycle |

## 验收结果

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build --target cad-core cad_core_ffi
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_s5_screw_rackpinion_grounded_fixtures_are_published_supported
```

native oracle gate：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
FREECADCMD=/home/user/.local/bin/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-screw-joint-real-solver.json --check
FREECADCMD=/home/user/.local/bin/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-rackpinion-joint-real-solver.json --check
```

## 非目标

- 不刷新无关 c3m6 expected。
- 不把 current machine OCCT drift 当 expected 修正依据。
- 不实现完整 `DistanceType` geometry、reference element kind 或 radius extraction。
- 不声明 GUI drag / postDrag / Reverse UI lifecycle、persistent MBD session 或 full Assembly transaction。
