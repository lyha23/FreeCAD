# P8 GearsBeltJoint S5 Capability 与 unsupported 矩阵专项复审

## 目标

关闭 `GBJ-BLOCK-005` 和 `GBJ-BLOCK-006`：同步 C ABI capabilities、focused tests、既有 P8 AssemblySolver docs / TSV，并保护 remaining unsupported matrix。

## FreeCAD 依据

- Gears / Belt 已有直接 `ASMTGearJoint` mapping。
- RackPinion 依赖 special marker rewrite。
- Screw 依赖 sliding part detection / `swapJCS()`。
- complex Distance geometry 依赖 reference geometry 和 radius extraction，不进入本包。

## scope 表

| scope | 复审内容 |
| --- | --- |
| `GBJ-SCOPE-005` | `supported_joint_matrix` 增加 Gears / Belt，`unsupported_joint_matrix` 移除它们 |
| `GBJ-SCOPE-006` | unsupported matrix 仍包含 RackPinion / Screw |
| `GBJ-SCOPE-007` | complex Distance geometry 仍不被本包发布 |
| `GBJ-SCOPE-008` | nonGoal 不被能力发布稀释 |

## 必须回写的矩阵行

- `GBJ-CAND-009`
- `GBJ-CAND-012`
- `GBJ-BLOCK-005`
- `GBJ-BLOCK-006`

## 验收标准

- `cad-core/src/adapters/c_api/c_api.cpp` 的 `ondselSolverCapabilityJson().covered` 增加 grounded Gears / Belt 覆盖项。
- `supported_joint_matrix` 包含 Gears / Belt。
- `unsupported_joint_matrix` 只保留 RackPinion / Screw。
- `cad-core/tests/test_adapters.py` 和 `cad-core/tests/test_p8_features.py` 同步断言。
- 既有 P8 AssemblySolver 主线的 `P8ASM-SCOPE-007` / backend gap / blocker 行只更新 Gears / Belt 相关内容。
- 检查命令：

```bash
rg -n "supported_joint_matrix|unsupported_joint_matrix|grounded_gears_joint|grounded_belt_joint|Gears|Belt|RackPinion|Screw" cad-core/src/adapters/c_api/c_api.cpp cad-core/tests/test_adapters.py cad-core/tests/test_p8_features.py docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver
```

## 非目标

- 不把 RackPinion / Screw 从 unsupported 移除。
- 不把 Gears / Belt 支持扩展成完整 Assembly transaction lifecycle。
- 不改 unrelated CADCore3.0 阶段文档，除非其 current capability 文案已经和 tests 冲突。
