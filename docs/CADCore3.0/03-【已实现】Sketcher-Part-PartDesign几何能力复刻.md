# Sketcher / Part / PartDesign 几何能力复刻

## 目标

C3-M3 / C3-M4 / C3-M5 的目标是把 FreeCAD 常用建模语义从“代表 fixture 可跑”推进到“按 Workbench family 可持续复刻”。本主线必须建立在 C3-M1 / C3-M2 的 TopoNaming 与 ExternalGeometry 主路径之上。

## C3-M3：Sketcher 复刻

FreeCAD 依据：

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectGeometry.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectConstraints.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp`

交付内容：

- Sketcher conic arcs 已发布为 P5 支持能力：`ArcOfHyperbola` / `Part::GeomArcOfHyperbola` 与 `ArcOfParabola` / `Part::GeomArcOfParabola` 支持 profile / raw edge 建边、construction 过滤、native `ExternalGeo` 旧几何复用，以及 projected `ExternalGeometry` TopoDS Edge 投影为 construction external curves；证据为 `cad-core/fixtures/p5/sketch-hyperbola-arc-profile.json`、`sketch-parabola-arc-profile.json`、`sketch-conic-arcs-construction-filter.json`、`sketch-conic-arcs-external-geometry-native.json`、`sketch-conic-arcs-external-geometry-projected.json` 及对应 focused tests / FreeCAD expected。完整 Sketcher solver 内部辅助几何 / conic 约束、GUI conic edit、未进入本轮的 Part workbench conic surface 不在该支持声明内。
- solver-facing diagnostics 第一切片已覆盖：同一 target 的 Horizontal / Vertical 冲突、重复 orientation 约束冗余、同一 datum target 不同值冲突、重复 datum 冗余，以及已迁移 orientation / dimension 约束的 malformed index / datum；这些状态在 profile 构造前输出 `sketch_solver_conflict` / `sketch_solver_redundant` / `sketch_solver_malformed_constraint`，并返回 `profile_ready=false`。
- partial redundancy diagnostics 已有 request-local 第一片：依据 FreeCAD `Sketch::analyseBlockedGeometry()` 中 Block 影响其它 driving constraint 的分支，以及 `SketchObject::solve()` 中 `lastHasPartialRedundancies` 只发 warning、不设置错误码的语义，Block 与同一几何上的已迁移一元约束组合输出 `sketch_solver_partially_redundant` warning 和 `solver_partially_redundant_constraints`，profile 继续构造；完整 QR dependent-parameter group 分析仍归入 full DoF / full solver gap。
- 空约束的 request-local sketch geometry 已暴露非阻塞 `solver_state=underconstrained`，profile 仍按 FreeCAD `execute() -> buildShape()` 路径继续构造；后续 DoF metadata 已扩展到简单 scalar 约束第一片，但仍不声明完整 `lastDoF`。
- 整线 Horizontal / Vertical 约束已有 request-local 几何写回第一片，按 FreeCAD `solve(true)` 的 `solvedSketch.extractGeometry()` / `Geometry` 更新语义修改 sketch line segment，再进入 `buildShape()`；这不声明尺寸、关系或端点约束的完整 GCS 求解能力。
- 单端点 `DistanceX/Y` 坐标约束已有 request-local 几何写回第一片，依据 FreeCAD `Sketch::addConstraint()` 中 `point on fixed x-coordinate` / `point on fixed y-coordinate` 路径，只在无端点合并时更新对应 line endpoint 的 X / Y 坐标。
- Circle `Radius/Diameter` 约束已有 request-local 几何写回第一片，依据 FreeCAD `Sketch::addRadiusConstraint()` / `addDiameterConstraint()` 中 Circle 分支更新 circle radius；Arc 半径约束本身仍留给后续完整 solver geometry。
- Line `Distance` 长度约束已有 request-local 几何写回第一片，依据 FreeCAD `Sketch::addDistanceConstraint(int geoId, double* value, bool driving)` 中 Line 分支的 `GCSsys.addConstraintP2PDistance(l.p1, l.p2, value, tag, driving)`，在无端点合并时保持 p1 并沿当前线方向移动 p2。
- Arc `Distance` 长度约束已有 request-local 几何写回第一片，依据 FreeCAD `Sketch::addDistanceConstraint(int geoId, double* value, bool driving)` 中 Arc 分支的 `GCSsys.addConstraintArcLength(a, value, tag, driving)`，并按 `Sketch::updateArcOfCircle()` 写回 `setRadius(*myArc.rad)` / `setRange(*myArc.startAngle, *myArc.endAngle)` 的语义，在保持当前角度范围时缩放 radius；endpoint-coupled arc 与 relation 仍留给后续完整 solver geometry。
- DoF 驱动欠约束状态已有 request-local 第一片：依据 FreeCAD `Sketch::setUpSketch()` / `SketchObject::solve()` 的 `lastDoF` 语义和 `Sketch.h` 中 “positive degrees of freedom correspond to an under-constrained sketch”，对已迁移的简单 2D 参数和 scalar constraints 输出 `solver_degrees_of_freedom` / `solver_dof_status=request_local_first_slice`，并在 DoF 为正时保持 non-blocking `solver_state=underconstrained`；Coincident、relation、Block、ellipse、BSpline、rank / dependent parameters 仍留给完整 GCS DoF。
- PointOnObject 线端点投影已有 request-local relation geometry 第一片，依据 FreeCAD `Sketch::addPointOnObjectConstraint()` 的 Line target 分支 `GCSsys.addConstraintPointOnLine(p1, l2, tag, driving)`，仅把 LineSegment endpoint 投影到目标直线并输出 `solver_relation_geometry_updates`；PointOnObject 到 circle / arc / ellipse 仍留给完整 solver。
- Parallel / Perpendicular 双线关系已有 request-local relation geometry 第一片，依据 FreeCAD `Sketch::addParallelConstraint()` 的 `GCSsys.addConstraintParallel(l1, l2, tag)` 与 `Sketch::addPerpendicularConstraint()` 两条 Line 分支的 `GCSsys.addConstraintPerpendicular(l1, l2, tag)`，保持第一条线不动、保留第二条线起点和长度并旋转到最近的平行或垂直方向，输出 `solver_line_pair_relation_geometry_updates`。
- Perpendicular Line + Circle/Arc midpoint 已有 request-local curve relation 第一片，依据 FreeCAD `Sketch::addPerpendicularConstraint(int,int)` 的 `Points[Geoms[geoId2].midPointId]` 与 `GCSsys.addConstraintPointOnLine(p2, l1, tag)`，保持线、半径和 arc range 不动，把 Circle/Arc center 投影到 Line 上，输出 `solver_curve_relation_geometry_updates`。
- Equal line/circle/arc 已有 request-local relation geometry 第一片，依据 FreeCAD `Sketch::addEqualConstraint()` 的 `GCSsys.addConstraintEqualLength(l1, l2, tag)` 与 `GCSsys.addConstraintEqualRadius` 路径，保持 First 几何不动，更新 Second line 长度或 round radius，输出 `solver_equal_relation_geometry_updates`。
- Tangent Line + Circle/Arc 已有 request-local relation geometry 第一片，依据 FreeCAD `Sketch::addTangentConstraint()` 的 `GCSsys.addConstraintTangent(l, c, ...)` / `GCSsys.addConstraintTangent(l, a, ...)` 路径，保持 line 与 round radius / arc range 不动，把 Circle/Arc center 移到最近 tangent side，输出 `solver_tangent_relation_geometry_updates`。
- Symmetric point-pair about Line / center point / Arc endpoint 已有 request-local relation geometry 第一片，依据 FreeCAD `Sketch::addSymmetricConstraint()` 的 `GCSsys.addConstraintP2PSymmetric(p1, p2, l, tag)` 与 `GCSsys.addConstraintP2PSymmetric(p1, p2, p, tag)` 路径，以及 `Sketch::updateArcOfCircle()` 的 `setRange(*myArc.startAngle, *myArc.endAngle, ...)` 写回语义；保持 First point 与 symmetry line / center point 不动，direct point 场景更新 Second point reference，Arc endpoint 场景保持 center / radius 不动并更新 Second `ArcOfCircle` endpoint 参数，输出 `solver_symmetric_relation_geometry_updates` 以及 line / center 子计数。
- C3-M8 freeze 下，request-local full-rank DoF、dependent / blocked dependent parameter group metadata 与 Symmetric + Tangent coupled relation geometry update 已收口；原 `full DoF / dependent group / coupled relation` 不再作为当前 remaining gap。若后续要引入真实 GCS session 或更多 curve relation，必须作为新的 solver producer / lifecycle 项拆分，不能恢复旧的笼统 solver gap。
- ExternalGeometry projection / intersection 复杂路径若继续扩展，按 face / edge / vertex / datum / linked target / missing target 拆成新的 fixture 和 diagnostics；不把当前已收口的 native lifecycle 回退成 broad gap。
- InternalShape 的 bounded face + open wire 混合场景已有 oracle；复杂 self-intersection、solver-facing split diagnostics 和更多 inter-edge split 组合若继续扩展，必须继续走 FaceMaker / WireJoiner producer evidence 与 MapperHistory，不在 Sketch executor 里补输出规则。

完成判定：

- Sketcher fixture 不只覆盖 profile 成功，还覆盖 solver state、diagnostics、ExternalGeometry flags update 和 stable subname。
- open profile / open wire 不伪装为 profile-ready face；solver conflict / redundant / malformed 不继续生成 fake profile。
- InternalFace / InternalEdge / InternalVertex 仍只由 producer evidence / MapperHistory / diagnostics 解释。

## C3-M4：Part Workbench 复刻

FreeCAD 依据：

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PartFeature.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeatureOffset.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/modelRefine.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMaker.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureExtrusion.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp`

