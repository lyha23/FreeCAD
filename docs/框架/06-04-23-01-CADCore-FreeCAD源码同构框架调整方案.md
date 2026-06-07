# CAD Core FreeCAD 源码同构框架调整方案

本文参考 `/Users/li/Chili3DProject/重构Chili/opencascade-rs/docs/框架/5-28-10-17-FreeCAD迁移框架重构方案.md`，把当前 `cad-core` 的长期结构调整方向改为更接近本地 FreeCAD 源码，而不是继续按一般后端或几何库分层扩展。

本地 FreeCAD 依据统一使用 `/Users/li/Chili3DProject/重构Chili/FreeCAD`。

## 结论

当前 `cad-core` 已有 `document`、`graph`、`runtime`、`features`、`geometry`、`topo`、`adapters` 分层，这对早期 MVP 很有效，但继续扩大到完整 FreeCAD 几何库时会出现两个问题：

- `features/` 变成 Part、PartDesign、Sketcher、App Link、Mesh、Assembly 的混合目录，文件名不能直接回到 FreeCAD 源文件。
- `geometry/` 和 `topo/` 容易承接过多 FreeCAD 业务语义，例如 FaceMaker、WireJoiner、TopoShapeExpansion、ElementMap history，本质上它们不是普通低层 helper。

长期目标应调整为：

- `app/` 对齐 FreeCAD `src/App`：文档对象、属性、链接、GeoFeature、ElementMap、MappedName、Link。
- `part/` 对齐 FreeCAD `src/Mod/Part/App`：Part::Feature、PropertyTopoShape、TopoShape、TopoShapeExpansion、TopoShapeMapper、FaceMaker、WireJoiner、Attachment、Part primitives、Part boolean/import/export。
- `part_design/` 对齐 FreeCAD `src/Mod/PartDesign/App`：Body、Feature、FeatureAddSub、FeatureExtrude、Pad、Pocket、Transformed、Pattern、Mirror、Hole、DressUp、Datum。
- `sketcher/` 对齐 FreeCAD `src/Mod/Sketcher/App`：Sketch、SketchObject、SketchObjectGeometry、SketchObjectExternal、SketchObjectConstraints、SketchObjectOperations。
- `mesh/` 对齐 FreeCAD `src/Mod/Mesh/App`：MeshFeature、FeatureMeshImport 等 Mesh 模块对象。
- `assembly/` 对齐 FreeCAD `src/Mod/Assembly/App`：AssemblyObject、AssemblyLink、JointGroup、求解诊断。
- `base/` 只保留 FreeCAD Base 风格的无状态基础类型，例如 Placement、Matrix、Vector 等值语义。
- `graph/`、`runtime/`、`adapters/` 继续存在，但只做无状态请求计划、执行调度和协议转换，不承载 FreeCAD 业务规则。

一句话：`cad-core` 的目录结构要让开发者看到一个文件名就能定位到本地 FreeCAD 对应源码，而不是先猜它属于 executor、helper、mapper 还是 exporter。

## FreeCAD 源码依据

### App 层

本地依据：

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/CMakeLists.txt`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/DocumentObject.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyGeo.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/GeoFeature.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/ElementMap.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/MappedElement.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/MappedName.cpp`

判断：

- 文档对象身份、属性容器、PropertyLinkSub / PropertyLinkSubList、引用更新、Link 展开、ElementMap 名称关系都属于 App 基础设施。
- `cad-core` 的无状态 JSON 请求不改变这个归属；它只是把 FreeCAD Document 运行期对象压平成一次请求输入。

### Part 层

本地依据：

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/CMakeLists.txt`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PartFeature.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/BodyBase.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PropertyTopoShape.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMaker.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBullseye.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/AttachExtension.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/Attacher.cpp`

判断：

- `TopoShape` / `TopoShapeExpansion` / `TopoShapeMapper` 是 Part 的核心语义，不应长期作为通用 `topo` 目录下的无来源工具。
- FaceMaker / WireJoiner 在 FreeCAD 中本来就是 Part/App 下的大状态机，不应因为文件大就拆散账本。
- Part primitives、Part Boolean、Import/Export 应落在 Part 模块，而不是和 PartDesign Pad/Pocket/Hole 混在同一个 `features/` 目录。

### PartDesign 层

本地依据：

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/CMakeLists.txt`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/Body.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/Feature.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureAddSub.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeaturePad.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeaturePocket.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureLinearPattern.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureMirrored.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeaturePolarPattern.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureScaled.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureMultiTransform.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/DatumPlane.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/DatumLine.cpp`

判断：

