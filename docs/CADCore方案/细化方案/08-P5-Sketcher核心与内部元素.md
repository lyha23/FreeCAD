# P5：Sketcher 核心与内部元素

P5 的目标是让 `Sketcher::SketchObject` 不再只是简单闭合 profile 生成器，而能承载 FreeCAD 风格草图几何、外部引用、内部元素和后续 PartDesign 特征所需的稳定引用。

## FreeCAD 语义来源

| 语义 | FreeCAD 参考位置 |
| --- | --- |
| Sketch 执行 | `src/Mod/Sketcher/App/SketchObject.cpp`：`SketchObject::execute()` |
| 形状构建 | `SketchObject.cpp`：`buildShape()`、`buildInternals()` |
| 几何数据 | `src/Mod/Sketcher/App/SketchObjectGeometry.cpp`、`Sketch.cpp` |
| 约束数据 | `src/Mod/Sketcher/App/SketchObjectConstraints.cpp` |
| 外部引用 | `src/Mod/Sketcher/App/SketchObjectExternal.cpp` |
| 操作语义 | `src/Mod/Sketcher/App/SketchObjectOperations.cpp` |
| 构面 | `src/Mod/Part/App/FaceMaker*.cpp`、`WireJoiner.cpp` |

## 当前问题

- 当前 Sketch 已能服务 MVP/P2/P3a/P3b 的可成形 profile，并开始消费 FreeCAD 风格 `Coincident(Type=1)` 端点约束；`LineSegment`、`ArcOfCircle`、`ArcOfEllipse`、单个非 construction `Circle` 和单个非 construction `Ellipse` 已可参与 profile，完整几何矩阵和 solver 仍未迁移。
- `ExternalGeometry` 已开始按 FreeCAD 的持久 LinkSubList / transient ExternalGeo 边界迁移；Sketch 原始 `Shape` 与 PartDesign 闭合 profile face 已拆开，open wire sketch 不再被直接当成 sketch 执行失败。闭合 sketch 已导出运行态 `InternalFaceN` / `InternalEdgeN` / `InternalVertexN`；最小双向 `internal_element_map` 已收敛到 `topo/element_map`，并允许其它 sketch 的 `ExternalGeometry` 引用 `InternalEdgeN` / `InternalVertexN`。FaceMakerBuildFace / WireJoiner 完整账本、复杂 `getInternalElementMap()` 和旧引用恢复还没有完整迁移。
- FaceMaker / WireJoiner 的几何账本不能继续由 sketch executor 猜测。
- 约束求解不应在这一阶段无边界扩成完整 GUI Sketcher；先保证 CAD Core recompute 需要的 solver-facing 状态。

## 当前落地状态

- `Sketcher::SketchObject` 已支持 `LineSegment`、`ArcOfCircle`、`ArcOfEllipse` 组成的闭合 profile，以及单个非 construction `Circle` / `Ellipse` profile；bspline 等未迁移几何仍返回 `unsupported_geometry`。
- `construction=true` 的 line / arc / circle / ellipse 不参与 profile 构面；当前只作为输入表达保留，未进入内部元素账本。
- Sketch executor 已对齐 FreeCAD `SketchObject::buildShape()` 的关键边界：非 construction 几何先形成 raw sketch `Shape`，闭合 profile face 作为 PartDesign `ProfileBased` 的可选运行态结果单独保存。open wire sketch 当前 `status=ok`、`profile_ready=false`、保留 raw `EdgeN` subshape，依赖它的 Pad / Pocket 通过 `open_profile` diagnostics 失败，不把原始边丢掉也不伪造闭合 face。
- closed profile 的 `InternalShape` 当前作为运行态辅助结果导出 `InternalFaceN` / `InternalEdgeN` / `InternalVertexN`，并通过 `topo/element_map` 输出 `InternalEdgeN <-> EdgeN`、`InternalVertexN <-> VertexN` 形式的最小 `internal_element_map`。解析时只在目标是 Sketch 且 subname 带 `Internal` 前缀时转到 `internalShape`，避免把普通 solid 的 `InternalFace1` 错当成 `Face1`。
- `ExternalGeometry` 已允许 `App::PropertyLinkSubList` 输入，当前 runtime 支持 DatumLine 或 `EdgeN` straight-line edge 投影成 transient construction line，支持 circle `EdgeN` 按 FreeCAD 平行 / 垂直 / 一般倾斜语义投影成 transient construction circle / line / ellipse，支持完整 ellipse `EdgeN` 按平行 / 倾斜语义投影成 transient construction ellipse 或退化 construction line，也支持 DatumPoint 或 `VertexN` 投影成 transient construction point，并记录 `external_geometry_count` / `external_point_count` / `external_curve_count`；face / 非平行 circle 或 ellipse arc edge / defining external profile 仍返回 diagnostics 或保留为后续任务。
- `Constraints` 已允许保存 FreeCAD 风格字段 `Type` / `First` / `FirstPos` / `Second` / `SecondPos`，当前 runtime 只消费 `Coincident` / `Type=1`，并只合并 line endpoint。其它 constraint 返回 `unsupported_property`，避免悄悄做不完整求解。
- line/arc profile 构造已从“输入顺序必须天然首尾相接”推进到“先合并 Coincident 端点，再按端点连通性寻找闭合 wire”；circle / ellipse profile 走 OCCT 单闭合 wire。这仍不是完整 planegcs，只是 CAD Core 当前需要的 solver-facing 子集。
- `fixtures/p5` 已覆盖 Coincident 端点合并生成 Pad profile、ArcOfCircle profile、ArcOfEllipse profile、Circle profile、Ellipse profile、construction geometry ignored、open wire raw shape / no profile face、closed sketch internal subshape export、closed sketch `internal_element_map`、external sketch `InternalEdgeN` / `InternalVertexN` projection、external DatumLine edge projection、external DatumPoint vertex projection、external circle edge 的平行 / 垂直 / 倾斜投影、external ellipse edge 的平行 / 倾斜投影、missing external target、unsupported external face subshape、unsupported BSpline geometry、unsupported constraint diagnostics；当前 `python3 -m unittest tests/test_mvp.py` 为 53 tests OK。

