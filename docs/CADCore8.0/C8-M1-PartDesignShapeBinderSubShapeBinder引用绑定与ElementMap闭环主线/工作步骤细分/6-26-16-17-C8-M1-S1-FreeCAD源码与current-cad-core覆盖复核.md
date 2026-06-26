# C8-M1-S1 FreeCAD 源码与 current cad-core 覆盖复核

## 目标

复核 ShapeBinder / SubShapeBinder 的 FreeCAD source authority、上游测试证据、current `cad-core` registry / executor / topo 能力和不可跨越边界。S1 不采 oracle，不改 C++。

## FreeCAD 依据

| 入口 | 必查函数 / 字段 | 复核重点 |
| --- | --- | --- |
| `src/Mod/PartDesign/App/ShapeBinder.cpp` | `ShapeBinder::updatedShape()` | `TraceSupport` placement |
| `src/Mod/PartDesign/App/ShapeBinder.cpp` | `ShapeBinder::getFilteredReferences()` | single Part feature、multi subshape |
| `src/Mod/PartDesign/App/ShapeBinder.cpp` | `ShapeBinder::buildShapeFromReferences()` | whole shape、subshape compound、datum fallback |
| `src/Mod/PartDesign/App/ShapeBinder.cpp` | `SubShapeBinder::update()` | support resolution、shape collection、MakeFace/Fuse/Offset/Refine |
| `src/Mod/PartDesign/App/ShapeBinder.cpp` | `SubShapeBinder::getSubObject()` | nested object / label route |
| `src/Mod/PartDesign/App/ShapeBinder.cpp` | `setupCopyOnChange()`、`checkCopyOnChange()` | CopyOnChange lifecycle |
| `src/Mod/PartDesign/App/Body.cpp` | ShapeBinder / SubShapeBinder group eligibility | Body chain interaction |
| `src/Mod/PartDesign/App/Feature.cpp` | binder as BaseFeature / profile consumer | downstream feature behavior |

## current cad-core 复核

必须检查：

- `cad-core/src/runtime/feature_registry.cpp`
- `cad-core/src/part_design/body.cpp`
- `cad-core/src/part_design/profile_resolver.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/src/app/copy_on_change.cpp`
- `cad-core/src/runtime/reference_resolution.cpp`
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_p8_features.py`

## 必须回写的矩阵行

- `c8m1_shapebinder_source_authority.tsv`：所有 source row 必须有 FreeCAD symbol 和 cad-core landing。
- `c8m1_shapebinder_scope.tsv`：每个 scope 必须有 current status。
- `c8m1_shapebinder_non_goal_registry.tsv`：GUI/session/adapter/Rust 边界必须明确。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'PartDesign::ShapeBinder|PartDesign::SubShapeBinder|ShapeBinder::updatedShape|SubShapeBinder::update|setupCopyOnChange|BindMode|BindCopyOnChange|makeElementOffset2D|makeElementFace' src/Mod/PartDesign/App src/Mod/PartDesign/PartDesignTests cad-core/src cad-core/tests docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线
rg -n 'PartDesign::ShapeBinder|PartDesign::SubShapeBinder' cad-core/src/runtime/feature_registry.cpp
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/矩阵/*.tsv
git diff --check
```

`feature_registry.cpp` 的 rg 可以无匹配，但 S1 文档必须把这件事记录为 current gap evidence，而不是直接发布 supported。

验收通过后，将本文件重命名为 `6-26-16-17-【已实现】C8-M1-S1-FreeCAD源码与current-cad-core覆盖复核.md`。

## 非目标

- 不实现 C++。
- 不采 FreeCAD oracle。
- 不把 source-only evidence 提升为 backend gap conclusion。
