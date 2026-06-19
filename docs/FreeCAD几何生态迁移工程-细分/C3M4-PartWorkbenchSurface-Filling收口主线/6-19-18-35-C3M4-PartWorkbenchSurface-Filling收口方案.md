# C3M4 Part Workbench Surface Filling 收口方案

## 目标

实现 FreeCAD Part 后端 filled face / filling surface 能力：`Part.makeFilledFace()`、`TopoShape::makeElementFilledFace()`、`BRepOffsetAPI_MakeFilling` 参数、oracle fixture 和 capability 发布。

## S0 结论

- Filling 在 FreeCAD 里不是独立 `Part::FilledFace` DocumentObject；公开入口是 `Part.makeFilledFace()` / `Part.makeFilledSurface()` Python helper，GUI ShapeBuilder 也是把 helper 结果写入普通 `Part::Feature.Shape`。
- `cad-core` S1 应定义 source-backed helper operation，例如 `Part::FilledFace` 请求对象，显式表达 `Boundary` link/list/subshape 选择；collector 把该对象翻译成 `Part.makeFilledFace(...)` 调用采集 oracle，capability 文案必须说明它不是 FreeCAD 原生 DocumentObject 类型。
- 首批 required oracle 只发布默认参数的 boundary filling 和 diagnostic group：closed wire、connected boundary edges、空/缺失/非 boundary 输入。它们足够约束 `makeElementFilledFace()` 的 boundary 发现、wire 构造和 `BRepOffsetAPI_MakeFilling` 主路径。
- `surface`、`supports`、`orders` 和非默认 filling 参数先不标成 supported。源码中 `TopoShape::BRepFillingParams` 与 `makeElementFilledFace()` 已有这些分支，但 `AppPartPy.cpp` 的 helper parser 实参列表和 keyword 列表存在错位风险，必须先用 FreeCADCmd oracle probe 或修正 collector 路径证明可采集。

## FreeCAD 调用链

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp:122` 的 `getPyShapes()` 接受单个 `TopoShapePy`、`GeometryPy` 或 shape sequence；sequence 中非 shape 会报 `expect shape in sequence`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp:1229` 的 `makeFilledFace()` 声明 `shapes/surface/supports/orders/degree/ptsOnCurve/numIter/anisotropy/tol2d/tol3d/tolG1/tolG2/maxDegree/maxSegments/op`，空 `shapes` 抛 `No input shape`，最后调用 `TopoShape(0, shapes.front().Hasher).makeElementFilledFace(shapes, params, op)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp:1238` 的 keyword 列表包含 `supports`，但 `Wrapped_ParseTupleAndKeywords(... "O|O!OOIIIOddddIIs", ...)` 实参没有传 `&supports`，且 `O!` 位置没有显式 `TopoShapePy::Type` 实参；S1 不得直接假定 helper kwargs 可稳定采集。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeMapper.h:255` 的 `BRepFillingParams` 定义 `surface`、`orders`、`supports`、`boundary_begin/end`，以及默认 `degree=3`、`ptsoncurve=15`、`numiter=2`、`anisotropy=false`、`tol2d=1e-5`、`tol3d=1e-4`、`tolG1=0.01`、`tolG2=0.1`、`maxdeg=8`、`maxseg=9`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp:3786` 的 `makeElementFilledFace()` 构造 `BRepOffsetAPI_MakeFilling`；可选 initial surface 仅在 `params.surface` 是 face 时 `LoadInitSurface()`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp:3809` 先 `expandCompound()`，再优先选择 closed wire 作为 boundary；没有 wire 时收集 edge 并通过 `makeElementWires(... ConnectionPolicy::requireSharedVertex, &output)` 构造 wire。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp:3910` 找不到 boundary 时抛 `No boundary wire`；随后对 boundary 执行 `fix(Precision::Confusion(), ...)`，避免 OCCT `BRepFill_Filling.cxx WireFromList()` 因 wire connection tolerance 崩溃。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp:3930` 对 boundary ordered edges 调 `maker.Add(edge, support, order, true)`；剩余 wire/edge 作为非 boundary constraint，face 和 vertex 分别以 `maker.Add(face, order)`、`maker.Add(point)` 加入。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp:3970` `maker.Build()` 后若 `!maker.IsDone()` 抛 `Failed to created face by filling edges`；成功时 `makeElementShape(maker, _shapes, op)` 消费 `MapperMaker` 的 Modified/Generated history，默认 op 为 `Part::OpCodes::FilledFace`。

