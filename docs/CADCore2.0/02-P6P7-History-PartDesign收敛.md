# P6/P7：History 与 PartDesign 生态收敛

本主线在 P5/P6 ExternalGeometry / TopoNaming 主路径稳定后推进。目标是把当前分散在 ShapeFix、RefineModel、taper、transformed、DressUp、PartDesign pattern 中的 partial history 收敛到统一 MapperHistory 生命周期。

## 目标边界

- ShapeFix、RefineModel、taper、transformed、DressUp 都通过同一 MapperHistory / ElementMap 入口传播 stable subname。
- PartDesign transformed / pattern 的 source ownership 不靠 result shape 几何猜测。
- DressUp / transformed / Body boolean / Link retag 后的 terminal split / deleted / merge history 可持续传播。
- 复杂路径不能静默 fallback；必须输出 `known_gap` 或结构化 diagnostics。

## FreeCAD 依据

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp`
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
- taper `BRepOffsetAPI_ThruSections` / section source history 从 `known_gap:taper_history` 收敛到正式 history 或更窄的 explicit gap。
- ShapeFix history 补齐，至少覆盖当前会改变 edge / wire / face 身份的主路径。
- Fillet / Chamfer / DressUp cache 的 AddSubShape slot 级 `NamedShape` 继续传播 source alias、terminal split / deleted、merge history。

完成判定：

- Refine / taper / DressUp fixture 不再各自维护互不兼容的 history 字段。
- `element_history_status` 对 generated / modified / deleted / split / merge 的解释一致。
- 复杂 maker history 未迁移时有稳定 diagnostics，不吞掉旧引用。

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

## C2-M6b：Hole 与 PartDesign 参数余量

交付内容：

- Hole ModelThread 已有 metric pipe-shell 子集，继续补 FreeCAD 表驱动和复杂 thread / head-cut 余量。
- Fillet / Chamfer 复杂参数组合和引用变化后恢复纳入 P6 history 主路径。
- 非当前主线的复杂 feature 只补 diagnostics，不先扩大窄路径。

完成判定：

- Hole / Fillet / Chamfer 新增 fixture 均有 FreeCAD native oracle 或明确 geometry-equivalent 边界。
- 参数解析失败、资源表缺失、unsupported profile 均有稳定 diagnostics。
