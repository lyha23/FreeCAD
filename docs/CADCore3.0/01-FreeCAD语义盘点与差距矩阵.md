# FreeCAD 语义盘点与差距矩阵

## 目标

C3-M0 的目标是先建立“要复刻什么”的权威矩阵，再进入实现。矩阵不是 feature 清单，而是 FreeCAD 源码语义、cad-core 分层落点、oracle 和 current gap 的统一账本。

没有进入矩阵的能力，不进入 3.0 主线实现；已经进入矩阵但无法立即实现的能力，必须给出 `known_gap`、diagnostic policy 或后续阶段。

## 矩阵字段

每个条目至少包含：

| 字段 | 含义 |
| --- | --- |
| FreeCAD 模块 | `src/App`、`src/Mod/Sketcher/App`、`src/Mod/Part/App`、`src/Mod/PartDesign/App`、`src/Mod/Assembly/App` |
| FreeCAD 源码依据 | 绝对路径、类 / 函数、关键字段或短句 |
| 语义对象 | DocumentObject、Property、Feature、Maker、Mapper、Solver、Adapter contract |
| cad-core 落点 | `app`、`base`、`graph`、`runtime`、`sketcher`、`part`、`part_design`、`mesh`、`assembly`、`adapters` |
| 当前状态 | `done`、`partial`、`diagnostic_only`、`known_gap`、`not_started` |
| oracle 来源 | FreeCADCmd collector、expected fixture、unit test、C ABI contract、manual probe |
| 验收方式 | build/test 命令、expected fields、diagnostic codes、capabilities 字段 |
| 非目标 / 边界 | GUI、session、BREP 持久化、adapter 语义等明确排除项 |

## 模块盘点范围

### `src/App`

优先盘点：

- `Document` / `DocumentObject` 生命周期、依赖、recompute。
- `PropertyLinks.cpp`、`PropertyGeo.cpp`、`GeoFeature.cpp`。
- `ElementMap.cpp`、`MappedName.cpp`。
- `Link.cpp` / `Link.h` 的 LinkBaseExtension、ShowElement、ElementList、`_ChildCache`、copy-on-change。

cad-core 落点：

- `app/document.*`、`app/document_object.*`、`app/property*.*`：输入模型、属性解析、link schema。
- `graph/*`：依赖、循环、recompute target。
- `runtime/recompute.*`：执行计划、写回建议、diagnostics。
- `app/element_map.*`、`part/topo_shape.*`、`part/topo_shape_expansion.*`：stable key、mapped postfix、reference update。
- `app/link.*`：Link / LinkSub / LinkGroup / Assembly display。

### `src/Mod/Sketcher/App`

优先盘点：

- `SketchObject.cpp`、`SketchObjectGeometry.cpp`、`SketchObjectConstraints.cpp`。
- `SketchObjectExternal.cpp`、`ExternalGeometryExtension.cpp`。
- solver-facing geometry / constraints、ExternalGeometry projection / intersection、InternalShape。

cad-core 落点：

- `sketcher/sketch_object*.*`：SketchObject 调用顺序和属性语义。
- `part/face_maker.*`、`part/wire_joiner.*`：InternalShape producer evidence。
- `part/topo_shape.*`、`part/property_topo_shape.*`：InternalFace / InternalEdge / InternalVertex naming。

### `src/Mod/Part/App`

优先盘点：

- `TopoShape.cpp`、`TopoShapeExpansion.cpp`、`TopoShapeMapper.cpp`。
- `PropertyTopoShape.cpp`、`BodyBase.cpp`。
- `FaceMaker*.cpp`、`WireJoiner.cpp`。
- `modelRefine.cpp`、ShapeFix、Part Boolean、Part primitives、import/export。

cad-core 落点：

- `part/topo_shape.*`、`part/shape_exporter.*`、`part/part_import.*`：OCCT maker、shape construction、mesh、bbox、volume、file import/export。
- `part/topo_shape.*`、`part/property_topo_shape.*`：MapperHistory、ElementMap、ShapeFix history、import shape history。
- `part/part_feature.*`、`part_design/feature_*.*`、`mesh/feature_mesh_import.*`：Part::Feature、Part Boolean、Part primitive executor。

### `src/Mod/PartDesign/App`

优先盘点：

- `Body.cpp`、`Feature.cpp`、`FeatureAddSub.cpp`、`FeatureExtrude.cpp`。
- `FeaturePad.cpp`、`FeaturePocket.cpp`、`FeatureTransformed.cpp`。
- Mirror、LinearPattern、PolarPattern、Scaled、MultiTransform。
- Hole、Fillet、Chamfer、Draft、Thickness、Groove、Revolution、Loft、Pipe、Boolean、Datum。

cad-core 落点：

- `part_design/feature_extrude.*`、`part_design/feature_pad.*`、`part_design/feature_pocket.*`、`part_design/feature_transformed.*`、`part_design/feature_dress_up.*`、`part_design/feature_fillet.*`、`part_design/feature_chamfer.*`。
- `part/extrusion_helper.*`、`part/refine_model.*`、新增对应 maker helper。
- `part/topo_shape.*`、`part/topo_shape_expansion.*`：maker history 消费与 terminal history 传播。

### `src/Mod/Assembly/App`

优先盘点：

- `AssemblyObject.cpp`、`JointObject.cpp`、`JointGroup.cpp`。
- GroundedJoint、Fixed、Revolute、Slider、Ball、Distance、Angle 等 JointType。
- Ondsel solver 输入、placement update、失败 diagnostics。

cad-core 落点：

- `app/link.*` 或后续 `assembly/assembly_object.*`、`assembly/joint_group.*`：AssemblyObject / Joint 输入。
- `runtime/*`：solver 调度与 `documentObjectUpdates`。
- `adapters/*`：统一暴露 solver diagnostics，不承载求解语义。

## gap 分级

| 优先级 | 定义 | 处理方式 |
| --- | --- | --- |
| P0 | 会破坏稳定引用、错误 recompute 或前端长期编辑 | 必须进入 C3-M1 / C3-M2 |
| P1 | 常用 Sketch / Part / PartDesign 建模链缺失或 history 不完整 | 进入 C3-M3 / C3-M4 / C3-M5 |
| P2 | Link / Assembly / Web 产品化能力缺失 | 进入 C3-M6 / C3-M7 |
| P3 | 低频 feature、GUI-only 行为、非核心 Workbench | 只记录，不阻塞 3.0 freeze |

## C3-M0 交付

C3-M0 完成后应新增或更新：

- `docs/CADCore3.0/FreeCAD语义矩阵.md`：完整矩阵。
- `docs/CADCore3.0/capabilities-gap对照表.md`：当前 `cad-core` capabilities、remaining gaps、对应 C3 阶段和暴露方式。
- `docs/CADCore3.0/oracle-fixture队列.md`：新增 oracle、待迁移 known gap、可删除旧 skipped 和第一批可执行 fixture。
- `FreeCADCmd` collector 队列：需要采集的 native oracle case。

## C3-M0 验收

必须满足：

- 每个 P0 / P1 gap 都能追溯到 FreeCAD 源码路径和 cad-core 落点。
- 每个 gap 都有阶段归属，不存在“泛化 complete_mapper_history”这类不可执行占位。
- 2.0 已完成项不重复立项；只在 3.0 中引用为基线。
- 文档只保留当前基线和后续队列，不记录过程流水账。
