# P7：PartDesign 常用生态

P7 在 P3-P6 底座上扩展常用 Body feature。原则是所有 feature 都继续消费 document/link/topo 主路径，不引入孤立几何能力。

## 当前基线

- Datum / Origin：`PartDesign::Plane`、`Line`、`Point`、`CoordinateSystem`、`App::Origin` 已接入基础 executor。
- Refine：`Refine=false` 按 FreeCAD no-op 执行；Pad standalone、Body AddSub final-result（Pad / Pocket / Hole）和 Fillet / Chamfer / Transformed family replacement refine 已接入 FreeCAD `BRepBuilderAPI_RefineModel` / `FaceUniter` / `MyRefineMaker::populate()` / `GenericShapeMapper::init()` history 路径。失败时按 FreeCAD Warn 策略保留原 shape 并给 warning diagnostics。
- Hole：支持 Sketch circle / arc center 与 Sketch point 的平底圆柱盲孔和通孔；支持 Tapered、Counterbore、Countersink、Counterdrill、Angled drill point；非建模 Threaded 已覆盖 ISO metric / ISO metric fine / UNC / UNF / UNEF / NPT / BSP / BSW / BSF / ISOTyre，其中 TapDrill 优先走 FreeCAD 表，TapDrill 缺失项按 FreeCAD `determineDiameter()` 走 NPT、BSP/BSW/BSF 或 `diameter - pitch` fallback 公式；Clearance 已覆盖 ISO metric 表、UTS 表和非 ISO fallback；Metric / MetricFine threaded head-cut 已按 FreeCAD `readCutDefinitions()` 接入 `Resources/Hole/*.json` 表加载，并支持 `CAD_CORE_HOLE_RESOURCE_DIR` 追加自定义表；`ThreadDepthType=Hole Depth / Dimension / Tapped (DIN76)` 已按 FreeCAD `updateThreadDepthParam()` 归一化；ModelThread 已按 `Hole::makeThread()` 接入 ISO/UTS、Whitworth 和 ISOTyre thread profile、left-handed、class/custom clearance、thread depth helix length 与 pipe-shell solid tool；结果走 subtractive `AddSubShape` 后由 Body cut。
- Fillet / Chamfer：支持基础 Edge / Face Base、连续边过滤和 OCCT fillet/chamfer maker，产出 replacement solid；同时按 `DressUp::getAddSubShape()` 生成基础 AddSubShape cache，cache 已携带 slot 级 `NamedShape`；`SupportTransform=true` 可被 transformed family 的 Features 模式按 add/sub slot ownership 消费；连续 DressUp 链会跳过中间 DressUp 并回到前一个 FeatureAddSub support。
- Mirrored：支持 `TransformMode=Features` 和 `Whole shape`，MirrorPlane 可为 DatumPlane 或 solid planar Face。
- LinearPattern：支持 `TransformMode=Features` 和 `Whole shape`，DatumLine / DatumPlane / solid Edge / Face / Sketch axis 方向，Extent、Spacing、Spacings、SpacingPattern 和双方向；Sketch axis 覆盖 `H_Axis` / `V_Axis` / `N_Axis` 和 construction `AxisN`；Features 模式已按 `Body::setBaseProperty()` 恢复前序 Body support，并可逐个 replay subtractive-only original 与 multi-original AddSubShape add / sub slot；Whole shape 模式按实际 BaseFeature / Body 前缀 support 执行和报告 source，不依赖隐藏的 `Originals`；前缀 support 会消费前序 AddSub feature 的 final-result `Refine=true` Shape。
- PolarPattern：支持 `TransformMode=Features` 和 `Whole shape`，DatumLine / shape Edge / Sketch axis 轴，Extent、Spacing、SpacingPattern；Sketch axis 覆盖 `H_Axis` / `V_Axis` / `N_Axis` 和 construction `AxisN`。
- Scaled：支持 `TransformMode=Features` 和 `Whole shape`；Features 模式以第一个 original AddSubShape 体积质心为缩放中心，Whole shape 按 FreeCAD 空 originals 行为使用原点作为缩放中心。
- MultiTransform：支持 Features 和 Whole shape 模式下 Mirrored / LinearPattern / PolarPattern / Scaled 子特征模板，非 Scaled 乘法组合，Scaled diagonal 组合；transformed copy source alias 和 split / deleted terminal history 已由 `topo::namedShapeForTransformedCopy()` 承接。

