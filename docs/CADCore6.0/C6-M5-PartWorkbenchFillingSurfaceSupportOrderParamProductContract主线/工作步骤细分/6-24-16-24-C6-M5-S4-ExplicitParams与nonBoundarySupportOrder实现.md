# C6-M5-S4 ExplicitParams 与 nonBoundarySupportOrder 实现

## 目标

在 S3 的 Surface / SupportOrder 基础上，实现 Filling explicit params 与 non-boundary support/order product contract，关闭参数批次和非边界约束两类剩余缺口。

## 本轮代码实现目标

| blocker | scope | C++ 落点 | FreeCAD 依据 | 成功标准 |
| --- | --- | --- | --- | --- |
| `BLK-201` | PtsOnCurve、Anisotropy、TolG1/TolG2、MaxSegments、all params | `cad-core/src/part/part_filling.cpp`、`cad-core/src/part/topo_shape_expansion.cpp` | `AppPartPy.cpp::makeFilledFace()` constructor kwargs、`BRepOffsetAPI_MakeFillingPyImp.cpp::PyInit()`、`SetConstrParam`、`SetResolParam`、`SetApproxParam` | 每个参数子集有 locatable diagnostics 或稳定 product fixture，all params 不 crash。 |
| `BLK-202` | non-boundary edge support/order | `cad-core/src/part/part_filling.cpp`、`cad-core/src/part/topo_shape_expansion.cpp` | `TopoShapeExpansion.cpp::makeElementFilledFace()` 添加 remaining wire/edge/face/vertex constraints、wrapper `Add(edge, face, order, isBound)` | non-boundary support/order 能解析、执行或结构化诊断，并与 no-support/order 子集保持一致。 |

## 参数合同

- 保留现有 Degree、NumIter、Tol2d+Tol3d、MaxDegree expected-backed 子集。
- 新增 PtsOnCurve、Anisotropy、TolG1、TolG2、MaxSegments 与 all params 的产品状态。
- 参数错误必须带 property path 或对象名，不能只返回 `execution_failed`。

## 禁止捷径

- 不用 FreeCADCmd SIGSEGV 作为 expected。
- 不把参数静默丢弃。
- 不通过输出端修剪 / 排序隐藏 builder 失败。
- 不把 non-boundary source 猜成 boundary source。

## 必须回写的矩阵行

- `SCOPE-201`、`SCOPE-202`
- `IN-201`、`IN-202`
- `BLK-201`、`BLK-202`
- `ORC-201`、`ORC-202`
- `VAL-203`、`VAL-204`

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest
cd /home/user/Chili3DProject/FreeCAD
git diff --check -- cad-core/src/part/part_filling.cpp cad-core/src/part/topo_shape_expansion.cpp cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_p8_features.py docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线
```

验收通过后，将本文重命名为 `6-24-16-24-【已实现】C6-M5-S4-ExplicitParams与nonBoundarySupportOrder实现.md`。

## 非目标

- 不实现 UV point-on-support wrapper product API。
- 不实现 cross-request mutable builder。
- 不扩展到 GeomPlate 或 full Part surface family。