- `FeatureExtrude.cpp` 是 Pad/Pocket 的共用主体，Pad/Pocket 文件应是薄入口。
- `FeatureTransformed.cpp` 是 Linear/Mirror/Polar/Scaled/MultiTransform 的共用主体，子类文件只负责变换列表和属性差异。
- Hole、DressUp、Datum 不应继续挂在泛化 `features/` 目录下，而应按 PartDesign 源文件边界独立。

### Sketcher 层

本地依据：

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/CMakeLists.txt`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectGeometry.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectConstraints.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectOperations.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectSF.cpp`

判断：

- SketchObject 控制流、Geometry parser、ExternalGeometry、Constraints、Operations 应逐步拆到 Sketcher 同名文件。
- FaceMaker / WireJoiner 仍属于 Part，Sketcher 只保留调用顺序和 SketchObject 业务语义。
- `SketchObjectSF.cpp` 是弃用 flat-file 对象，不是 internal face 构造归属地。

## 目标目录

建议长期目标为：

```text
cad-core/
  include/cad_core/
    app/
    base/
    part/
    part_design/
    sketcher/
    mesh/
    assembly/
    graph/
    runtime/
    adapters/
    compatibility/

  src/
    app/
      document.cpp
      document_object.cpp
      property.cpp
      property_links.cpp
      property_geo.cpp
      geo_feature.cpp
      link.cpp
      element_map.cpp
      mapped_name.cpp

    base/
      placement.cpp

    part/
      part_feature.cpp
      part_features.cpp
      body_base.cpp
      property_topo_shape.cpp
      topo_shape.cpp
      topo_shape_expansion.cpp
      topo_shape_mapper.cpp
      shape_exporter.cpp
      brep_snapshot.cpp
      face_maker.cpp
      face_maker_build_face.cpp
      face_maker_bullseye.cpp
      wire_joiner.cpp
      attach_extension.cpp
      attacher.cpp
      extrusion_helper.cpp
      refine_model.cpp
      shape_fix.cpp
      part_boolean.cpp
      part_import_export.cpp

    part_design/
      body.cpp
      feature.cpp
      feature_base.cpp
      feature_add_sub.cpp
      feature_sketch_based.cpp
      feature_extrude.cpp
      feature_pad.cpp
      feature_pocket.cpp
      feature_transformed.cpp
      feature_linear_pattern.cpp
      feature_mirrored.cpp
      feature_polar_pattern.cpp
      feature_scaled.cpp
      feature_multi_transform.cpp
      feature_hole.cpp
      feature_dress_up.cpp
      datum_plane.cpp
      datum_line.cpp
      datum_point.cpp
      datum_coordinate_system.cpp

    sketcher/
      sketch.cpp
      sketch_object.cpp
      sketch_object_geometry.cpp
      sketch_object_external.cpp
      sketch_object_constraints.cpp
      sketch_object_operations.cpp
      sketch_object_sf.cpp

    mesh/
      mesh_feature.cpp
      feature_mesh_import.cpp

    assembly/
      assembly_object.cpp
      assembly_link.cpp
      joint_group.cpp
      assembly_utils.cpp

    graph/
      recompute_plan.cpp

    runtime/
      compute_context.cpp
      diagnostics.cpp
      feature_registry.cpp
      io.cpp
      recompute.cpp

    adapters/
      cli/
      c_api/
```

`compatibility/` 只用于迁移期旧 include 路径兼容，不能成为新的语义落点。

## 当前文件迁移映射

