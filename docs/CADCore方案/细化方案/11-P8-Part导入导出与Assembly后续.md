# P8：Part、导入导出与 Assembly 后续

P8 已覆盖 FreeCAD `Part::Primitive` 与常用 Part Boolean 子集。导入导出、Assembly 和产品化 adapter 仍保持后置。

## 当前基线

- `Part::Box` 已注册 executor，按 `Length` / `Width` / `Height` 构造 OCCT solid，默认值和过小尺寸校验对齐 FreeCAD `Box::execute()`。
- `Part::Cylinder` 已注册 executor，按 `Radius` / `Height` / `Angle` 构造圆柱底面，再按 `PrismExtension` 的 `FirstAngle` / `SecondAngle` 拉伸成 solid。
- `Part::Prism` 已注册 executor，按 `Polygon` / `Circumradius` 构造正多边形底面，再按 `Height` 和 `PrismExtension` 的 `FirstAngle` / `SecondAngle` 拉伸成 solid。
- `Part::RegularPolygon` 已注册 executor，按 `Polygon` / `Circumradius` 构造正多边形 wire。
- `Part::Sphere` 已注册 executor，按 `Radius` / `Angle1` / `Angle2` / `Angle3` 调用 FreeCAD 同款 `BRepPrimAPI_MakeSphere` 参数。
- `Part::Ellipsoid` 已注册 executor，按 `Radius1` / `Radius2` / `Radius3` 和角度参数构造 sphere 后用 `BRepBuilderAPI_GTransform` 缩放。
- `Part::Cone` 已注册 executor，按 `Radius1` / `Radius2` / `Height` / `Angle` 调用 `BRepPrimAPI_MakeCone`，两端半径相等时退化为 cylinder 路径。
- `Part::Torus` 已注册 executor，按 `Radius1` / `Radius2` / `Angle1` / `Angle2` / `Angle3` 走 FreeCAD `TopoShape::makeTorus()` 的圆面旋转路径。
- `Part::Wedge` 已注册 executor，按 `Xmin` / `Ymin` / `Zmin` / `X2min` / `Z2min` / `Xmax` / `Ymax` / `Zmax` / `X2max` / `Z2max` 构造 `BRepPrim_Wedge` solid。
- `Part::Vertex` 已注册 executor，按 `X` / `Y` / `Z` 构造 OCCT vertex。
- `Part::Line` 已注册 executor，按 `X1` / `Y1` / `Z1` 和 `X2` / `Y2` / `Z2` 构造 OCCT edge。
- `Part::Ellipse` 已注册 executor，按 `MajorRadius` / `MinorRadius` / `Angle1` / `Angle2` 构造 OCCT edge。
- `Part::Plane` 已注册 executor，按 `Length` / `Width` 构造位于 XY 平面的 OCCT face。
- `Part::Helix` 已注册 executor，按 `Pitch` / `Height` / `Radius` / `Angle` / `SegmentLength` / `LocalCoord` 走 FreeCAD `TopoShape::makeSpiralHelix()` 路径构造 wire。
- `Part::Spiral` 已注册 executor，按 `Growth` / `Radius` / `Rotations` / `SegmentLength` 走 FreeCAD `TopoShape::makeSpiralHelix()` 路径构造 wire。
- `Part::Fuse` / `Part::Cut` / `Part::Common` 已注册 executor，按 `Base` / `Tool` 链接读取请求内 shape，走 `topo::makeElementBooleanFromSources()` 的 OCCT maker history 主路径，输出 mesh / subshape / `NamedShape`，并在 `Refine=true` 时使用当前 RefineModel 子集。
- `Part::Section` 已注册 executor，按 `Base` / `Tool` 链接和 `Approximation` 构造 section edges compound，走 `topo::makeElementSectionFromSources()` 的 maker history 主路径。
- `Part::MultiFuse` / `Part::MultiCommon` 已注册 executor，按 `Shapes` 链接列表读取请求内 shape；`MultiFuse` 支持单 compound 输入展开到子 shape，`MultiCommon` 默认按 `CommonOfAllShapes` 逐步求交，并保留 `CommonOfFirstAndRest` 兼容行为。
- `Part::XOR` / `Part::FeatureXOR` 已作为 BOPTools `FeatureXOR` typed alias 注册 executor，按 `Objects` 链接列表读取请求内 shape，走 `topo::makeElementXorFromSources()` 的 Fuse / Common / Cut 主路径；当前支持 `Tolerance=0`。
- `Part::BooleanFragments` / `Part::FeatureBooleanFragments` 已作为 BOPTools `FeatureBooleanFragments` typed alias 注册 executor，按 `Objects` 链接列表读取请求内 shape，支持 `Mode=Standard`、`Mode=Split` 的 solid-safe / wire aggregate / CompSolid aggregate 子集、`Mode=CompSolid` 和非负 `Tolerance`，走 `topo::makeElementGeneralFuseFromSources()` 的 generalFuse maker history 主路径；wire aggregate Split 对齐 FreeCAD `GeneralFuseResult.makeSplitPieces()` 的 Wire 分支，按 split vertices 重建 wire pieces；CompSolid aggregate Split 对齐同一入口的 CompSolid 分支，按 split faces 将 solid children 重新组合为 compsolid pieces；Shell aggregate pieces 仍保持显式 unsupported diagnostics；`Mode=CompSolid` 按 FreeCAD `ShapeMerge.mergeSolids(..., bool_compsolid=True)` 将 solid pieces 按共享面分组为 compound of compsolids。
- Part primitive 输出 mesh、subshape map、bbox、volume、kernel metadata 和 indexed `NamedShape`；Part Boolean 输出 maker-history derived `NamedShape`。
- `fixtures/p8` 已覆盖 `part-box`、`part-cylinder`、`part-cylinder-angled-prism`、`part-prism`、`part-regular-polygon`、`part-sphere`、`part-ellipsoid`、`part-cone`、`part-torus`、`part-wedge`、`part-vertex`、`part-line`、`part-ellipse`、`part-plane`、`part-helix`、`part-spiral`、`part-fuse`、`part-cut`、`part-common`、`part-section`、`part-multi-fuse`、`part-multi-common`、`part-multi-common-first-rest`、`part-xor`、`part-boolean-fragments`、`part-boolean-fragments-split`、`part-boolean-fragments-wire-split`、`part-boolean-fragments-compsolid` 和 `part-boolean-fragments-compsolid-split`。