## Step 36：草图几何矩阵

目标：

- 支持 line、arc、circle、ellipse、bspline、construction geometry 的输入表达。
- 明确哪些几何参与 profile，哪些只做 construction / reference。
- 对 unsupported geometry 返回 diagnostics。

fixtures：

```text
fixtures/p5/
  sketch-line-profile.json
  sketch-coincident-profile.json
  sketch-arc-profile.json
  sketch-circle-profile.json
  sketch-construction-ignored.json
  sketch-unsupported-bspline.json
```

## Step 37：Constraints 输入和 solver-facing 状态

目标：

- 持久数据允许保存 FreeCAD 风格 constraints。
- runtime 先只执行 CAD Core 当前需要的约束子集，未支持约束必须 diagnostics 或明确忽略边界。
- Coincident / endpoint merge / closed-wire validation 要稳定。

验收：

- 不把完整 planegcs 求解器作为 P5 的前置要求。
- 不支持的约束不能悄悄改变几何。

fixtures：

```text
fixtures/p5/
  sketch-coincident-profile.json
  sketch-unsupported-constraint.json
```

当前已完成 Coincident endpoint merge 的最小运行子集；完整 Horizontal / Vertical / Distance / Tangent 等约束仍不执行。

## Step 38：External geometry

目标：

- 支持 Sketch external reference 的 LinkSub 输入。
- 从 P4/P6 的 LinkSub / stable subname 解析到目标 face / edge / vertex。
- 外部引用参与 profile 或 construction 时有明确语义。

fixtures：

```text
fixtures/p5/
  sketch-external-edge.json
  sketch-external-vertex.json
  sketch-external-circle-edge.json
  sketch-external-circle-edge-as-line.json
  sketch-external-tilted-circle-edge.json
  sketch-external-ellipse-edge.json
  sketch-external-tilted-ellipse-edge.json
  sketch-external-face-unsupported.json
  sketch-missing-external.json
```

验收：

- 目标 subname 丢失时返回 diagnostics，不生成错误 profile。
- 当前完成 `ExternalGeometry` -> projected construction line / point / circle edge / full ellipse edge 子集；face / 非平行 circle 或 ellipse arc edge 和 defining external profile 需要 FaceMaker / ExternalGeo 账本继续迁移。
- 修改 base feature 后 external reference 的恢复交给 P6 topo naming。

## Step 39：InternalShape 和内部元素

目标：

- 对齐 FreeCAD `buildInternals()`。
- 支持 `InternalShape`、`InternalFaceN`、`InternalEdgeN`、`InternalVertexN`。
- 支持 `getInternalElementMap()` 等价输出。
- open profile / open wire 语义和 FreeCAD 保持一致。

定位规则：

- `FaceMakerBuildFace` 的 face / edge / vertex 几何结果归 `geometry/`。
- `WireJoiner::getOpenWires()` 的 open wire 结果归 `geometry/`。
- `NamedShape` / `ElementMap` 消费 history 归 `topo/`。
- `sketch_object.cpp` 只表达 SketchObject 的调用顺序和属性语义。

fixtures：

```text
fixtures/p5/
  sketch-internal-face.json
  sketch-open-wire-internal-empty.json
  sketch-external-internal-edge.json
```

当前已新增 `sketch-open-wire-internal-empty.json` 约束 raw sketch shape / closed profile face 分离：open wire sketch 自身成功、raw `EdgeN` 保留、`profile_ready=false`，但完整 `InternalFaceN` / `InternalEdgeN` / `InternalVertexN` 和 WireJoiner open-wire history 仍需继续迁移。

当前已新增 `sketch-internal-face.json` 和 `sketch-external-internal-edge.json` 约束 closed sketch 的运行态内部元素导出与 `ExternalGeometry` 对 `InternalEdgeN` 的解析；这还不是完整 `getInternalElementMap()`，旧引用恢复和 history 传播仍归 P6。

当前已在 `sketch-internal-face.json` 中约束最小 `internal_element_map`，并新增 `sketch-external-internal-vertex.json` 约束 `ExternalGeometry` 对 `InternalVertexN` 的解析；映射生成逻辑已从 `sketch_object.cpp` 下沉到 `topo/element_map`。复杂 split / open-wire / history 场景仍等待 FaceMaker / WireJoiner / P6 topo naming 主路径。

## 完成定义

P5 完成需要同时满足：

- Sketch 几何矩阵有明确支持 / 不支持边界。
- external geometry 通过统一 LinkSub / topo naming 解析。
- `InternalShape` 和内部元素映射有 fixture。
- open profile 与 `InternalShape` 为空的语义和 FreeCAD 一致。
- 不存在按 fixture 名称、几何类型排序或 source edge 猜测补结果的逻辑。