| 当前文件 | 目标文件 | FreeCAD 对应 | 调整原则 |
| --- | --- | --- | --- |
| `src/document/model.cpp` | `src/app/document.cpp`、`document_object.cpp`、`property.cpp` | `src/App/Document.cpp`、`DocumentObject.cpp`、`Property.cpp` | 文档对象 graph 是 App 语义；JSON 解析只是输入适配。 |
| `src/features/link.cpp` | `src/app/link.cpp`，必要时拆 `src/assembly/assembly_link.cpp` | `src/App/Link.cpp`、`src/Mod/Assembly/App/AssemblyLink.cpp` | App Link 和 Assembly Link 分开，不留在 features。 |
| `src/features/part.cpp` | `src/part/part_feature.cpp`、`part_features.cpp` | `src/Mod/Part/App/PartFeature.cpp`、`PartFeatures.cpp` | Part primitive/Part::Feature 回到 Part 层。 |
| `src/features/part_boolean.cpp` | `src/part/part_boolean.cpp` | `src/Mod/Part/App/FeaturePartBoolean.cpp` 及 Part boolean 相关路径 | Part Workbench boolean 不属于 PartDesign。 |
| `src/features/body.cpp` | `src/part_design/body.cpp`，共用基础放 `src/part/body_base.cpp` | `PartDesign/App/Body.cpp`、`Part/App/BodyBase.cpp` | Body Tip、BaseFeature、Group、SubObject 按 FreeCAD 类边界拆。 |
| `src/features/feature_base.cpp` | `src/part_design/feature_base.cpp` | `PartDesign/App/FeatureBase.cpp` | 链首 base feature 独立。 |
| `src/features/feature_extrude.cpp` | `src/part_design/feature_extrude.cpp` | `PartDesign/App/FeatureExtrude.cpp` | 继续作为 Pad/Pocket 共用主体，但归入 PartDesign。 |
| `src/features/pad.cpp` | `src/part_design/feature_pad.cpp` | `PartDesign/App/FeaturePad.cpp` | Pad 只保留入口和 additive 语义。 |
| `src/features/pocket.cpp` | `src/part_design/feature_pocket.cpp` | `PartDesign/App/FeaturePocket.cpp` | Pocket 只保留入口和 subtractive 语义。 |
| `src/features/transformed.cpp` | `src/part_design/feature_transformed.cpp`，并拆子类文件 | `FeatureTransformed.cpp`、`FeatureLinearPattern.cpp`、`FeatureMirrored.cpp`、`FeaturePolarPattern.cpp`、`FeatureScaled.cpp`、`FeatureMultiTransform.cpp` | common runner 和子类 transformation 生成分开。 |
| `src/features/hole.cpp` | `src/part_design/feature_hole.cpp` | `PartDesign/App/FeatureHole.cpp` | Hole 参数、thread/counterbore/countersink 语义独立。 |
| `src/features/dress_up.cpp` | `src/part_design/feature_dress_up.cpp`，必要时拆 `feature_fillet.cpp`、`feature_chamfer.cpp` | `FeatureDressUp.cpp`、`FeatureFillet.cpp`、`FeatureChamfer.cpp` | DressUp 链和 edge selection 不留在泛化 features。 |
| `src/features/datum_*.cpp` | `src/part_design/datum_*.cpp` | `DatumPlane.cpp`、`DatumLine.cpp`、`DatumPoint.cpp`、`DatumCS.cpp` | Datum 是 PartDesign feature family。 |
| `src/features/sketch_object.cpp` | `src/sketcher/sketch_object.cpp`，再拆 geometry/external/constraints/operations | `SketchObject*.cpp`、`Sketch.cpp` | SketchObject 控制流归 Sketcher，Part maker 不放这里。 |
| `src/geometry/sketch_internal_builder.cpp` | 调度留 `src/sketcher/sketch_object.cpp`，maker 落 `src/part/face_maker*.cpp` | `SketchObject.cpp::buildInternals()`、`FaceMaker*.cpp` | 拆清 Sketcher 控制流和 Part maker 账本。 |
| `src/features/mesh.cpp` | `src/mesh/feature_mesh_import.cpp` | `src/Mod/Mesh/App/FeatureMeshImport.cpp`、`MeshFeature.cpp` | Mesh::Import 属于 Mesh 模块，不留在泛化 features。 |
| `src/geometry/face_maker.cpp` | `src/part/face_maker.cpp`、`face_maker_build_face.cpp`、`face_maker_bullseye.cpp` | `Part/App/FaceMaker*.cpp` | FaceMaker 是 Part 账本，不是普通 geometry helper。 |
| `src/geometry/wire_joiner.cpp` | `src/part/wire_joiner.cpp` | `Part/App/WireJoiner.cpp` | 保持单文件状态机，先不为小文件拆 EdgeInfo/WireInfo。 |
| `src/geometry/extrusion_helper.cpp` | `src/part/extrusion_helper.cpp` | `Part/App/ExtrusionHelper.cpp` | PartDesign 只触发，底层 shape maker 属于 Part。 |
| `src/geometry/refine_model.cpp` | `src/part/refine_model.cpp` | `Part/App/TopoShape.cpp` / refine 相关路径 | Refine 是 Part shape 后处理。 |
| `src/geometry/shape_fix.cpp` | `src/part/shape_fix.cpp` | `Part/App/TopoShape.cpp::fix()` 相关 | ShapeFix history 要进入 Part/TopoShape 语义链。 |
| `src/geometry/shape_exporter.cpp` | `src/part/shape_exporter.cpp` | `Part/App/TopoShape.cpp` export / tessellation 相关 | bbox、volume、mesh、BREP/STEP/STL export 是 Part TopoShape 输出边界。 |
| `src/geometry/brep_snapshot.cpp` | `src/part/brep_snapshot.cpp` | `Part/App/PartFeature.cpp::Feature::onBeforeChange()`、`TopoShape.cpp::exportBrep()` | `ReferenceShadow.brep` 旧 subshape 证据读取和写出归 Part/App topo reference 恢复链。 |
| `src/geometry/placement.cpp` | `src/base/placement.cpp`，属性解析放 `src/app/property_geo.cpp` | `src/Base/Placement.cpp`、`src/App/PropertyGeo.cpp` | 值类型和属性层分开。 |
| `src/topo/named_shape.cpp` | `src/part/topo_shape.cpp`、`topo_shape_expansion.cpp`、`topo_shape_mapper.cpp` | `TopoShape.cpp`、`TopoShapeExpansion.cpp`、`TopoShapeMapper.cpp` | 不继续把 Part TopoShape 语义藏在通用 topo。 |
| `src/topo/element_map.cpp` | `src/app/element_map.cpp`，Part 消费入口在 `part/topo_shape_expansion.cpp` | `src/App/ElementMap.cpp`、`src/Mod/Part/App/TopoShapeExpansion.cpp` | ElementMap 数据结构属 App，传播消费属 Part。 |
| `src/topo/mapper_history.cpp` | `src/part/topo_shape_mapper.cpp` 或 `src/part/topo_shape_expansion.cpp` | `TopoShapeMapper.cpp`、`TopoShapeExpansion.cpp` | history 是 Part topo naming 主线，不是 executor 输出修正。 |
| `src/topo/reference_matcher.cpp` | `src/app/property_links.cpp`、`src/part/topo_shape_reference.cpp` | `PropertyLinks.cpp`、`TopoShapeExpansion.cpp` | App 解析 ReferenceShadow 请求证据，Part 承接旧 subshape 几何恢复。 |
| `src/topo/subshape_map.cpp` | `src/part/property_topo_shape.cpp` 或 `src/part/topo_shape.cpp` | `PropertyTopoShape.cpp`、`TopoShape.cpp` | subshape map 是 Part shape 输出结构。 |
| `src/graph/recompute_plan.cpp` | 保留 `src/graph/recompute_plan.cpp` | FreeCAD Document recompute 计划的无状态裁剪 | graph 不放 feature 语义。 |
| `src/runtime/*` | 保留 `src/runtime/*` | cad-core 自身无状态执行壳 | runtime 只调度，不拥有 FreeCAD 类语义。 |
| `src/adapters/*` | 保留 `src/adapters/*` | CLI / C ABI adapter | adapter 只做协议转换和能力暴露。 |

