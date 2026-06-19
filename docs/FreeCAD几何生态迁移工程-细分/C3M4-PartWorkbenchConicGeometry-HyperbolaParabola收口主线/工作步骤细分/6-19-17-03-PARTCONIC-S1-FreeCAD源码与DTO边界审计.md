# PARTCONIC-S1 FreeCAD 源码与 DTO 边界审计

## 目标

用 FreeCAD 源码证明本轮到底是 source-backed `DocumentObject` executor，还是 Python geometry wrapper 对应的 cad-core Part geometry DTO。当前预期是后者；若源码证明不同，以源码为准。

## 必读

- `src/Mod/Part/App/Geometry.cpp`
- `src/Mod/Part/App/Geometry.h`
- `src/Mod/Part/App/AppPart.cpp`
- `src/Mod/Part/App/PrimitiveFeature.cpp`
- `src/Mod/Part/App/HyperbolaPyImp.cpp`
- `src/Mod/Part/App/ParabolaPyImp.cpp`
- `src/Mod/Part/App/ArcPyImp.cpp`
- `src/Mod/Part/App/TopoShapeEdgePyImp.cpp`
- `cad-core/src/part/primitive_feature.cpp`
- `cad-core/src/runtime/feature_registry.cpp`
- `矩阵/part_conic_geometry_source_candidates.tsv`
- `矩阵/part_conic_geometry_non_goal_registry.tsv`

## 工作内容

1. 补全 `part_conic_geometry_source_candidates.tsv`：字段、构造器、Python wrapper、edge extraction、primitive absence 都要有证据。
2. 明确 cad-core 输入表达：
   - 如果 FreeCAD 有真实 `DocumentObject`，列出 `TypeId` 和 executor 落点。
   - 如果没有，写清 cad-core adapter DTO 名称、字段和“不是 FreeCAD DocumentObject”的发布口径。
3. 更新 scope/blocker：关闭或保留 `PARTCONIC-BLOCK-001` / `BLOCK-002`。
4. 在方案文档里补一段 S1 裁决结论。

## 非目标

- 不写实现。
- 不为了省事注册假的 `Part::Hyperbola` / `Part::Parabola`。
- 不把 Sketcher conic structs 暴露为 Part public API。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchConicGeometry-HyperbolaParabola收口主线
```

完成后把本文件重命名为 `6-19-17-03-【已实现】PARTCONIC-S1-FreeCAD源码与DTO边界审计.md`。
