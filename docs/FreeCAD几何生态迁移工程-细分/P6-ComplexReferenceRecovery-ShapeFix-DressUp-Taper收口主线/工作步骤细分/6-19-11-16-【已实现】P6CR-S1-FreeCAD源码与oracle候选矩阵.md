# P6CR-S1 FreeCAD 源码与 oracle 候选矩阵【已实现】

## 目标

把 ShapeFix、DressUp / Refine、taper 三类 producer 各压成一个可采集 FreeCAD oracle 的最小候选场景。

## 必读源码

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShape.h::MapperHistory`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::ShapeFixModule::removeSmallEdges()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getAddSubShape()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/ExtrusionHelper.cpp`

## 输出

- 为 `P6CR-SCOPE-002..004` 各写一个 concrete oracle 候选。
- 每个候选必须说明旧稳定引用是什么、producer 如何 split / deleted / modified、下游观察点是什么。
- 若某类找不到可采集场景，保留 `notCollected`，不要用 cad-core 输出倒推。

## 验收

```bash
rg -n 'MapperHistory|makeShapeWithElementMap|removeSmallEdges|getAddSubShape|MapperThruSections' src/Mod/Part/App src/Mod/PartDesign/App
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P6-ComplexReferenceRecovery-ShapeFix-DressUp-Taper收口主线
```

## 完成结论

- S1 已新增 `矩阵/p6_complex_reference_recovery_source_candidates.tsv`，为 `P6CR-SCOPE-002..004` 各保留一个 oracle-first 候选：ShapeFix/ReShape、DressUp/Refine、taper/ThruSections。
- ShapeFix 候选为 `P6CR-CAND-003`：`Source.Edge1` 小边经 `ShapeFixModule::removeSmallEdges()` / `ShapeBuild_ReShape` / `MapperHistory(BRepTools_ReShape)` 后进入 `makeShapeWithElementMap()`，下游用 `ProbeSketch.ExternalGeometry` 观察 deleted 或 recovered 行为。
- DressUp/Refine 候选为 `P6CR-CAND-004`：`Pad.Edge1` 经 Body-member `Chamfer::execute()`、`makeElementChamfer()`、`DressUp::getAddSubShape()` 与 `makeElementRefine()`，下游用 `ProbeSketch.ExternalGeometry` 或 `SubShapeBinder.Support` 观察 split / deleted / modified。
- taper 候选为 `P6CR-CAND-006`：`Sketch.Edge1` 经 `FeatureExtrude::buildExtrusion()`、`ExtrusionHelper::makeElementDraft()`、`BRepOffsetAPI_ThruSections` 与 `MapperThruSections::generated()`，下游用 `ProbePad.UpToFace` 或 `ProbeSketch.ExternalGeometry` 观察 generated side 的 unique/ambiguous/deleted 结果。
- `P6CR-BLOCK-003`、`010`、`011`、`020`、`021`、`030`、`031` 已按源码证据与 fixture/probe 设计关闭；`012`、`022`、`032` 仍留给 S2 采集或 collectorGap 裁决。
- `P6CR-SCOPE-002..004` 仍保持 `notCollected`：本轮只完成 source/oracle 候选矩阵，不采 expected，不决定 backendGap，不写 C++。