## 需要修正的边界

### 1. `features/` 不再作为语义归属地

`FeatureExecutor` 可以继续作为 C++ 调度接口存在，但 executor 不是 FreeCAD 语义归属地。新增或迁移 feature 时，应先问它在 FreeCAD 属于哪个模块：

- `Part::Box`、`Part::Cylinder`、`Part::Cut`、`Part::Import` 属于 `part/`。
- `PartDesign::Pad`、`Pocket`、`Hole`、`Fillet`、`Chamfer` 属于 `part_design/`。
- `Sketcher::SketchObject` 属于 `sketcher/`。
- `App::Link` 属于 `app/`，Assembly Link 再进入 `assembly/`。

### 2. `geometry/` 只能留低层 OCCT helper

允许保留少量真正无 FreeCAD 业务语义的 OCCT 工具。但当 bbox、mesh exporter、BREP snapshot 编解码、STEP/IGES/STL 辅助导出已经绑定 FreeCAD `TopoShape` / `ReferenceShadow` 语义时，应归到 `part/` 正式路径，旧 `geometry/` 仅保留兼容转发。以下内容不应长期放在 `geometry/`：

- FaceMaker / FaceMakerBuildFace / FaceMakerBullseye。
- WireJoiner。
- ExtrusionHelper。
- RefineModel / ShapeFix 中依赖 FreeCAD history 的部分。
- Sketch internal face 的 FreeCAD 调用顺序。

### 3. `topo/` 不能成为输出修正层

当前 `topo` 目录承担了命名、ElementMap、MapperHistory、ReferenceMatcher、SubshapeMap 等能力。长期应拆成：

- App ElementMap / MappedName 数据结构。
- Part TopoShape / TopoShapeExpansion / TopoShapeMapper。
- Runtime 或 adapter 所需的只读导出 facade。

禁止把 topo naming 继续写成 executor 或 adapter 层的输出补丁。历史、split、deleted、generated、modified、merge 的语义必须沿 FreeCAD `TopoShapeExpansion` / `TopoShapeMapper` / `ElementMap` 路径传播。

### 4. `sketch_object.cpp` 要按 FreeCAD 文件拆，而不是按 fixture 拆

`src/features/sketch_object.cpp` 是当前最大文件之一。它需要拆，但拆分依据必须是 FreeCAD 文件：

- `sketch.cpp`：请求局部 Sketch 数据模型、solver-facing 基础状态。
- `sketch_object.cpp`：SketchObject execute/buildShape/buildInternals 控制流。
- `sketch_object_geometry.cpp`：GeometryList、construction、id repair、geometry extension。
- `sketch_object_external.cpp`：ExternalGeometry、ReferenceShadow、missing/frozen/sync link。
- `sketch_object_constraints.cpp`：constraint parser 和 solver-facing constraint 状态。
- `sketch_object_operations.cpp`：copy/modify/operation 风格行为。

