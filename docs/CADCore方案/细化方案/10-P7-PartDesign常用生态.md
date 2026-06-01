# P7：PartDesign 常用生态

P7 在 P3-P6 底座上扩展常用 Body feature。原则是所有 feature 都继续消费 document/link/topo 主路径，不引入孤立几何能力。

## 当前基线

- Datum / Origin：`PartDesign::Plane`、`Line`、`Point`、`CoordinateSystem`、`App::Origin` 已接入基础 executor。
- Refine：`Refine=false` 按 FreeCAD no-op 执行；Pad standalone、Body AddSub final-result（Pad / Pocket / Hole）和 Fillet / Chamfer / Transformed family replacement refine 已接入 FreeCAD `BRepBuilderAPI_RefineModel` / `FaceUniter` / `MyRefineMaker::populate()` / `GenericShapeMapper::init()` history 路径。失败时按 FreeCAD Warn 策略保留原 shape 并给 warning diagnostics。
- Hole：支持 Sketch circle / arc center 与 Sketch point 的平底圆柱盲孔和通孔；支持 Tapered、Counterbore、Countersink、Counterdrill、Angled drill point；非建模 Threaded 已覆盖 ISO metric / ISO metric fine / UNC / UNF / UNEF / NPT / BSP / BSW / BSF / ISOTyre，其中 TapDrill 优先走 FreeCAD 表，TapDrill 缺失项按 FreeCAD `determineDiameter()` 走 NPT、BSP/BSW/BSF 或 `diameter - pitch` fallback 公式；Clearance 已覆盖 ISO metric 表、UTS 表和非 ISO fallback；Metric / MetricFine threaded head-cut 已按 FreeCAD `readCutDefinitions()` 接入 `Resources/Hole/*.json` 表加载，并支持 `CAD_CORE_HOLE_RESOURCE_DIR` 追加自定义表；`ThreadDepthType=Hole Depth / Dimension / Tapped (DIN76)` 已按 FreeCAD `updateThreadDepthParam()` 归一化；ModelThread 已按 `Hole::makeThread()` 接入 ISO/UTS、Whitworth 和 ISOTyre thread profile、left-handed、class/custom clearance、thread depth helix length 与 pipe-shell solid tool；结果走 subtractive `AddSubShape` 后由 Body cut。
- FreeCAD expected collector 已补 Hole / Body membership / Sketch support 的还原生命周期，并以 support-backed fixture 固化 Hole native oracle：覆盖 blind depth、through-all、point profile、tapered、angled drill point、Counterbore / Countersink / Counterdrill、threaded standard counterbore / countersink、threaded dynamic DIN7984 / ISO2009、ThreadDepth DIN76 / Dimension clamp、thread clearance 和 ModelThread metric pipe-shell。Body `Group` 走 `addObject()`，Sketch `AttachmentSupport` / `MapMode` 延后到目标 shape 存在后写入，Hole 的 depth / thread / head-cut 属性延后到 `BaseFeature` 和 base shape 可用后写入。当前 native Hole oracle 只接受带 `AttachmentSupport` / `Support` 的 Profile sketch；既有 placement-only Hole fixture 仍按 geometry-equivalent CAD Core 验收，不伪装成 FreeCAD native golden。
- Fillet / Chamfer：支持基础 Edge / Face Base、连续边过滤和 OCCT fillet/chamfer maker，产出 replacement solid；同时按 `DressUp::getAddSubShape()` 生成基础 AddSubShape cache，cache 已携带 slot 级 `NamedShape`；`SupportTransform=true` 可被 transformed family 的 Features 模式按 add/sub slot ownership 消费；连续 DressUp 链会跳过中间 DressUp 并回到前一个 FeatureAddSub support。FreeCAD expected collector 已接入 Body-member Fillet / Chamfer native oracle，采集 DressUp feature shape 与最终 Body shape；诊断型 standalone DressUp fixture 不冻结为 native geometry oracle。
- Mirrored：支持 `TransformMode=Features` 和 `Whole shape`，MirrorPlane 可为 DatumPlane 或 solid planar Face；FreeCAD expected collector 已接入 `PartDesign::Plane` + `PartDesign::Mirrored` 的基础 native oracle，覆盖 Features / Whole shape、Refine=true、Fillet `SupportTransform` 和链式 Fillet / Chamfer `SupportTransform` 镜像路径。当前 native oracle 只把已对齐的 bbox / volume 冻结为硬验收；非 refine / SupportTransform 镜像及链式 DressUp 镜像的拓扑计数仍归入 transformed maker-history 收敛范围。
- LinearPattern：支持 `TransformMode=Features` 和 `Whole shape`，DatumLine / DatumPlane / solid Edge / Face / Sketch axis 方向，Extent、Spacing、Spacings、SpacingPattern 和双方向；Sketch axis 覆盖 `H_Axis` / `V_Axis` / `N_Axis` 和 construction `AxisN`；Features 模式已按 `Body::setBaseProperty()` 恢复前序 Body support，并可逐个 replay subtractive-only original 与 multi-original AddSubShape add / sub slot；Whole shape 模式按实际 BaseFeature / Body 前缀 support 执行和报告 source，不依赖隐藏的 `Originals`；前缀 support 会消费前序 AddSub feature 的 final-result `Refine=true` Shape。FreeCAD expected collector 已接入 `PartDesign::Line` + `PartDesign::LinearPattern` 的基础 native oracle；当前只把已对齐的 bbox / volume 冻结为硬验收，基础 pattern 的 topology 计数仍归入 transformed maker-history 收敛范围。
- PolarPattern：支持 `TransformMode=Features` 和 `Whole shape`，DatumLine / shape Edge / Sketch axis 轴，Extent、Spacing、SpacingPattern；Sketch axis 覆盖 `H_Axis` / `V_Axis` / `N_Axis` 和 construction `AxisN`。FreeCAD expected collector 已接入 `PartDesign::Line` + `PartDesign::PolarPattern` 的基础 native oracle，覆盖 DatumLine / Sketch axis Features 模式、SpacingPattern 和 Body-prefix Whole shape；旧 standalone Whole shape fixture 保留为 CAD Core 等价值用例，不作为 native golden。
- Scaled：支持 `TransformMode=Features` 和 `Whole shape`；Features 模式以第一个 original AddSubShape 体积质心为缩放中心，Whole shape 按 FreeCAD 空 originals 行为使用原点作为缩放中心。FreeCAD expected collector 已接入 `PartDesign::Scaled` 的基础 native oracle；Features 模式 bbox / volume / topology 已对齐，Whole shape 只冻结已对齐的 bbox / volume，topology 计数仍归入 transformed maker-history 收敛范围。
- MultiTransform：支持 Features 和 Whole shape 模式下 Mirrored / LinearPattern / PolarPattern / Scaled 子特征模板，非 Scaled 乘法组合，Scaled diagonal 组合；transformed copy source alias、原 feature stable alias 和 split / deleted terminal history 已由 `topo::namedShapeForTransformedCopy()` 承接，并有 P7 回归约束 copy 后的 `Mirrored.TransformN.*` / source-prefix ElementMap，以及 transformed / DressUp 链路的 `element_history_status` generated / terminal / merge 状态。FreeCAD expected collector 已接入 `PartDesign::MultiTransform` 的基础 native oracle，并按 FreeCAD 模板语义跳过 `Transformations` 子对象的 Shape 采集；LinearPattern+Mirrored、LinearPattern+Scaled diagonal、Whole shape + LinearPattern 已以 native bbox / volume 收口，其中 Scaled diagonal topology 已对齐，其余 topology 仍归入 transformed maker-history 收敛范围。

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

