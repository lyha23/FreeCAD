# P5：Sketcher 核心与内部元素

P5 让 Sketch 不只是 Pad 的简单 profile，而能承载 FreeCAD 风格外部引用、内部元素和 solver-facing 子集。

下一阶段的 Sketcher 重点不再单独扩大 ExternalGeometry fixture，而是按 `13-ExternalGeometry-TopoNaming下一阶段主线.md` 与 P6 联合推进：ExternalGeometryExtension 状态机、FaceMaker / WireJoiner history 和复杂引用恢复必须落到同一套 MapperHistory / ElementMap 主路径。

## 当前基线

- 支持 line、arc of circle、arc of ellipse、circle、ellipse 和基础非周期 BSpline profile；Sketch `Point` 会按 FreeCAD `GeomPoint::toShape()` 输出 raw vertex。
- construction geometry 不参与 profile 构面。
- `Coincident` / `Type=1` 可合并 line endpoint；Horizontal / Vertical line / line-end-pair constraint、Parallel whole-line relation constraint、Perpendicular whole-line / line-circle-arc midpoint / point-wise / point-point-line relation constraint、Tangent whole-geometry direct / point-wise relation constraint、PointOnObject point-on-curve constraint、Symmetric point-point relation constraint、Block fixed-geometry constraint、Angle whole-line / point-wise datum constraint、Equal line-length / circle-radius constraint、Distance / DistanceX / DistanceY / Radius / Diameter datum constraint，以及 line endpoint 的 fixed X/Y coordinate datum 可接受已经满足的 solver-facing 约束或固定声明，但不移动未满足约束的几何。
- 多闭合 wire 可构成一个基础 face-with-holes；最大面积 wire 作为 outer，奇数层 wire 作为 hole，偶数层嵌套 wire 作为 island face，已覆盖 line outer + circle inner 和 hole-with-island 的混合闭合 profile。closed-wire hole 场景已按 FreeCAD `SketchObject::buildInternals()` 口径拆开：Pad / Pocket 继续消费 closed profile face-with-holes，Sketch `InternalShape` 可发布 `FaceMakerBuildFace` bounded region 结果。
- `geometry::buildSketchInternals()` 已接入 `FaceMakerBuildFace` bounded split 子集和 `WireJoiner::getOpenWires(noOriginal=true)` 子集：矩形分隔线、十字切割、closed-wire hole、重叠闭合 profile、自相交 wire、BSpline profile / figure-8 / empty InternalShape oracle、dangling open line、贯穿 profile 后外伸的 open cutter 等场景可发布 `InternalFaceN` / `InternalEdgeN` / `InternalVertexN`，其中原始 source open edge 会按 FreeCAD `noOriginal` 过滤，非原始 split fragments 可留在 `InternalShape`；`InternalFaceN` 已按 FaceMaker 外环边命名口径记录 generated-from-outer-boundary-`EdgeN` history，hole / inner wire 不混入同一个 face 来源，且仍不写 `FaceN -> InternalFaceN` alias；raw sketch edge 拆成多个 `InternalEdgeN` 时已记录 terminal split history，自交 cubic BSpline 单边 pre-split 已约束为 raw `Edge1` 到多个 `InternalEdgeN` 的 terminal split history，raw sketch `EdgeN/VertexN` 被过滤且没有 Internal* target 时已记录 terminal deleted history；FaceMakerBuildFace 的 pre-split / splitter history summary 与 WireJoiner EdgeInfo / WireInfo summary 已作为 request-local 账本暴露。这些非一对一关系都不写入单一可解析 `ElementMap` target；Pad / Pocket 可通过 `Profile.SubList=InternalFaceN` 选择局部 profile。
- open wire sketch 自身成功输出 raw shape，但 `profile_ready=false`，Pad/Pocket 通过 `open_profile` 失败。
- closed sketch 可导出基础 `InternalFaceN` / `InternalEdgeN` / `InternalVertexN`。
- `topo/element_map` 承接最小 `InternalEdgeN/InternalVertexN <-> EdgeN/VertexN` 映射。
- ExternalGeometry 支持 DatumLine / DatumPoint、straight edge、vertex、circle edge、ellipse edge、planar face boundary edge、whole-shape Face / Edge 展开和 sketch internal edge / vertex 的基础投影；planar face 垂直于 sketch plane 时按 FreeCAD `processFace()` 收敛为一条 construction line；`ExternalTypes=Projection/Intersection/Both` 已接入基础 section intersection 路径。

