# P6/P7：History 与 PartDesign 生态收敛

本主线在 P5/P6 ExternalGeometry / TopoNaming 主路径稳定后推进。目标是把当前分散在 ShapeFix、RefineModel、taper、transformed、DressUp、PartDesign pattern 中的 partial history 收敛到统一 MapperHistory 生命周期。

## 目标边界

- ShapeFix、RefineModel、taper、transformed、DressUp 都通过同一 MapperHistory / ElementMap 入口传播 stable subname。
- PartDesign transformed / pattern 的 source ownership 不靠 result shape 几何猜测。
- DressUp / transformed / Body boolean / Link retag 后的 terminal split / deleted / merge history 可持续传播。
- 复杂路径不能静默 fallback；必须输出 `known_gap` 或结构化 diagnostics。

当前前置基线：C2-M1 / C2-M2 / C2-M3 / C2-M4 已提供统一 MapperHistory event、Sketch InternalShape producer evidence 主路径、summary-only diagnostic 隔离和 ExternalGeometry resolver / 写回建议；C2-M5 已先把 taper ThruSections、RefineModel 当前 P7 覆盖子路径与 DressUp SupportTransform AddSubShape slot 接入同一账本；C2-M6 已把 transformed / pattern Add/Sub ownership 子路径接入同一账本。本主线剩余工作仍是把 ShapeFix 主路径、RefineModel 复杂 identity-change、transformed full history / DressUp 复杂余量的 producer history 继续收敛。

## FreeCAD 依据

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/ExtrusionHelper.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/modelRefine.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureRefine.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureFillet.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureChamfer.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureMirrored.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureLinearPattern.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeaturePolarPattern.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureScaled.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureMultiTransform.cpp`

## C2-M5：ShapeFix / Refine / taper / DressUp history 收敛

交付内容：

- RefineModel `Modified()` / `IsDeleted()` / generated face history 全部转换成 MapperHistory event。
- taper `BRepOffsetAPI_ThruSections` / section source history 从 `known_gap:taper_history` 收敛到正式 history；当前覆盖 Pad / Pocket / Part::Extrusion 的 Length、Two sides、Symmetric 和 inner-wire taper 子集。
- ShapeFix history 补齐，至少覆盖当前会改变 edge / wire / face 身份的主路径。
- Fillet / Chamfer / DressUp cache 的 AddSubShape slot 级 `NamedShape` 继续传播 source alias、terminal split / deleted、merge history；当前 SupportTransform 子集先落地。

完成判定：

- Refine / taper / DressUp fixture 不再各自维护互不兼容的 history 字段。
- `element_history_status` 对 generated / modified / deleted / split / merge 的解释一致。
- 复杂 maker history 未迁移时有稳定 diagnostics，不吞掉旧引用。

当前状态：

- taper ThruSections 已按 FreeCAD `ExtrusionHelper::makeElementDraft()` / `TopoShape::makeElementShape(mkGenerator, list_of_sections)` 对齐：`geometry/extrusion_helper.cpp` 保留 `BRepOffsetAPI_ThruSections` maker 和 section sources，`topo/named_shape.cpp` 通过 `MapperThruSections` 等价逻辑消费 `GeneratedFace()`、`FirstShape()` 和 `LastShape()`，生成统一 `mapper_history`。
- Pad / Pocket / Part::Extrusion 当前 taper fixture 不再发布 `known_gap:taper_history`；`cad_core_capabilities_json()` 的 maker history 暴露 `taper_thru_sections`，`remaining_gaps` 不再包含 `taper_full_history`。
- DressUp SupportTransform AddSubShape slot 已按 FreeCAD `DressUp::getAddSubShape()` 对齐：`features/dress_up.cpp` 对 Fillet / Chamfer 结果与 support base 分别构造 add/sub slot，`transformed.cpp` 消费 slot 时保留 `Fillet` / `Chamfer` / `Pad` source-prefixed alias、maker history、terminal split / deleted 和 merge；capabilities 暴露 `dressup_addsubshape_slot`。
- RefineModel 当前 P7 fixture 已按 FreeCAD `TopoShape::makeElementRefine()` / `MyRefineMaker::populate()` / `GenericShapeMapper::init()` 对齐：Pad、Pocket、Hole、Fillet / Chamfer 和 Mirrored refined support 消费 generated / modified / terminal deleted / merge history，`tests.test_p7_features` 锁定对应 `mapper_history` event。

剩余缺口：

- ShapeFix 主路径 history 和 RefineModel 更复杂 identity-change case 仍需继续收敛；ShapeFix 主路径当前按 FreeCAD `TopoShape::fix()` / `MapperHistory(ShapeFix_Root&)` 保留为显式 `topo_history.remaining_gaps.shapefix_history`，不由笼统 `complete_mapper_history` 单独承载。
- DressUp 非 SupportTransform 复杂参数组合和后续 transformed / pattern ownership 仍需继续扩大覆盖。

## C2-M6：PartDesign transformed / pattern 复杂 ownership

交付内容：

- Mirrored / LinearPattern / PolarPattern / Scaled / MultiTransform 的 Features / Whole shape 模式都消费统一 AddSubShape slot history。
- chain support、SupportTransform、refined prefix support、multi-original Add/Sub replay 的 source ownership 明确。
- transformed copy 不从 result 几何倒推 source，而按 FreeCAD `copyElementMap(tmp, op)` 等价语义保留 source-prefixed alias。
- pattern 后进入 Body boolean、DressUp、Link retag 时继续传播 terminal history。

完成判定：

- pattern / transformed fixture 覆盖 additive、subtractive、multi-original、refined support、chain DressUp、Link retag 后引用传播。
- topology count 未冻结的 native expected 要么收敛为 hard oracle，要么保留明确 pending gap。
- 不按 instance index、bbox、面积或输出顺序猜 source ownership。

当前状态：

- transformed / pattern Add/Sub ownership 子路径已按 FreeCAD `Transformed::execute()` 与 `TopoShape::makeElementTransform()` 对齐：Features 模式消费 `getAddSubShape(fuseShape, cutShape)`，WholeShape 变换 support；cad-core 通过 `namedShapeForTransformedCopy()` 保留 `copyElementMap(tmp, op)` 等价 alias，并在 LinearPattern、PolarPattern、Scaled、MultiTransform 代表 fixture 中传播 source object history、multi-original Add/Sub replay、refined prefix support 和 terminal split / deleted / merge。
- capabilities 暴露 `transformed_pattern_addsub_ownership`；`tests.test_p7_features` 锁定 transformed copy alias、source object history 和 terminal history。

剩余缺口：

- `transformed_pattern_full_history` 仍保留为完整 transformed / pattern ownership 缺口，后续继续覆盖超出当前 Add/Sub ownership 子路径的 pattern / Link retag 组合。

## C2-M6b：Hole 与 PartDesign 参数余量

交付内容：

- Hole ModelThread 已有 metric pipe-shell 子集，继续补 FreeCAD 表驱动和复杂 thread / head-cut 余量。
- Fillet / Chamfer 复杂参数组合和引用变化后恢复纳入 P6 history 主路径。
- 非当前主线的复杂 feature 只补 diagnostics，不先扩大窄路径。

完成判定：

- Hole / Fillet / Chamfer 新增 fixture 均有 FreeCAD native oracle 或明确 geometry-equivalent 边界。
- 参数解析失败、资源表缺失、unsupported profile 均有稳定 diagnostics。