交付内容：

- 系统覆盖 Part primitives：Box、Cylinder、Cone、Sphere、Torus、Plane、Line、Circle、Ellipse 等。
- 系统覆盖 Part operations：Boolean、Extrude、Revolve、Sweep、Loft、Section、Offset、Thickness、Refine、Compound、CompoundFilter。
- PARTCONIC 已发布 Part geometry Hyperbola / Parabola 第一批能力：`Part.Hyperbola` / `Part.Parabola` geometry wrapper 通过请求级 `PartConicCurveDTO` 转为有限 `TopoDS_Edge`，保留 `GeomAbs_Hyperbola` / `GeomAbs_Parabola` 与 `Part.Hyperbola` / `Part.Parabola` metadata，并由 `partGeometryCurveConsumers` 证明 Hyperbola / Parabola edge 可进入现有 `Part::Extrusion` consumer 输出 `occt_face`。FreeCAD 依据是当前源码 `Geometry.cpp::GeomHyperbola/GeomParabola/GeomArcOf*::Save/Restore()`、`PrimitiveFeature.cpp` 中不存在 `Part::Hyperbola` / `Part::Parabola` primitive、`FeatureExtrusion.cpp::Extrusion::extrudeShape()` regular path `result.makeElementPrism(myShape, vec)`；cad-core 落点为 `cad-core/src/part/part_geometry_curve.cpp`、`cad-core/include/cad_core/part/part_geometry_curve.h`、`cad-core/tools/collect_freecad_expected.py`、`cad-core/tests/test_p8_features.py`、`cad-core/tests/test_diagnostics.py` 和 `cad-core/src/adapters/c_api/c_api.cpp` capability metadata。验收 fixture 为 `cad-core/fixtures/p8/part-hyperbola-edge.json`、`part-parabola-edge.json`、`part-conic-edge-invalid-params.json`、`part-conic-edge-extrusion.json` 及对应 FreeCAD expected；发布口径不包含 `Part::Hyperbola` / `Part::Parabola` `DocumentObject` executor、GUI conic edit、完整 Sketcher solver conic constraints、DistanceType default/TODO、BREP / polyline / BSpline 替代 typed conic；surface family 边界由独立 `part_workbench.*` capability 维护。
- PARTSURF 已发布 `Part::RuledSurface` expected-backed 能力：source-backed executor 通过 `Curve1` / `Curve2` `App::PropertyLinkSub` 读取源 edge / wire，支持 `Orientation=Automatic/Forward/Reversed`，按 FreeCAD `TopoShape::makeElementRuledSurface()` 的 edge 分支调用 `BRepFill::Face`、wire 分支调用 `BRepFill::Shell`，并记录 source edge / wire provenance。cad-core 落点为 `cad-core/src/part/part_ruled_surface.cpp`、`cad-core/src/part/topo_shape_expansion.cpp`、`cad-core/src/runtime/feature_registry.cpp`、`cad-core/tools/collect_freecad_expected.py` 和 capability metadata；验收 fixture 为 `part-ruled-surface-line-line`、`part-ruled-surface-conic-line`、`part-ruled-surface-orientation-reversed`、`part-ruled-surface-invalid-input`、`part-ruled-surface-wire-wire`。发布口径不包含完整 Part surface family；`ProjectOnSurface` 已拆到独立 capability。
- PARTSURF 已发布 `Part::ProjectOnSurface` 当前 expected-backed/source-backed slice：source-backed executor 读取 `SupportFace=App::PropertyLinkSub` 与 ordered `Projection=App::PropertyLinkSubList`，覆盖 `Mode=Edges/Faces/All`、face rebuild / hole wires、`Mode=All` Height solid、Offset placement、多 Projection ordered LinkSubList、普通 indexed `NamedShape`、稳定 diagnostics，以及 C5-M9 edge / wire / face / all provenance、Projection item ledger、MapperHistory / ElementMap / reference recovery hook。FreeCAD 依据是 `FeatureProjectOnSurface.cpp::tryExecute()` 的 `getSupportFace()` -> `getProjectionShapes()` -> `createProjectedWire()` -> `projectWire()` / `projectFace()` -> `filterShapes()` -> `createCompound()`，以及 `createFaceFromParametricWire()` / `createSolidIfHeight()` / `getOffsetPlacement()`；cad-core 落点为 `cad-core/src/part/part_project_on_surface.cpp`、`cad-core/src/part/topo_shape.cpp`、`cad-core/src/part/topo_shape_mapper.cpp`、`cad-core/include/cad_core/part/topo_shape_mapper.h`、`cad-core/src/runtime/feature_registry.cpp`、`cad-core/tools/collect_freecad_expected.py`、`cad-core/tests/test_p8_features.py`、`cad-core/tests/test_expected_fixtures.py` 和 `cad-core/src/adapters/c_api/c_api.cpp` capability metadata。验收 fixture 为 12 个 `part-project-on-surface-*` c4m1 cases、5 个 c5m9 provenance cases 及对应 expected / source-backed known_gap / diagnostic-backed focused tests；发布口径不包含 GUI projection task panel、未验证高级分支、完整 `ProjectOnSurface` 或完整 Part surface family。`native_project_on_surface_mapper_history_hidden_until_probe` 保留为 source-backed request-local 边界，而不是 broad remaining gap。
- PARTSURF 已发布 `Part::Loft` 与 `Part::Sweep` / PipeShell 当前 expected-backed slice：Loft 覆盖 `Sections`、`Solid`、`Ruled`、`Closed`、`MaxDegree`、`Linearize=true`、face / vertex profile、`BRepOffsetAPI_ThruSections` 和 `loft_thru_sections` maker history；Sweep 覆盖 `Sections`、`Spine`、`Solid`、`Frenet`、`Transition`、`Linearize=true`、multi-profile `Sections`、Spine SubList compound 和 PipeShell maker history。验收 fixture 为五个 c3m4 `part-loft-*`、两个 c4m1 Loft profile / Linearize cases、八个 c3m4 `part-sweep-*`、一个 c4m1 Sweep multi-profile / Linearize case 及对应 expected / focused tests；`part-sweep-advanced-deferred` 只证明 advanced wrapper 属性输出 locatable diagnostics。发布口径不包含复杂 profile family、advanced PipeShell wrapper contract 或完整 Part surface family；剩余 owner 分别为 `future_loft_complex_profile_family` 与 `future_sweep_advanced_contract`。
- PARTSURF 已发布 `Part::FilledFace` helper 第一批 + C5-M8 收口能力：它是 cad-core 对 FreeCAD `Part.makeFilledFace()` 的 source-backed helper parity，不是原生 FreeCAD `DocumentObject`。cad-core 通过 `Boundary` / `App::PropertyLinkSubList` 读取源 wire/edge/face/vertex，按 `TopoShape::makeElementFilledFace()` 使用 `BRepOffsetAPI_MakeFilling`，发布 `maker_history:filling`、boundary source evidence、`Surface` -> `LoadInitSurface` source evidence、boundary / non-boundary edge `Supports` face map、G1 `Orders` map、constructor params metadata、non-boundary constraint source evidence、compound optional expected-backed source expansion、direct wrapper / UV point-on-support `unsupported_wrapper_lifecycle` diagnostics 和 locatable diagnostics；验收 fixture 为 `part-filling-closed-wire-default`、`part-filling-boundary-edges-default`、`part-filling-invalid-inputs`、`c5m8/part-filling-initial-surface-boundary`、`c5m8/part-filling-support-order-edge-face`、`c5m8/part-filling-invalid-support-order`、`c5m8/part-filling-non-default-params`、`c5m8/part-filling-param-diagnostics`、`c5m8/part-filling-non-boundary-edge-support`、`c5m8/part-filling-non-boundary-face-point`、`c5m8/part-filling-non-boundary-wire`、`c5m8/part-filling-non-boundary-diagnostics`、`c5m8/part-filling-compound-optional-boundary`、`c5m8/part-filling-wrapper-boundary`、`c5m8/part-filling-wrapper-uv-point-boundary`。发布口径不包含 expected-backed surface/support/order parity、G2 stable geometry、expected-backed explicit non-default params geometry、non-boundary edge support/order native expected、native `Part::FilledFace` DocumentObject、Surface Workbench GUI/native feature、cross-request mutable `Part.BRepOffsetAPI.MakeFilling` wrapper 或完整 Part surface family。
- PARTSURF 已发布 `Part::GeomPlateSurface` helper 第一批能力：它是 cad-core 对 FreeCAD `Part.GeomPlate.BuildPlateSurface` 的 source-backed geometry helper parity，不是 GUI feature 或原生 FreeCAD `DocumentObject`。cad-core 通过 `CurveConstraints` / `PointConstraints` 读取 3D edge G0 constraints 与 3D point vectors，记录 build params / approximation metadata，并输出 `GeomPlate_Surface` approximation face；验收 fixture 为 `part-geomplate-curve-point-default`、`part-geomplate-invalid-inputs`。发布口径不包含 initial surface reference、G1 curve-on-surface、projected 2D curve、2D point-on-surface、custom constraint criteria、`Part.PlateSurface.Curves` wrapper、Filling 扩展或完整 Part surface family。
- `Part::Offset` 面源第一切片已覆盖：`Source`、`Value`、`Mode`、`Join`、`Intersection`、`SelfIntersection`、`Fill=false` 进入 `part::makeElementOffsetFromSource()`，并通过 maker history 输出 `Plane.Face/Edge/Vertex -> Offset.*`；`Fill=true` 进入 FreeCAD free-bound perimeter + sewing 路径并暴露 `part_offset_fill:sewing_history`；solid source 不再被 executor 拦截，shell / compsolid `makeElementSolid()` helper 暴露 `part_make_solid:make_element_solid`；`Part::Offset2D` planar face / no-fill、closed source/result wires fill、Edge/Wire no-fill 与单 open-wire fill 均进入 `part::makeElementOffset2DFromSource()` 的 Part 层 MakeOffsetFix/FaceMaker 路径，并分别暴露 `part_offset2d:face_no_fill_makeoffset`、`part_offset2d:face_fill_closed_makeoffset`、`part_offset2d:wire_no_fill_makeoffset`、`part_offset2d:wire_fill_open_makeoffset`；`Part::Compound` Links executor 与 `Part::Offset2D` compound `Intersection=false` child recursion 已暴露 `part_compound:make_element_compound` / `element_map_child_map:preserve_source_ranges` / `part_offset2d:compound_child_recursive`，嵌套 Compound 已按 ElementMap grand child map resolution 暴露 `element_map_child_map:recursive_source_ranges`，并按 `hashChildMaps()` 暴露 `element_map_child_map:hashed_child_map_keys`；`Part::Offset2D` compound `Intersection=true` 已按 FreeCAD collective 分支收集非 compound children 后一次性 MakeOffsetFix，并暴露 `part_offset2d:compound_collective_makeoffset`；`Part::Thickness` single-solid FaceN 第一片进入 `part::makeElementThickSolidFromSource()`，按 `BRepOffsetAPI_MakeThickSolid` 输出 `part_thickness:make_thick_solid`，并已覆盖 `Mode=RectoVerso` / `Join=Tangent` 按 FreeCAD 规则转为 `effective_join=Intersection` 的 oracle。
- `Part::Section` 稳定 history 第一切片已覆盖：`FeaturePartSection.cpp::Section::makeOperation()` 读取 `Approximation`，`FCBRepAlgoAPI_Section::setAutoFuzzy()` 按输入 bbox 设置 fuzzy value，`Part::Boolean::execute()` 消费 `makeElementShape` history；`cad-core` 输出 `Plane.Edge* -> Section.Edge*` source-qualified history 与 `Box.Edge*` terminal deleted history。
- import/export 建立 ElementMap 或明确不可恢复 diagnostics。
- ShapeFix、BOPCheck、invalid shape diagnostics 进入统一 runtime 输出。