不要按 `ellipse && bspline`、fixture 名称、source edge 猜测或 internal face 输出顺序拆分。

### 5. `wire_joiner.cpp` 先保持大状态机

`WireJoiner.cpp` 在 FreeCAD 中就是复杂状态机。当前 C++ 实现如果继续追 FreeCAD parity，应优先保持 EdgeInfo、WireInfo、iteration、superEdge、aHistory、result producer/blocker 这些账本在同一模块内可追踪。

可以拆私有 helper，但不应把 ownership/history 账本拆到互相看不到状态的多个抽象层里。

## 实施路线

### M0：新增 FreeCAD 同构 facade，不改行为

目标：

- 新增 `include/cad_core/app`、`part`、`part_design`、`sketcher`、`mesh`、`assembly`、`base` 目录。
- 新增对应 `src/app`、`src/part`、`src/part_design`、`src/sketcher`、`src/mesh`、`src/assembly`、`src/base`。
- 旧 `features`、`geometry`、`topo` header 先作为 compatibility include 或 type alias 保留。
- `CMakeLists.txt` 只增加新文件并保持 target 不变。

验收：

- 行为不变。
- `cmake --build build` 通过。
- `python3 -m unittest tests.test_mvp tests.test_feature_flows` 通过。

### M1：App / Base 基础设施归位

迁移：

- `document/model.*` 拆到 `app/document*`、`app/document_object*`、`app/property*`。
- `features/link.*` 拆到 `app/link.*`，Assembly 部分预留到 `assembly/`。
- `geometry/placement.*` 拆到 `base/placement.*` 和 `app/property_geo.*`。
- `topo/element_map.*` 的数据结构部分拆到 `app/element_map.*`。

边界：

- `graph/recompute_plan.*` 继续只读 App graph，不放对象业务规则。
- `runtime/compute_context.*` 只存请求期缓存，不暴露 App 行为方法。

### 当前落地基线（2026-06-05）

已完成的框架基线：