## 目标范围

- Part primitives：Box、Cylinder、Prism、RegularPolygon、Sphere、Ellipsoid、Cone、Torus、Wedge、Vertex、Line、Ellipse、Plane、Helix、Spiral 已接入。
- Part Boolean：Fuse、Cut、Common、Section、MultiFuse、MultiCommon、BOPTools FeatureXOR、BOPTools BooleanFragments Standard / solid-safe Split / wire aggregate Split / CompSolid aggregate Split / CompSolid mode 已接入；BooleanFragments Shell aggregate Split 后处理仍待迁移。
- 文件导入导出：STEP / BREP / STL 等 adapter 能力。
- Assembly：Link、Joint、约束求解和装配 recompute。
- 产品化 adapter：Worker、WASM、Web service bridge。

## 边界

- 文件导入导出可以处理 BREP，但 BREP 不进入持久 `DocumentObject graph` 的默认状态模型。
- Web / Worker / WASM 只做 adapter，不改变 CAD Core 无状态边界。
- Assembly 不应绕过 topo naming；Link / Joint 的 subname 和 placement 仍需要稳定引用模型。
- 当前 Part primitive 仍使用 indexed `NamedShape`；Part Boolean 已消费 boolean / section / generalFuse maker history，完整 primitive maker history、BooleanFragments Shell aggregate Split history、当前 aggregate Split partial history 向完整 MapperHistory 收敛，以及导入 shape 的 ElementMap 仍属于 P6/P8 后续工作。

## 前置条件

- P6 MapperHistory、split / merge 旧引用恢复和 ShapeFix / Refine history 足够稳定。
- P5 Sketcher external geometry 和 internal element map 能支撑常用引用。
- P7 Body 生态不再依赖高层 fixture 特判。
- CLI / C ABI 对同一 fixture 的核心结果一致。

## 规划落点

| 能力 | cad-core 落点 |
| --- | --- |
| Part primitives | 当前落在 `features/part.cpp`，后续复杂 primitive 可拆到 `geometry/primitives.*` |
| Part Boolean | `features/part_boolean.*` + `topo/named_shape.*` |
| Import / Export | `adapters/` 和可选 `geometry/io.*` |
| Assembly Link / Joint | `features/assembly_*`、`document/` link 扩展、`graph/` |
| Worker / WASM / Web | adapter 层 |

## 剩余缺口

- BooleanFragments Shell aggregate Split 尚未完整迁移 `GeneralFuseResult.splitAggregates()` 对 Shell pieces 的后处理；嵌套 compound 中若出现 shell aggregate，仍保持显式 unsupported diagnostics。
- STEP / BREP / STL 导入导出还没有 adapter 能力。
- Assembly Link / Joint、placement chain 和装配求解未迁移。
- Worker / WASM / Web service bridge 未产品化。

## 验收

- 每个 Part / Assembly `TypeId` 有明确 executor 或 diagnostics。
- 文件导入导出不污染无状态核心边界。
- Link / Joint placement 和 stable subname 不靠前端猜测。
- Worker / WASM / Web adapter 与 CLI / C ABI 复用同一 core recompute。
