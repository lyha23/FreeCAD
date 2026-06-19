# C3M4 Part Workbench Surface Sweep / PipeShell 收口方案

## 目标

实现 `Part::Sweep` 和后端 PipeShell 能力：source-backed executor、`BRepOffsetAPI_MakePipeShell` 参数、oracle fixture、maker history 和 capability 发布。

## FreeCAD 依据

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.h::Part::Sweep`：属性为 `Sections`、`Spine`、`Solid`、`Frenet`、`Linearize`、`Transition`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()`：读取 spine link-sub，必要时把 spine subshapes 合成 compound，再把 spine 作为 shapes[0]，sections 作为 profiles，调用 `result.makeElementPipeShell(shapes, isSolid, isFrenet, transMode, Part::OpCodes::Sweep)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementPipeShell()`：spine 必须能形成 single wire；使用 `BRepOffsetAPI_MakePipeShell`、`SetMode(isFrenet)`、`SetTransitionMode()`、`SetTolerance()`、`Add(profile)`、`IsReady()`、`Build()`、可选 `MakeSolid()`，最后 `makeElementShape(mkPipeShell, shapes, op)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::makeSweepSurface()`：注释说明 `makeSweep uses GeomFill_Pipe which does not support shape history. So use makeElementPipeShell() as a replacement`。

## 首批范围

- `Part::Sweep`：single spine wire + one profile。
- `Solid=false/true`、`Frenet=false/true`、`Transition=Transformed/Right corner/Round corner`。
- low-level PipeShell helper 要保留 maker history，不能只输出 final shell/solid。

## 非目标

- 不迁移 GUI sweep dialog。
- 不把 Hole ModelThread 的内部 PipeShell 使用等同于 Part::Sweep 支持。
- 不一次性发布 auxiliary spine / all low-level Python wrapper methods。
