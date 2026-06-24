# C6-M4-S4 AdvancedCombined Auxiliary Transition Tolerance 实现

## 目标

在 S3 located profile 结果上实现 combined auxiliary + located section + transition + tolerance case。S4 不重新定义 auxiliary/tolerance 单独能力；它只处理 combined case 对 Location path 的依赖。

## 输入边界

| 输入 | 语义 |
| --- | --- |
| `AuxiliarySpine` / `AuxiliaryCurvilinear` | 已有 wrapper expected-backed 单项能力；combined 中复用 request-local builder 配置。 |
| `Transition` | `Transformed`、`Right corner`、`Round corner` 均必须维持现有 mapping。 |
| `Tolerance.tol3d/boundTol/tolAngular` | 已有 triple wrapper expected-backed 单项能力；combined 中不可退回 scalar compatibility placeholder。 |
| `SectionOptions[].Location` | 必须消费 S3 located profile path，不能绕开。 |

## 代码落点

- `cad-core/src/part/part_sweep.cpp`：combined metadata、diagnostic priority、status 切换。
- `cad-core/src/part/topo_shape_expansion.cpp`：PipeShell options call order、auxiliary/tolerance/location 同时存在的 build path。
- `cad-core/tests/test_p8_features.py`：新增 c6m4 combined assertions，同时保留 c5m10 known_gap guard。
- `cad-core/fixtures/c6m4`：新增 combined product fixture/expected 和 invalid sibling diagnostics。

## 禁止路径

- 不把 combined 失败归咎于 auxiliary/tolerance 单项能力。
- 不在 Location blocker 未处理时伪造 combined expected。
- 不新增 fixture 名称分支或 bbox/order 特判。
- 不修改 Filling、Loft、Groove。

## 验收标准

通过条件：

- `C6M4-BLK-201` 关闭或明确重路由；若关闭，必须有 combined fixture、expected、focused assertions 和 metadata。
- combined no-location control 仍作为 evidence，不能被误标为 blocker。
- invalid combined siblings 的 diagnostics priority 与当前 c5m10 guard 一致或有 documented product-contract 变更。
- capability remaining gap 删除仍只在 S5/S6 发生。

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
git diff --check -- cad-core docs/CADCore6.0
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest.test_c5m10_part_sweep_combined_advanced_contract_and_diagnostic_priority
```

Focused（实现后）：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest
```
