# C6-M4-S3 ProfilePlacement 与 LocationOverload 实现

## 目标

消费 S2 的 located profile 合同，实现或明确保留 `SectionOptions[].Location` / profile placement 的 CAD Core product path。S3 的输出不能伪装成 FreeCAD expected；如果仍依赖失败的 OCCT Location overload，则必须保持 `known_gap`。

## 代码落点

| 落点 | 任务 |
| --- | --- |
| `cad-core/include/cad_core/part/topo_shape_expansion.h` | 若需要新增 product-contract profile placement DTO，在 public boundary 标注 FreeCAD wrapper authority。 |
| `cad-core/src/part/topo_shape_expansion.cpp` | 实现 located profile product path；禁止输出端 bbox/order fixup。 |
| `cad-core/src/part/part_sweep.cpp` | 接入 `SectionOptions` metadata、diagnostics、known_gap/product status 切换。 |
| `cad-core/tests/test_p8_features.py` | 保留 c5m10 guard，新增 c6m4 focused assertions。 |
| `cad-core/fixtures/c6m4` | 新增 located profile product fixtures 和 expected。 |

## 实现纪律

- 先证明 profile placement 语义，再切换 executor。
- 如果采用 CAD Core product contract，response 必须写 `contract=cad_core_product_contract`，并保留 `freecadcmd_location_overload_status=notCollected` 或等价 metadata。
- 如果复用 OCCT `Add(profile, vertex, ...)` 仍触发 `NCollection_Array1::Value`，不得把错误吞掉后输出 shape。
- invalid location / bool diagnostics 必须先于 product build。
- 保留 request-local，无 persistent wrapper object、无 BREP 跨请求状态。

## 必测场景

| 场景 | 期望 |
| --- | --- |
| valid located profile + contact/correction | 输出 product-contract shape、metadata、NamedShape history；或明确保留 backendGap。 |
| location target missing | locatable `missing_link_target`。 |
| location subname invalid | locatable `invalid_subshape`。 |
| location resolves non-vertex | locatable `invalid_subshape`。 |
| `WithContact/WithCorrection` 非 bool | locatable `invalid_parameter`。 |

## 验收标准

通过条件：

- `C6M4-BLK-102` 被 S3 关闭或重路由，且 close condition 记录在 blocker queue。
- 若实现 product path：`cad-core/fixtures/c6m4` 有 located profile fixture/expected，focused test 断言 status、metadata、shape、NamedShape history、diagnostics；capability 仍等 S5 发布。
- 若无法实现：`backend_gap_classification` 必须说明原因、FreeCAD authority、cad-core mismatch 和下一步，不得删除 c5m10 known_gap。
- `part_sweep_located_profile_freecadcmd_wrapper_build_blocker` 在 S3 不从 capability remaining gaps 删除；删除只允许 S5/S6。

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
git diff --check -- cad-core docs/CADCore6.0
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest.test_c5m10_part_sweep_located_profile_contract_keeps_freecadcmd_blocker
```

Focused（实现后）：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest
```
