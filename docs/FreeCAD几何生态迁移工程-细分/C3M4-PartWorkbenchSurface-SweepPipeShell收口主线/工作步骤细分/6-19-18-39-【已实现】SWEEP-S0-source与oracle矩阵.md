# 【已实现】SWEEP-S0 source 与 oracle 矩阵

复核 `Part::Sweep::execute()`、`TopoShape::makeElementPipeShell()` 和 `BRepOffsetAPI_MakePipeShell` wrapper，设计 spine/profile、solid/frenet/transition、invalid spine fixtures。

## live 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD`：`b4365e45ea`。
- `git log -1 --oneline`：`b4365e45ea chore: 补充 Surface 主线规划与 oracle 辅助接口`。
- `git -c core.quotepath=false status --short -uall`：起始工作区干净。

## FreeCAD 源码裁决

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.h:102` 定义 `Part::Sweep`，公开属性为 `Sections`、`Spine`、`Solid`、`Frenet`、`Linearize`、`Transition`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp:256` 的 `TransitionEnums` 顺序为 `Transformed`、`Right corner`、`Round corner`；构造函数默认 `Solid=true`、`Frenet=true`、`Transition=Right corner`、`Linearize=false`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp:303` 的 `Sweep::execute()` 先校验 `Sections` / `Spine`，按 `ResolveLink | Transform` 取 spine，若 `Spine` 有 subvalues 则逐个 `getSubTopoShape()` 后合成 compound，再把 spine 放入 `shapes[0]`、sections 作为后续 profile。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp:2436` 的 `TopoShape::makeElementPipeShell()` 要求至少两个输入，第一项必须能 `makeElementWires()` 成 single wire，后续 profiles 走 `prepareProfiles()`，再 `SetMode(isFrenet)`、`SetTransitionMode()`、`Add(profile)`、`IsReady()`、`Build()`、可选 `MakeSolid()`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp:2510` 通过 `makeElementShape(mkPipeShell, shapes, op)` 消费 maker history；`/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShape.h:3088` 的 `MapperMaker` 明确用 `Modified/Generated()` 生成元素命名映射。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp:2128` 的 `makeSweepSurface()` 说明 `GeomFill_Pipe` 没有 shape history，因此低层 helper 也改走 `makeElementPipeShell()`；它可辅助 oracle，但发布口径仍以 `Part::Sweep` DocumentObject 为准。

## S0 矩阵结论

- 已更新 `矩阵/part_surface_sweep_pipeshell_scope.tsv`，新增 `矩阵/part_surface_sweep_pipeshell_source_matrix.tsv` 与 `矩阵/part_surface_sweep_pipeshell_fixture_oracle_matrix.tsv`。
- S1 fixture schema 必须表达 source-backed `DocumentObject`：`TypeId=Part::Sweep`，`Sections=App::PropertyLinkList`，`Spine=App::PropertyLinkSub` 且可带 `SubList`，`Solid/Frenet/Linearize` 为 bool，`Transition` 按 FreeCAD enum 0/1/2 或等价 label。
- 首批 oracle 固定为 surface/default Right corner、solid、Frenet=false、Transition=Transformed/Round corner、Spine SubList、open profile、invalid inputs；`Linearize=true`、auxiliary spine / located profile / support / trihedron / binormal、GUI dialog、Hole ModelThread 内部 PipeShell 均不进入首批支持声明。
- S1 需要给 collector 增加 native `Part::Sweep` 和 sweep expected payload；当前 `cad-core/tools/collect_freecad_expected.py` 已有 `Part::Loft`，但尚无 `Part::Sweep`。
- executor / helper 必须保留 PipeShell maker history，并在 focused tests 中断言 source spine/profile history；不能只比较 final shape、bbox 或 mesh。

## 非目标

- 不实现 C++ executor。
- 不新增 cad-core fixture / expected 文件。
- 不采集 FreeCAD expected。
- 不发布 capability。
- 不迁移 GUI sweep dialog。
- 不把 Hole ModelThread 内部 PipeShell 等同为 `Part::Sweep` 支持。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py 'docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-SweepPipeShell收口主线/工作步骤细分' --format markdown
git diff --check -- 'docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-SweepPipeShell收口主线'
```

完成状态：本文件已按完成规则命名为 `6-19-18-39-【已实现】SWEEP-S0-source与oracle矩阵.md`。