- `include/cad_core/app`、`base`、`part`、`part_design`、`sketcher`、`mesh`、`assembly`、`compatibility` 已建立；`src/app`、`src/base`、`src/part`、`src/part_design`、`src/sketcher`、`src/mesh`、`src/assembly` 已承接真实编译单元。
- `src/document/model.cpp` 已迁到 `src/app/document.cpp`，聚合入口保留在 `include/cad_core/app/document.h`；公开 App request model 已拆到 `include/cad_core/app/document_object.h`、`property.h`、`property_links.h`、`property_geo.h`。单个 `DocumentObject` 的 JSON 解析已迁到 `src/app/document_object.cpp`；`PropertyLinks` 的 link 类型分类、Hidden scope 判定、LinkSub / ReferenceShadow / external geometry flags JSON 解析、link payload 校验、ReferenceShadow 对象重命名和 label reference normalize 已迁到 `src/app/property_links.cpp`；对象级 Property 读取接口、typed payload 校验与 `parsePropertyValue` 已迁到 `src/app/property.cpp`；Placement 读取接口已迁到 `src/app/property_geo.cpp`；旧 `include/cad_core/document/model.h` 只保留 compatibility 转发。
- App request model 的公开类型、读取 API 和实现符号已正式切到 `cad_core::app::{Document,DocumentObject,Link,ReferenceShadow,Placement,parseDocument,read*}`；`cad_core::document` 仅保留旧命名兼容 alias。
- `src/features/link.cpp` 已迁到 `src/app/link.cpp`，公开声明迁到 `include/cad_core/app/link.h`，实现符号已切到 `cad_core::app`；旧 `include/cad_core/features/link.h` 只保留 compatibility 转发 / using。
- `src/geometry/placement.cpp` 已迁到 `src/base/placement.cpp`，公开声明和实现符号已切到 `cad_core::base`；旧 `cad_core::geometry::{placementFromComponents,transformShape}` 只保留 compatibility using。
- `src/topo/element_map.cpp` 已迁到 `src/app/element_map.cpp`，公开声明和实现符号已切到 `cad_core::app`；旧 `cad_core::topo::internalElementMapForSketch` 只保留 compatibility using。
- 新增 Part、PartDesign、Sketcher、Assembly facade header，用于让新调用侧先指向 FreeCAD 同构路径；现阶段未改变几何语义。
- M2 Part/Topo 主体归位已完成到文件和 namespace 边界：`src/features/part.cpp`、`src/features/part_boolean.cpp` 已迁到 `src/part/part_feature.cpp`、`src/part/part_boolean.cpp`，Part executor 实现符号已切到 `cad_core::part`；`src/geometry/extrusion_helper.cpp`、`face_maker.cpp`、`refine_model.cpp`、`shape_fix.cpp`、`wire_joiner.cpp`、`shape_exporter.cpp`、`brep_snapshot.cpp` 已迁到 `src/part/`，公开与实现符号已切到 `cad_core::part`；旧 `cad_core::geometry` 只保留 compatibility using。
- `src/topo/named_shape.cpp`、`mapper_history.cpp`、`import_element_map.cpp`、`subshape_map.cpp` 已迁到 `src/part/topo_shape*.cpp` 与 `property_topo_shape.cpp`，公开与实现符号已切到 `cad_core::part`；旧 `cad_core::topo` 只保留 compatibility using。
- M2 ReferenceShadow 旧 subshape 恢复能力已从 `src/topo/reference_matcher.cpp` 迁到 `src/part/topo_shape_reference.cpp`，公开与实现符号已切到 `cad_core::part`；旧 `include/cad_core/topo/reference_matcher.h` 仅保留 compatibility 转发。
- M3 PartDesign feature family 已归位到文件边界层：Body、FeatureBase、FeatureExtrude、Pad、Pocket、Transformed、Hole、DressUp、Datum 相关源文件已迁到 `src/part_design/`，实现符号已切到 `cad_core::part_design`，旧 `features/*` header 只保留 compatibility 转发 / using。
- M4 SketchObject 主体已归位到 `src/sketcher/sketch_object.cpp`；ExternalGeometry flags 读取 / 序列化、ReferenceShadow JSON / BREP snapshot 处理、stable subname 诊断、ShadowSub / InternalEdge / InternalVertex 恢复、whole-shape 外部引用展开、external reference key helper、`resolveExternalGeometryLink`、native `ExternalGeo` 读取 / detach 更新、OCCT 投影 / intersection 和 `rebuildExternalGeometry` 主循环已迁到独立编译单元 `src/sketcher/sketch_object_external.cpp` / `.h`；Sketch 基础几何数据结构、Geometry parser、profile construction 过滤和角度采样 helper 已迁到 `src/sketcher/sketch_object_geometry.cpp` / `.h`；约束数据模型、约束 Type / Datum 读取、malformed / redundant / conflict 诊断分析、constraint parser / apply 主循环、solver 状态命名、constraint index JSON 和 solver failure 输出已迁到 `src/sketcher/sketch_object_constraints.cpp` / `.h`；profile edge、raw Sketch Shape、profile face、InternalShape 构建 helper 已迁到 `src/sketcher/sketch_object_operations.cpp` / `.h`；`SketchObject::buildInternals()` 的 FaceMaker / WireJoiner 调用顺序已从 `geometry/sketch_internal_builder.cpp` 迁到 `src/sketcher/sketch_internal_builder.cpp`，实现符号已切到 `cad_core::sketcher`，旧 geometry header / namespace 只保留 compatibility 转发 / using。FreeCAD `SketchObjectOperations.cpp` 的编辑操作语义后续继续拆分。
- M5 Assembly / Link 产品化边界已完成到执行入口层：`AssemblyObject`、`AssemblyLink`、`JointGroup` 和 Assembly `App::FeaturePython` joint 执行入口已迁到 `src/assembly/assembly_object.cpp`、`assembly_link.cpp`、`joint_group.cpp`，共享求解诊断和发布 helper 落到 `src/assembly/assembly_utils.cpp`，实现符号已切到 `cad_core::assembly`；`App::Link` 通用展开语义继续留在 `src/app/link.cpp`，`AssemblyLink` 仅复用 App LinkBaseExtension 风格的共享执行入口。
- Mesh `FeatureMeshImport` 当前 STL/AST import 入口已从 `src/features/mesh.cpp` 迁到 `src/mesh/feature_mesh_import.cpp`，公开声明迁到 `include/cad_core/mesh/feature_mesh_import.h`，实现符号已切到 `cad_core::mesh`；旧 `include/cad_core/features/mesh.h` 只保留 compatibility 转发 / using。
- App、Part、PartDesign、Sketcher、Mesh、Assembly 的执行入口 header 已建立 module namespace 正式入口，旧 `cad_core::features::*` 执行入口仅作为 compatibility using；`runtime/feature_registry.cpp` 注册表已改为通过 `app::`、`part::`、`part_design::`、`sketcher::`、`mesh::`、`assembly::` 引用业务执行入口。
- `FeatureExecutor` 调度函数签名、unsupported-property 诊断和 Refine helper 的正式符号已迁到 `cad_core::runtime`；旧 `cad_core::features::{ExecuteFn,rejectUnsupportedProperties,applyRefineProperty*}` 只保留 compatibility using。
- M6 已完成内部 include 与 namespace 调用收口：内部调用不再 include `cad_core/document/model.h`、`cad_core/geometry/placement.h`、`cad_core/features/*`、`cad_core/geometry/sketch_internal_builder.h`、旧 `cad_core/geometry/{face_maker,wire_joiner,extrusion_helper,refine_model,shape_fix,shape_exporter,brep_snapshot}.h` 或旧 `cad_core/topo/*` 语义路径；`src/features/`、`src/topo/` 与 `src/geometry/` 不再承接真实编译单元，旧 compatibility header 仍保留给迁移期外部和旧路径调用。