## 已知缺口

- Fillet / Chamfer 复杂参数组合、复杂引用变更后的完整稳定恢复。
- transformed family 的更复杂 pattern ownership 和完整 MapperHistory。

## cad-core 落点

| 文件 | 职责 |
| --- | --- |
| `features/datum_coordinate_system.*` | CoordinateSystem / Origin |
| `features/feature_executor.*` | PartDesign `Refine` 通用后处理入口，支持显式输出 owner |
| `features/hole.*` | Hole subtractive executor、基础 tapered / head-cut / drill-point 轮廓、thread diameter / tap-drill 表、NPT / Whitworth fallback 公式、clearance 表 / fallback、HoleCutType 资源表加载、ThreadDepth / DIN76、ModelThread helix / pipe-shell 实体螺纹工具 |
| `features/dress_up.*` | Fillet / Chamfer、DressUp AddSubShape cache、slot 级 NamedShape 和链式 `SupportTransform` source 解析 |
| `features/transformed.*` | Mirrored / Pattern / Scaled / MultiTransform，按 BaseFeature / Body 前缀恢复 support，并消费 AddSubShape slot 级 NamedShape |
| `features/body.*` | replacement solid、Body Tip、AddSub final-result refine |
| `topo/named_shape.*` | maker history、transformed source alias 和 terminal history 传播 |

## FreeCAD 依据

- `src/Mod/PartDesign/App/FeatureHole.cpp`
- `src/Mod/PartDesign/Resources/Hole/*.json`
- `src/Mod/PartDesign/App/FeatureDressUp.cpp`
- `src/Mod/PartDesign/App/FeatureFillet.cpp`
- `src/Mod/PartDesign/App/FeatureChamfer.cpp`
- `src/Mod/PartDesign/App/FeatureTransformed.cpp`
- `src/Mod/PartDesign/App/FeatureMirrored.cpp`
- `src/Mod/PartDesign/App/FeatureLinearPattern.cpp`
- `src/Mod/PartDesign/App/FeaturePolarPattern.cpp`
- `src/Mod/PartDesign/App/FeatureScaled.cpp`
- `src/Mod/PartDesign/App/FeatureMultiTransform.cpp`
- `src/Mod/PartDesign/App/FeatureRefine.cpp`
- `src/Mod/Part/App/modelRefine.cpp`

## 验收

- `fixtures/p7` 覆盖 Datum / Origin、RefineModel + GenericShapeMapper history、Hole final-result refine、Hole 基础孔、Hole point profile、Hole tapered、Hole head-cut / drill-point、Hole cosmetic threaded、Hole fine threaded、Hole UNC / UNF / UNEF / NPT / BSP / BSW / BSF / ISOTyre threaded、Hole ISO metric / UTS / generic fallback thread clearance、Hole bundled 与外部追加 HoleCutType 资源表、Hole ThreadDepth / DIN76 参数归一化、Hole ModelThread metric pipe-shell 几何、Fillet、Chamfer、DressUp SupportTransform 与链式 SupportTransform、Mirrored、LinearPattern Whole shape Body support / refined prefix support、subtractive-only original 与 multi-original Add/Sub replay、PolarPattern、Scaled、MultiTransform 和 transformed stable history diagnostics。
- 剩余复杂路径必须明确 diagnostics；未接入 RefineModel 语义的特征族仍不能静默执行。
- transformed copy 通过 topo helper 继续传播 original stable key、AddSubShape slot history 和 split / deleted terminal history，再进入 Body boolean history 或 stable subname diagnostics。