- `fixtures/p7` 覆盖 Datum / Origin、RefineModel + GenericShapeMapper history、Hole final-result refine、Hole 基础孔、Hole support-backed native oracle（blind / through-all / point / tapered / drill-point / head-cut / threaded standard heads / threaded dynamic DIN7984 / ISO2009 / ThreadDepth / clearance / ModelThread metric pipe-shell）、Hole cosmetic threaded、Hole fine threaded、Hole UNC / UNF / UNEF / NPT / BSP / BSW / BSF / ISOTyre threaded、Hole ISO metric / UTS / generic fallback thread clearance、Hole bundled 与外部追加 HoleCutType 资源表、Hole ThreadDepth / DIN76 参数归一化、Fillet / Chamfer Body-member native oracle、DressUp SupportTransform 与链式 SupportTransform、Mirrored basic native oracle、链式 DressUp SupportTransform Mirrored native geometry oracle、LinearPattern Whole shape Body support / refined prefix support、PolarPattern basic native oracle、PolarPattern Body-prefix Whole shape oracle、Scaled basic native oracle、MultiTransform basic native oracle、subtractive-only original 与 multi-original Add/Sub replay、transformed stable history diagnostics。
- P7 native expected 采集允许 `PartDesign::Hole`、Body-member Fillet / Chamfer、基础 DatumPlane + Mirrored、链式 DressUp SupportTransform Mirrored、基础 DatumLine + LinearPattern / PolarPattern、基础 Scaled / MultiTransform，但 Hole 必须有真实 sketch support；无 support 的 Hole fixture、diagnostic-only DressUp fixture、standalone PolarPattern Whole shape 和错误诊断型 MultiTransform fixture 在 `--skip-unsupported` 模式下跳过，防止把 detached/no-cut、bodyless lifecycle 或当前 CAD Core 输出写成 oracle。
- 剩余复杂路径必须明确 diagnostics；未接入 RefineModel 语义的特征族仍不能静默执行。
- transformed copy 通过 topo helper 继续传播 original stable key、AddSubShape slot history 和 split / deleted terminal history，再进入 Body boolean history 或 stable subname diagnostics；P7 transformed / DressUp expected 与 split/deleted diagnostic fixture 已约束 `element_history_status` 的 generated / terminal / merge 状态。