剩余缺口：

- `app/document.cpp` 当前只保留 parseDocument 主流程、target 解析、父级 group 关系补充和 App::Link materialized element 依赖补充；后续若继续细分，应保持在 App Document / DocumentObject / PropertyLinks / PropertyGeo 边界内。
- Part 侧旧 subshape 恢复已经归位到 `part/topo_shape_reference.cpp`；App `ReferenceShadow` / LinkSub 请求证据读取、对象重命名和 label reference normalize 已迁到 `app/property_links.cpp`，后续若继续细分应保持在 `PropertyLinks.cpp` 语义边界内。
- M4 当前 `sketch_object.cpp` 只保留 SketchObject 执行控制流、support / SketchPlaneFrame frame 读取、placement 合成、外部几何 / 约束 / profile build 调用和输出发布；`sketch_object_geometry.cpp` 已承接 Geometry parser、profile 过滤和共享角度采样 helper，`sketch_object_external.cpp` 已承接外部引用 flags / ReferenceShadow / stable subname / ShadowSub 恢复、link resolve、native `ExternalGeo`、OCCT 投影 / intersection 和 rebuild 主循环，`sketch_object_constraints.cpp` 已承接 Constraints parser / solver-facing 状态和 apply 主循环，`sketch_object_operations.cpp` 已承接 profile edge、raw Shape、profile face 和 InternalShape 构建 helper。剩余缺口是 FreeCAD `SketchObjectOperations.cpp` 中更偏编辑操作的 add/del/restore 语义尚未单独落地。
- M6 仍未删除旧 compatibility facade；删除条件保持为外部调用侧都迁到同构路径，并确认 capability / adapter 兼容面不再依赖旧 include。