## 已知缺口

- 完整 constraint solver、会移动几何的 Horizontal / Vertical / Distance / Radius 等约束、BSpline solver/control-point 语义、`ExternalGeometryExtension` Defining / Frozen / Detached / Missing / Sync 状态机和 defining external profile 尚未迁移。
- ExternalGeometry 非平行 circle/ellipse arc edge、非平面 face HLR 投影等复杂场景仍未完整。
- 当前 `FaceMakerBuildFace` 覆盖 bounded split 主几何子集、self-intersecting edge pre-split terminal history、`InternalFaceN` outer-boundary generated history，以及 pre-split / splitter history summary；`WireJoiner` 覆盖 `noOriginal` 原始 source edge 过滤、open edge 按 face boundary split 的 fragment 子集、closed-source result-fragment ownership、bounded primary / secondary owner slot、terminal split / deleted history 和 open-wire carry-through；完整 `FaceMaker::postBuild()` / splitter MapperHistory 进入 `NamedShape` / `ElementMap`、`WireJoinerP::findTightBound()` / `exhaustTightBound()` ownership、open-wire history 和 source/split 账本尚未迁移。
- 复杂 `getInternalElementMap()`、split/merge/deleted history 到 internal element map 的传播和旧引用恢复仍需 P5/P6 联合补齐。

## cad-core 落点

| 文件 | 职责 |
| --- | --- |
| `features/sketch_object.*` | SketchObject 执行顺序、profile wire 收集、ExternalGeometry |
| `geometry/face_maker.*` | 多闭合 wire 基础 face-with-holes / island 构面 |
| `topo/element_map.*` | internal element map 基础、InternalFace outer-boundary generated history、InternalEdge split / `EdgeN` / `VertexN` deleted terminal history |
| `topo/subshape_map.*` | `Internal*` subshape 导出 |
| 后续 `geometry/` | 完整 FaceMakerBuildFace / WireJoiner 正式账本 |

## FreeCAD 依据

- `src/Mod/Sketcher/App/SketchObject.cpp`
- `src/Mod/Sketcher/App/SketchObjectGeometry.cpp`
- `src/Mod/Sketcher/App/SketchObjectExternal.cpp`
- `src/Mod/Part/App/FaceMaker*.cpp`
- `src/Mod/Part/App/WireJoiner.cpp`

## 验收

- `fixtures/p5` 覆盖 profile、BSpline profile、mixed/nested closed-wire face-with-holes、closed-wire hole 的 profile / InternalShape 分离、construction、Coincident、已满足的 Horizontal / Vertical orientation constraint、已满足的 Parallel whole-line relation constraint、已满足的 Perpendicular whole-line / line-circle-arc midpoint / endpoint-to-curve / endpoint-to-endpoint / via-point / point-point-line relation constraint、已满足的 Tangent whole-geometry direct / endpoint-to-curve / endpoint-to-endpoint / tangent-via-point relation constraint、已满足的 PointOnObject point-on-curve constraint、已满足的 Symmetric point-point relation constraint、Block fixed-geometry constraint、已满足的 Angle whole-line / endpoint-to-curve / endpoint-to-endpoint / via-point datum constraint、已满足的 Equal line-length / circle-radius constraint、已满足的 line / line-end-pair Distance / DistanceX / DistanceY / Radius / Diameter datum constraint、已满足的 line endpoint fixed coordinate datum、ExternalGeometry edge / vertex / planar face boundary / normal-face single-line projection / whole-shape Face expansion / ExternalTypes Intersection 和 Both、InternalShape bounded split / overlapping / self-intersection / dangling open-wire 子集、InternalFace SubList Pad 和 unsupported Sketcher 能力；点几何当前由 P7 Hole point fixture 约束。
- open profile 不得伪造成 closed face。
- internal name 解析只在 Sketch `InternalShape` 上生效；`InternalFaceN` 可以有 outer-boundary generated history，但 raw `FaceN` 仍不能解析为稳定 `InternalFaceN`。
