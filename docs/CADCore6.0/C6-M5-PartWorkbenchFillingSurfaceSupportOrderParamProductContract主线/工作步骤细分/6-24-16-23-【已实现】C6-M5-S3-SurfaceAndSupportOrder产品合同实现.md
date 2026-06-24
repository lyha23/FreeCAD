# C6-M5-S3 SurfaceAndSupportOrder 产品合同实现

## 目标

实现 Filling 的 Surface 与 Supports/Orders G1/G2 CAD Core product contract。该步骤是代码实现步骤，必须同时落 C++、fixtures、focused tests 和 capability draft，不允许只改文档。

## 本轮代码实现目标

| blocker | scope | C++ 落点 | FreeCAD 依据 | 成功标准 |
| --- | --- | --- | --- | --- |
| `BLK-101` | Surface / initial surface | `cad-core/src/part/part_filling.cpp`、`cad-core/src/part/topo_shape_expansion.cpp` | `AppPartPy.cpp::makeFilledFace()`、`TopoShapeExpansion.cpp::makeElementFilledFace()`、`BRepOffsetAPI_MakeFillingPyImp.cpp::loadInitSurface()` | request-local `Surface` face link 有稳定解析、诊断、metadata 和 fixture。 |
| `BLK-102` | Supports / Orders G1/G2 | `cad-core/src/part/part_filling.cpp`、`cad-core/src/part/topo_shape_expansion.cpp` | `BRepOffsetAPI_MakeFillingPyImp.cpp::add()` edge+face+order 分支 | Support face、Order C0/G1/G2、invalid target/source 都有 focused tests。 |

## S3 实现结果

- `cad-core/src/part/part_filling.cpp` 新增 `surface_source_status` 与 `support_order_source_status` 作为 request-local product-contract evidence；原有 `initial_surface_source_evidence`、`support_order_source_evidence` 和 locatable diagnostics 继续承接 FreeCAD `surface` / `supports` / `orders` 语义。
- `cad-core/src/runtime/capability_contract.cpp` 已记录 c6m5 Surface / Supports / Orders product-contract fixtures 和 S3 evidence；六个 `remaining_gaps` 全部保留，等待 S5/S6 gate。
- 新增 `cad-core/fixtures/c6m5/part-filling-surface-initial-face-product.json`、`part-filling-support-order-c0-g1-g2-product.json`、`part-filling-surface-support-order-invalid-product.json` 及对应 expected。
- `cad-core/tests/test_p8_features.py` 新增三条 C6-M5 focused tests，覆盖 Surface success、C0/G1/G2 support/order success、invalid_surface/support/order locatable diagnostics。

## 禁止捷径

- 不按 fixture 名称分支。
- 不靠 bbox、面积、长度排序推断 support ownership。
- 不在 adapter 层补业务语义。
- 不引入跨请求 `BRepOffsetAPI_MakeFilling` wrapper 状态。
- 不把 native helper crash 当作产品合同失败。

## 必须回写的矩阵行

- `SCOPE-101`、`SCOPE-102`
- `IN-101`、`IN-102`
- `BLK-101`、`BLK-102`
- `ORC-101`、`ORC-102`
- `VAL-201`、`VAL-202`

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest
cd /home/user/Chili3DProject/FreeCAD
git diff --check -- cad-core/src/part/part_filling.cpp cad-core/src/part/topo_shape_expansion.cpp cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_p8_features.py docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线
```

验收通过后，将本文重命名为 `6-24-16-23-【已实现】C6-M5-S3-SurfaceAndSupportOrder产品合同实现.md`；S3 不删除 capability `remaining_gaps`，删除等待 S5/S6 看到 capability / release gate 证据后再处理。

## 验收结果

- `cmake --build build` 通过。
- `python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest` 通过：`Ran 207 tests in 20.102s`，`OK`。
- 新增三条 C6-M5 tests 曾单跑通过：Surface success、SupportOrder C0/G1/G2 success、Surface/Support/Order invalid diagnostics。

## 非目标

- 不做 explicit params all-in-one。
- 不做 non-boundary support/order。
- 不做 Surface Workbench native filling feature。
