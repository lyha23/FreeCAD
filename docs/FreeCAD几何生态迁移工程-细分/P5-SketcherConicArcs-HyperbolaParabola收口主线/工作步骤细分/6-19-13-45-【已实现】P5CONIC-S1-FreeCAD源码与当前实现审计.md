# 【已实现】P5CONIC-S1 FreeCAD 源码与当前实现审计

## 目标

把 FreeCAD `GeomArcOfHyperbola` / `GeomArcOfParabola` 的字段、OCCT 构造顺序、Sketcher 支持判断和 cad-core 当前实现逐项对齐，形成实现缺口清单。

## 必读文件

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObjectGeometry.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchAnalysis.cpp`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/sketcher/sketch_object_geometry.cpp`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/sketcher/sketch_object_operations.cpp`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/sketcher/sketch_object_external.cpp`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/P5-SketcherConicArcs-HyperbolaParabola收口主线/矩阵/p5_conic_arcs_source_candidates.tsv`

## 操作

1. 记录 FreeCAD 调用链：
   - `GeomArcOfHyperbola::Save/Restore()` 字段与 `GC_MakeHyperbola` / `GC_MakeArcOfHyperbola`。
   - `GeomArcOfParabola::Save/Restore()` 字段与 `gce_MakeParab` / `GC_MakeArcOfParabola`。
   - `SketchObject::isSupportedGeometry()` 对两类 conic arc 的 supported 语义。
   - `PointConstraints::addGeometry()` 对两类 conic arc 的 start/end 点语义。
2. 审计 cad-core 当前实现是否覆盖：
   - JSON kind alias。
   - 必填字段和默认 normal / angle。
   - construction 过滤。
   - profile edge 建边。
   - external geometry 合并。
   - diagnostics 和异常转换。
3. 更新 `p5_conic_arcs_source_candidates.tsv` 与 `p5_conic_arcs_scope_review_matrix.tsv`。

## 非目标

- 不迁移完整 Sketcher solver。
- 不实现 conic 约束或内部辅助几何。
- 不改 GUI、TaskPanel 或 Python API。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P5-SketcherConicArcs-HyperbolaParabola收口主线
```

## 完成条件

矩阵能够回答：当前 cad-core 与 FreeCAD 字段/建边/Sketcher supported 语义差在哪里，哪些只需要测试口径切换，哪些需要代码补齐。

## S1 审计结论

- FreeCAD 双曲线弧持久化字段是 `CenterX/Y/Z`、`NormalX/Y/Z`、`MajorRadius`、`MinorRadius`、`AngleXU`、`StartAngle`、`EndAngle`；恢复路径先构造 `GC_MakeHyperbola`，再构造 `GC_MakeArcOfHyperbola(..., Standard_True)`。
- FreeCAD 抛物线弧持久化字段是 `CenterX/Y/Z`、`NormalX/Y/Z`、`Focal`、`AngleXU`、`StartAngle`、`EndAngle`；恢复路径先构造 `gce_MakeParab`，再构造 `GC_MakeArcOfParabola(..., Standard_True)`。
- `SketchObject::isSupportedGeometry()` 明确把 `Part::GeomArcOfHyperbola` 与 `Part::GeomArcOfParabola` 判为 supported；`PointConstraints::addGeometry()` 对两类弧只登记 start/end 点，本批次不迁移完整 solver 内部辅助几何。
- cad-core 当前已有 profile implementation seed：`ArcOfHyperbola` / `Part::GeomArcOfHyperbola` 与 `ArcOfParabola` / `Part::GeomArcOfParabola` 解析、construction 过滤、profile edge 建边和 `GC_Make*Arc*` 路径均存在；当前 JSON 使用 lower-camel 字段，`angle` 默认 `0`，normal 隐式为 sketch-local `+Z`。
- external 路径需要拆分：request-side `ExternalGeo` native 几何池能解析并合并 `hyperbolaArcs` / `parabolaArcs`；实际 `TopoDS_Edge` 投影路径仍只覆盖 line/circle/ellipse，`GeomAbs_Hyperbola` / `GeomAbs_Parabola` 是 S3 缺口。
- 旧 `sketch-unsupported-hyperbola` 诊断和 unsupported fixture 是 validationMismatch / 测试口径切换；S2 需要先设计 valid/invalid、construction、native external 与 projected external 的 fixture/oracle 矩阵，再由 S3 改测试或实现。