## cad-core 分层落点

- `document/app`：解析 source-backed filling 请求对象，禁止请求或响应携带完整 BREP；`Boundary`、可选 `Constraints` 用 link/list/subname 指向现有 graph 对象。
- `runtime`：注册 filling executor，并把 unsupported kwargs、缺失 link、非 edge/wire boundary、无 boundary wire 和 maker failure 输出为稳定 diagnostics。
- `part`：新增 `cad-core/src/part/part_filling.cpp`，风格对齐 `part_loft.cpp` / `part_sweep.cpp`，负责解析请求属性、resolve source shapes、记录输出字段和 diagnostic。
- `part/topo_shape_expansion`：新增 `makeElementFilledFaceFromSources(...)` helper，直接封装 `BRepOffsetAPI_MakeFilling`、boundary wire 选择、wire fix 和 maker history；不要在 adapter 层用 `FaceMaker` 或输出修正替代。
- `topo` / `NamedShape`：输出 `maker_history:filling` 或等价状态，并至少保留 boundary source edge 到生成 face/edge 的 source relation 证据；S1 测试不能只看 bbox。
- `tools/collect_freecad_expected.py`：为非原生 helper operation 增加 collector 分支：创建 source objects 后调用 `Part.makeFilledFace(...)`，再采集 shape summary、subshape、element map / history marker 和 diagnostics。
- `adapters/c_api`：S2 才发布 capability；文案区分 simple boundary filling、deferred support/order filling 和 diagnostic-only 分支。

## 首批 oracle / fixture 组合

- `part-filling-closed-wire-default`：一个闭合 wire boundary，默认 `BRepFillingParams`；验证输出为 filled face、subshape 稳定、history marker 存在。
- `part-filling-boundary-edges-default`：连接的 boundary edges 通过 `makeElementWires(... requireSharedVertex)` 进入同一 boundary；覆盖 FreeCAD GUI `Part.__sortEdges__(...)` 风格输入和 wire connection fix。
- `part-filling-invalid-inputs`：diagnostic group，覆盖空 boundary、missing link target、非 edge/wire/compound-only source、无法形成 boundary wire、unsupported support/order kwargs；不伪造 expected shape。
- `part-filling-compound-boundary`：保留 S1 optional 或 S2 row，用于证明 `expandCompound()` 后仍能找到 boundary。
- `part-filling-constraint-edge-vertex`：保留 S2 row，用于非 boundary edge/vertex/face constraint；实现前必须补 oracle 证据，不能混进 simple boundary capability。

## support / order 分批判断

- `TopoShapeExpansion.cpp` 内部已经按 source shape 或 `output` 回查 `params.orders` / `params.supports`，并把 boundary 与非 boundary edge 的 order/support 传给 `maker.Add()`；因此它属于正式 FreeCAD 语义，不是非目标。
- 当前 S1 不发布 support/order：`AppPartPy.cpp` helper parser 对 `surface/supports/orders` 的 keyword 映射存在源码异常，且没有 checked-in expected 覆盖。
- 解锁条件：S1 或后续单独 probe 需要用本机 FreeCADCmd 证明 `Part.makeFilledFace(..., supports=..., orders=...)` 可稳定执行并采集 expected；若 helper kwargs 不可用，则必须先选择 collector 专用低层 route 或把 FreeCAD 源码异常登记为 blocker，不能把 cad-core 支持声明为 FreeCAD parity。

## 非目标

- 不迁移 GUI command。
- 不把 Filling 和 GeomPlate 混成一个实现文件。
- 不把 `BRepOffsetAPI_MakeFillingPy` 的全部高级 wrapper 方法当作本专题包 supported surface capability。
- 不绕过 `BRepOffsetAPI_MakeFilling` 改用普通 FaceMaker 伪装支持。