完成判定：

- Part feature 不只输出 shape，还输出可追溯 `NamedShape`、subshape map、ElementMap / MapperHistory 或明确 diagnostics。
- import shape 的拾取引用能在可恢复范围内跨 recompute 保持稳定。
- OCCT 失败、退化、空 shape、unsupported format 都有稳定 diagnostics。

## C3-M5：PartDesign 完整生态

FreeCAD 依据：

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/Body.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/Feature.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureAddSub.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeaturePad.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeaturePocket.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp`

交付内容：

- full transformed / pattern history 已覆盖主路径：Mirror、LinearPattern、PolarPattern、Scaled、MultiTransform 的 Features / WholeShape、multi-original、chain DressUp、refined support 与 Link retag 组合。
- Pad / Pocket / Part::Extrusion taper history metadata 已收口：FreeCAD `FeatureExtrude.cpp` / `FeatureExtrusion.cpp` 的 taper 分支都调用 `ExtrusionHelper::makeElementDraft()`，`cad-core` 由 `part::makeTaperedExtrusion()` 和 `namedShapeForTaperedExtrusionHistory()` 消费 `BRepOffsetAPI_ThruSections` maker history；P3b / P5 taper fixture 输出 `topo_naming_history=maker_history:taper_thru_sections` 且不再输出旧 known-gap topo metadata。
- Pad / Pocket `UpToShape` 多面 LinkSubList 已覆盖当前主路径：`FeatureExtrude.cpp` 解析 `App::PropertyLinkSubList`，按 FreeCAD `getUpToShapeFromLinkSubList()` 收集 face / shape selection，并用 `findAllFacesCutBy()` 的 cut-face reach 生成 Pad / Pocket tool；`pocket-up-to-shape-multi-face` 与 `pad-up-to-shape-multi-face` 为成功验收，offset 多面和 Edge selection 仍输出稳定 diagnostics。
- DressUp 复杂参数：Fillet / Chamfer 的 Edge / Face 多选择已记录请求 subname 到实际 EdgeN 的展开证据；Chamfer `Two distances` / `Distance and Angle`、Draft no-face copy / 显式 DatumPlane-Line / 自动 neutral-plane guess、PartDesign Thickness no-face copy / FaceN Mode-Join 参数变体 / multi-solid fuse history 已有结构化 fixture；empty / invalid / unsupported Base selection 与 invalid parameter 失败诊断已有结构化 fixture；chain DressUp + Pattern history 已覆盖 SupportTransform AddSubShape cache、source-prefixed alias 和 terminal split/deleted 传播。
- Body chain ownership 已覆盖外部 `Body.BaseFeature` 触发内部 `PartDesign::FeatureBase` 创建、`Body.Group` 头部同步、首个 solid feature `BaseFeature` 回写、删除旧 Tip 后 previous / next `Body.Tip` 与下一 solid feature `BaseFeature` reroute、Origin placement、Origin datum relink 与 Add/Sub replay stop-at-tip；这些写回都保持 request graph immutable。
- Hole 完整表驱动已覆盖：ThreadType / ThreadSize 表驱动直径、`Resources/Hole` head cut definition、ISO / DIN 动态 head cut、ModelThread pipe-shell 几何、threaded ModelThread clearance、point profile + counterbore、`findHoles()` profile Edge/Vertex -> tool Face mapper history、`protoHole` / `protoThread` compound tool shape，以及 subtractive Body cut history。`hole-supported-model-thread-counterbore` 已作为 threaded + ModelThread + head cut native oracle 参与 expected 拓扑/体积回归。
- Revolution、Groove、Loft、Pipe、Boolean、Datum attachment 或 Body visibility / group 扩展若继续推进，作为新的 PartDesign family gap 独立拆分；不复用已收口的 taper / Body chain / Hole gap 名称。

完成判定：

- 常用 PartDesign workflow 可以长期编辑：修改 sketch、切换参数、重算 Body、下游引用仍可恢复或稳定诊断。
- transformed / pattern 不靠 instance index、bbox、面积或输出顺序猜 source ownership。
- unsupported 参数组合明确进入 capabilities / diagnostics，而不是静默失败。

## 验收命令

本主线代码修改后优先执行：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_feature_flows tests.test_p5_sketch tests.test_p6_topology tests.test_p7_features
```

阶段收口时补：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
python3 -m unittest tests.test_expected_fixtures
```
