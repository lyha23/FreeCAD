# 【已实现】PARTSURF-S1 FreeCAD 源码与批次裁决

## 目标

复核 `Part::RuledSurface` 和 `Part::ProjectOnSurface` 的 FreeCAD 调用链，裁决 S3 第一实现批次只做 source-backed `Part::RuledSurface` edge 分支，避免把 ProjectOnSurface 全分支混入同一实现。

## 必读

- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/6-19-18-21-C3M4-PartWorkbenchSurface-RuledProjection收口方案.md`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/矩阵/part_surface_source_candidates.tsv`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.h`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.h`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp`

## 工作内容

1. 用 `rg` 和源码阅读确认 `RuledSurface::execute()`、`TopoShape::makeElementRuledSurface()`、`ProjectOnSurface::tryExecute()`、`projectWire()`、`projectFace()`、`createSolidIfHeight()` 的真实调用顺序。
2. 更新 source candidates 和 scope matrix，写清 RuledSurface executor 的 cad-core 分层落点。
3. 裁决 S3 是否只纳入 edge/edge，还是可以同时纳入 wire/wire；必须写明依据，不能默认扩大。
4. 裁决 ProjectOnSurface 的 S4 处理方式：窄 edge projection 实现、还是拆出后续主线。

## live 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD`：`90fba4c359`。
- `git log -1 --oneline`：`90fba4c359 docs: 冻结PARTSURF S0基线`。
- `git -c core.quotepath=false status --short -uall`：仅有既有未暂存 `src/Mod/Sketcher/App/SketchObject.h`、`src/Mod/Sketcher/App/SketchObjectPyImp.cpp`；S1 不回退、不覆盖、不暂存这两个文件。

## FreeCAD 调用链裁决

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.h::Part::RuledSurface`：声明 `Orientation`、`Curve1`、`Curve2`，覆盖 `execute()` / `mustExecute()`；这是 source-backed `DocumentObject`，不是 request DTO helper。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::RuledSurface::RuledSurface()`：`OrientationEnums` 为 `"Automatic"` / `"Forward"` / `"Reversed"`，`Curve1` / `Curve2` / `Orientation` 都挂在 `"Ruled Surface"` 属性组。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::RuledSurface::execute()`：遍历 `Curve1` / `Curve2`，先用 `Part::Feature::getTopoShape(obj, ShapeOption::ResolveLink | ShapeOption::Transform)` 获取 linked shape；有且仅有一个 subname 时改用 `ShapeOption::NeedSubElement | ShapeOption::ResolveLink | ShapeOption::Transform`。关键短句为 `"No shape linked."`、`"Not exactly one sub-shape linked."`、`"Invalid link."`；成功后调用 `res.makeElementRuledSurface(shapes, Orientation.getValue())`，写入 `Shape`，再返回 `Part::Feature::execute()`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRuledSurface()`：先要求 exactly two shapes，关键短句为 `"Wrong number of input shapes"`、`"Null input shape"`、`"Input shape has no edge"`、`"Input shape forms more than one wire"`；接受 edge/wire，非 edge/wire 时提取 single wire / single edge 或调用 `makeElementWires()`，edge/wire 混合时把 edge 转 wire。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRuledSurface()`：`Automatic` 通过两条曲线端点附近采样、三角面法向点积判断是否反转第二条曲线；`Reversed` 直接 `S2.setShape(S2.getShape().Reversed(), false)`；edge/edge 调 `BRepFill::Face(Edge, Edge)`，wire/wire 调 `BRepFill::Shell(Wire, Wire)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRuledSurface()`：FreeCAD 明说 `"Both BRepFill::Face() and Shell() modifies the original input edges"`，因此用 `findSubShapesWithSharedVertex()` 找回输出边，`resetElementMap(e.elementMap())`，最后 `makeShapeWithElementMap(res.getShape(), Mapper(), edges, op)`；cad-core 不能在 adapter 输出端补猜 source ownership。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.h::Part::ProjectOnSurface`：声明 `Mode`、`Height`、`Offset`、`Direction`、`SupportFace`、`Projection`，并有 `createProjectedWire()`、`projectFace()`、`projectWire()`、`fixWire()`、`createFaceFromParametricWire()`、`createSolidIfHeight()`、`filterShapes()` 等内部步骤。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::tryExecute()`：先 `getSupportFace()`，再 `getProjectionShapes()`，读取 `Direction`，逐个 `createProjectedWire()`，再 `filterShapes()`、`createCompound()`、恢复 `Placement`。关键短句为 `"No support face specified"`、`"Expect exactly one support face"`、`"Number of objects and sub-names differ"`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::projectWire()`：`BRepProj_Projection(wire, supportFace, dir)`，取距离参考形状最近的 projected wire，再拆成 edge 输出。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::projectFace()` / `createSolidIfHeight()` / `filterShapes()`：face 分支会投影 wires、`fixWire()`、重建 surface parametric face，并在 `Height > 0 && Mode == All` 时走 `BRepPrimAPI_MakePrism`；`filterShapes()` 按 `"All"` / `"Faces"` / `"Edges"` 改变输出集合。

## S1 批次裁决

- `Part::RuledSurface` 是 source-backed `DocumentObject`；cad-core 落点应是 Part executor + `TopoShapeExpansion` 等价 helper + topo provenance，adapter 只能做协议转换，不能作为特例输出面。
- S3 第一批默认只纳入 edge/edge：注册 `Part::RuledSurface` executor，解析 `Curve1` / `Curve2` / `Orientation`，实现 edge/edge `BRepFill::Face`、orientation enum 和最小源 edge provenance。
- wire/wire 只有在 S2 同时证明 FreeCAD oracle 可采集、cad-core input/link schema 可控、shell/topo provenance 有验收约束时才扩入 S3；否则保留为后续批次，不把 full RuledSurface 写成 supported。
- `Part::ProjectOnSurface` 虽然也是 source-backed，但 `tryExecute()` 同时覆盖 support face、projection list、mode filter、height/offset、wire repair、face rebuild 和 solid 分支；它只进入 S4 裁决，禁止混入 S3。
- S1 已同步更新 `part_surface_source_candidates.tsv`、`part_surface_scope_review_matrix.tsv`、`part_surface_blocker_queue.tsv` 和总方案中的 S1 结论。

## 非目标

- 不写 cad-core 实现。
- 不采集 expected。
- 不新增 full surface family supported 口径。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/工作步骤细分 --format markdown
```

完成后把本文件重命名为 `6-19-18-23-【已实现】PARTSURF-S1-FreeCAD源码与批次裁决.md`。

完成状态：本文件已按完成规则命名为 `6-19-18-23-【已实现】PARTSURF-S1-FreeCAD源码与批次裁决.md`。
