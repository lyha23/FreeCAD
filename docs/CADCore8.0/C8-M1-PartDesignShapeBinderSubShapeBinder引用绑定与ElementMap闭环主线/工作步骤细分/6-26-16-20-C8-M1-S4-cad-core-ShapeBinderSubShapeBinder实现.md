# C8-M1-S4 cad-core ShapeBinder / SubShapeBinder 实现

## 目标

消费 S3 已采集且 current `cad-core` mismatch 的 backend gap，落 C++ executor / DTO / topo / registry 实现。S4 是本包的主要代码落点。

## 本轮代码实现目标

| blocker / scope | C++ 落点 | FreeCAD authority | 测试 |
| --- | --- | --- | --- |
| `C8M1-BG-101` / `C8M1-SCOPE-101..103` | `cad-core/src/part_design/feature_shape_binder.cpp` | `ShapeBinder::updatedShape()`、`buildShapeFromReferences()` | ShapeBinder whole / subshape / TraceSupport expected |
| `C8M1-BG-201` / `C8M1-SCOPE-201..205` | `cad-core/src/part_design/feature_shape_binder.cpp` | `SubShapeBinder::update()` | SubShapeBinder MakeFace / Offset / Fuse / Refine expected |
| `C8M1-BG-301` / `C8M1-SCOPE-206` | `part/topo_shape_expansion.cpp` reuse plus executor history fields | `makeElementCompound`、`reTagElementMap`、`makeElementTransform` | ElementMap / NamedShape focused tests |
| `C8M1-BG-401` / `C8M1-SCOPE-301..302` | `app/copy_on_change.cpp` reuse or diagnostic in executor | `setupCopyOnChange()`、`onChanged()` | BindMode / CopyOnChange supported subset or blocker tests |

## 必须修改的文件

- `cad-core/include/cad_core/part_design/feature_shape_binder.h`
- `cad-core/src/part_design/feature_shape_binder.cpp`
- `cad-core/src/runtime/feature_registry.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/CMakeLists.txt`
- focused tests under `cad-core/tests/`

## 禁止路径

- 不在 CLI / C ABI / Worker adapter 层修正业务语义。
- 不按 fixture 名称、bbox、面积、长度、几何类型排序选择 subshape。
- 不保存跨请求 `TopoDS_Shape`、BREP、mesh、NamedShape 或 ElementMap。
- 不把 CopyOnChange temporary document cache 搬成 backend session。
- 不跳过 ElementMap / NamedShape，只返回 display shape。

## 实现顺序

1. 增加 executor header / source，并注册 `PartDesign::ShapeBinder`、`PartDesign::SubShapeBinder`。
2. 实现 ShapeBinder support resolution、subshape compound 和 `TraceSupport` placement。
3. 实现 SubShapeBinder support collection、relative transform、MakeFace / Offset / Fuse / Refine。
4. 接入 ElementMap / NamedShape / history metadata。
5. 对 BindMode / CopyOnChange 按 S3 evidence 实现 supported subset 或显式 diagnostic。
6. 更新 capability contract。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_c8_shapebinder
python3 -m unittest tests.test_diagnostics
```

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'PartDesign::ShapeBinder|PartDesign::SubShapeBinder|feature_shape_binder|shape_binder|sub_shape_binder|C8M1-BG' cad-core/src cad-core/include cad-core/tests docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线
git diff --check
```

验收通过后，将本文件重命名为 `6-26-16-20-【已实现】C8-M1-S4-cad-core-ShapeBinderSubShapeBinder实现.md`。

## 非目标

- 不实现 GUI task panel。
- 不实现下游 Rust adapter。
- 不重开 C7-M7 Link persistent writeback。
