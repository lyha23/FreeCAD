# P5CONIC-S1 FreeCAD 源码与当前实现审计

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
