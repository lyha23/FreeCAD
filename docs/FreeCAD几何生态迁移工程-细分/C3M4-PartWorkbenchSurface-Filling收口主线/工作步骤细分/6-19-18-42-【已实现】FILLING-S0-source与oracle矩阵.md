# 【已实现】FILLING-S0 source 与 oracle 矩阵

复核 `makeFilledFace()`、`BRepFillingParams`、`makeElementFilledFace()` 和 OCCT `BRepOffsetAPI_MakeFilling` 分支，设计 boundary/support/order fixtures。只写专题包 docs/矩阵，不实现 C++/Rust/adapter，不采集或改写 checked-in expected。

## live 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD`：`cde2aca7a1`。
- `git log -1 --oneline`：`cde2aca7a1 feat: 发布 Part Sweep 能力`。
- `git -c core.quotepath=false status --short -uall`：干净。

## FreeCAD 源码裁决

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp:122` 的 `getPyShapes()` 接受单个 `TopoShapePy`、`GeometryPy` 或 shape sequence；sequence 中非 shape 会报 `expect shape in sequence`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp:1229` 的 `makeFilledFace()` 空输入报 `No input shape`，默认走 `TopoShape(...).makeElementFilledFace(shapes, params, op)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp:1238` 的 keyword 列表包含 `surface/supports/orders` 和全部参数，但 `Wrapped_ParseTupleAndKeywords(... "O|O!OOIIIOddddIIs", ...)` 的实参没有传 `&supports`，且 `O!` 没有显式 `TopoShapePy::Type` 实参；S1 不得把 optional kwargs 直接标成 supported。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeMapper.h:255` 的 `BRepFillingParams` 定义 filling defaults、initial surface、support/order map、boundary index。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp:3786` 的 `makeElementFilledFace()` 使用 `BRepOffsetAPI_MakeFilling`；先 `expandCompound()`，再优先 closed wire，否则构造 edge wire，找不到 boundary 报 `No boundary wire`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp:3918` 在 Add boundary 前执行 wire connection fix，避免 OCCT `BRepFill_Filling.cxx WireFromList()` 崩溃。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp:3930` 将 boundary edge 作为 `IsBound=true`，其它 wire/edge/face/vertex 作为 non-boundary constraints；成功后 `makeElementShape(maker, _shapes, op)` 消费 maker history。

## S0 矩阵结论

- 更新 `矩阵/part_surface_filling_scope.tsv`：把 simple boundary filling、diagnostics、topo history、support/order blocker 和 publish 状态拆开。
- 新增 `矩阵/part_surface_filling_source_matrix.tsv`：记录 helper signature、无原生 DocumentObject、parser kwargs 风险、boundary discovery、wire fix、constraint/support/order/history/diagnostic 证据。
- 新增 `矩阵/part_surface_filling_fixture_oracle_matrix.tsv`：S1 required 固定为 `part-filling-closed-wire-default`、`part-filling-boundary-edges-default`、`part-filling-invalid-inputs`；compound、constraints、initial surface、support/order、non-default params 作为 optional/deferred/blocker rows。
- 新增 `矩阵/part_surface_filling_blocker_status.tsv`：明确 `surface/supports/orders` 与非默认参数的 parser/oracle blocker，避免 S1 或 S2 capability 误宣称支持。
- 更新主方案：S1 落点为 `cad-core/src/part/part_filling.cpp` + `part/topo_shape_expansion` helper + collector helper oracle；不是 adapter-only 输出修正。

## 非目标

- 不实现 C++ executor、Rust 或 adapter。
- 不新增实际 fixture / expected 文件。
- 不采集 FreeCAD expected。
- 不修改 CADCore3.0 capability supported 文案。
- 不要求全量 FreeCAD build。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-Filling收口主线/工作步骤细分 --format markdown
git diff --check
```

完成状态：本文件已按完成规则命名为 `6-19-18-42-【已实现】FILLING-S0-source与oracle矩阵.md`。