当前本轮验收命令：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_mvp tests.test_feature_flows
python3 -m unittest tests.test_p5_sketch tests.test_p6_topology
python3 -m unittest tests.test_p7_features tests.test_p8_features tests.test_adapters
cd /Users/li/Chili3DProject/重构Chili/FreeCAD
git diff --check
```

### M2：Part TopoShape / ElementMap / FaceMaker / WireJoiner 归位

迁移：

- `topo/named_shape.*` 按 FreeCAD 拆为 `part/topo_shape.*`、`part/topo_shape_expansion.*`、`part/topo_shape_mapper.*`。
- `topo/mapper_history.*` 消费路径并入 `part/topo_shape_mapper.*` 或 `part/topo_shape_expansion.*`。
- `geometry/face_maker.*` 拆到 `part/face_maker*`。
- `geometry/wire_joiner.*` 整体迁到 `part/wire_joiner.*`。
- `geometry/extrusion_helper.*`、`refine_model.*`、`shape_fix.*` 迁到 Part。

边界：

- `part/topo_shape.cpp` 保持基础 shape 容器。
- `part/topo_shape_expansion.cpp` 承接 `makeElement*`、copy/reTag ElementMap、modified/generated/deleted/split propagation。
- `part/topo_shape_mapper.cpp` 承接 mapper/history 关系和可恢复性。
- FaceMaker / WireJoiner 不为了小文件化拆散账本。

### M3：PartDesign feature family 归位

迁移：

- `features/body.*` -> `part_design/body.*`，共用 BodyBase 到 `part/body_base.*`。
- `features/feature_base.*` -> `part_design/feature_base.*`。
- `features/feature_extrude.*` -> `part_design/feature_extrude.*`。
- `features/pad.*` -> `part_design/feature_pad.*`。
- `features/pocket.*` -> `part_design/feature_pocket.*`。
- `features/transformed.*` -> `part_design/feature_transformed.*`，并拆 Linear/Mirror/Polar/Scaled/MultiTransform 子文件。
- `features/hole.*` -> `part_design/feature_hole.*`。
- `features/dress_up.*` -> `part_design/feature_dress_up.*`，后续按 Fillet/Chamfer 继续拆。
- `features/datum_*.*` -> `part_design/datum_*.*`。

边界：

- PartDesign 文件只决定 feature 链、support/profile、add/sub、transform 列表、diagnostics。
- 底层 shape maker、ElementMap、FaceMaker、WireJoiner 仍调用 Part 层。

### M4：Sketcher 拆分

迁移：

- `features/sketch_object.*` 移到 `sketcher/sketch_object.*`。
- Geometry parser 下沉到 `sketcher/sketch_object_geometry.*`。
- ExternalGeometry / ReferenceShadow 下沉到 `sketcher/sketch_object_external.*`。
- Constraint parser 和 solver-facing 状态下沉到 `sketcher/sketch_object_constraints.*`。
- Sketch data model 下沉到 `sketcher/sketch.*`。
- Sketch internal face 构造调度留在 SketchObject，实际 FaceMaker/WireJoiner 调用回 Part。

边界：

- Sketcher 不合成 TopoShape history。
- Sketcher 不按 fixture 输出猜 internal names。
- `InternalFaceN` / `InternalEdgeN` / `InternalVertexN` 的当前名规则应贴近 SketchObject，但 history 传播归 Part/Topo。

### M5：Assembly / Link 产品化边界

迁移：

- `app/link.*` 保留 App Link 通用展开语义。
- `assembly/assembly_object.*` 承接 AssemblyObject。
- `assembly/assembly_link.*` 承接 AssemblyLink。
- `assembly/joint_group.*` 承接 JointGroup 和 solver diagnostics。

边界：

- 没有完整 solver 前，Assembly 可以先返回结构化 diagnostics。
- Link / Assembly 不应把 Part 或 PartDesign shape history 复制成另一套规则。

### M6：删除旧 facade

条件：

- 所有调用侧不再直接 include `features/*`、`geometry/face_maker*`、`geometry/wire_joiner*`、`topo/named_shape*` 这些旧语义路径。
- `CMakeLists.txt` 只保留 FreeCAD 同构目标文件。
- Python fixture test 和 C ABI capability test 全部绿色。

完成后：

- 删除 compatibility include。
- 更新 `docs/CADCore3.0/FreeCAD语义矩阵.md` 和 capability 对照表。

## 验收标准

### 结构验收

- 文件名能直接映射本地 FreeCAD 文件。例如：
  - `part_design/feature_extrude.cpp` -> `src/Mod/PartDesign/App/FeatureExtrude.cpp`
  - `part/topo_shape_expansion.cpp` -> `src/Mod/Part/App/TopoShapeExpansion.cpp`
  - `sketcher/sketch_object_external.cpp` -> `src/Mod/Sketcher/App/SketchObjectExternal.cpp`
  - `app/property_links.cpp` -> `src/App/PropertyLinks.cpp`
- `runtime`、`graph`、`adapters` 中不新增 FreeCAD feature 业务规则。
- `FeatureExecutor` 只作为调度接口存在，不再决定语义归属。
- 旧目录只作为短期兼容 facade，不能继续承接新功能。

### 行为验收

普通迁移每轮只验证本次 touched family：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_mvp tests.test_feature_flows
git diff --check
```

涉及 Sketcher / internal face：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p5_sketch tests.test_p6_topology
git diff --check
```

涉及 PartDesign / Link / Assembly：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_p8_features tests.test_adapters
git diff --check
```

阶段收口再跑：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
python3 -m unittest tests.test_adapters tests.test_diagnostics tests.test_expected_fixtures tests.test_feature_flows tests.test_mvp tests.test_p5_sketch tests.test_p6_topology tests.test_p7_features tests.test_p8_features
```

### 文档验收

- 每个迁移阶段只更新当前基线、剩余缺口和验收命令，不记录流水账。
- 涉及实质 FreeCAD 语义迁移时，相邻代码注释要标明 FreeCAD 源文件、类/函数和关键短句。
- 如只做文件边界迁移且行为不变，文档应明确“未改变几何语义”。

## 非目标

- 不引入 Qt、Gui、Workbench、TaskPanel 或 Web 会话状态。
- 不把 BREP 作为请求/响应长期状态，仍遵守 `ReferenceShadow.brep` 的唯一例外。
- 不为了减少单文件行数拆散 WireJoiner、FaceMaker、TopoShapeExpansion、MapperHistory 这类内部账本。
- 不在 adapter、response exporter 或 executor 输出端新增 fixture 特判来替代 topo/history 传播。
- 不因为目录重命名同步重写全部行为；框架迁移和行为 parity 应分开提交、分开验收。

## 近期建议

下一轮如果开始落地，优先选 M0 + M1：

1. 先新增 `app/`、`base/`、`part/`、`part_design/`、`sketcher/` facade 目录。
2. 只搬 `placement`、`link`、`document model` 这类低风险边界。
3. 保留旧 include 路径，避免一次性影响所有 fixture。
4. 跑 `cmake --build build`、`python3 -m unittest tests.test_mvp tests.test_feature_flows`、`git diff --check`。

不要第一轮就搬 `wire_joiner.cpp` 或 `sketch_object.cpp`。这两个文件需要按 FreeCAD 调用链和测试族单独做迁移方案。
