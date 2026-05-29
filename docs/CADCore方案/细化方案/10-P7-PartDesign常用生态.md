# P7：PartDesign 常用生态

P7 在 P3-P6 底座上扩展常用 Body feature。原则是所有 feature 都继续消费 document/link/topo 主路径，不引入孤立几何能力。

## 当前基线

- Datum / Origin：`PartDesign::Plane`、`Line`、`Point`、`CoordinateSystem`、`App::Origin` 已接入基础 executor。
- Refine：`Refine=false` 按 FreeCAD no-op 执行；`Refine=true` 返回 `unsupported_property`，等待 RefineModel / FaceUniter maker path。
- Hole：支持 Sketch circle / arc center 与 Sketch point 的平底圆柱盲孔和通孔；支持 Tapered、Counterbore、Countersink、Counterdrill、Angled drill point、非建模 Threaded tap-drill 和 ISO metric clearance fit 的基础路径；结果走 subtractive `AddSubShape` 后由 Body cut。
- Fillet / Chamfer：支持基础 Edge / Face Base、连续边过滤和 OCCT fillet/chamfer maker，产出 replacement solid；同时按 `DressUp::getAddSubShape()` 生成基础 AddSubShape cache，`SupportTransform=true` 可被 transformed family 的 Features 模式消费。
- Mirrored：支持 `TransformMode=Features` 和 `Whole shape`，MirrorPlane 可为 DatumPlane 或 solid planar Face。
- LinearPattern：支持 `TransformMode=Features` 和 `Whole shape`，DatumLine / DatumPlane / solid Edge / Face / Sketch axis 方向，Extent、Spacing、Spacings、SpacingPattern 和双方向；Sketch axis 覆盖 `H_Axis` / `V_Axis` / `N_Axis` 和 construction `AxisN`。
- PolarPattern：支持 `TransformMode=Features` 和 `Whole shape`，DatumLine / shape Edge / Sketch axis 轴，Extent、Spacing、SpacingPattern；Sketch axis 覆盖 `H_Axis` / `V_Axis` / `N_Axis` 和 construction `AxisN`。
- Scaled：支持 `TransformMode=Features` 和 `Whole shape`；Features 模式以第一个 original AddSubShape 体积质心为缩放中心，Whole shape 按 FreeCAD 空 originals 行为使用原点作为缩放中心。
- MultiTransform：支持 Features 和 Whole shape 模式下 Mirrored / LinearPattern / PolarPattern / Scaled 子特征模板，非 Scaled 乘法组合，Scaled diagonal 组合。

## 已知缺口

- Hole ModelThread 实体螺纹、完整 thread profile / clearance 表和标准件表驱动头部尺寸迁移。
- Fillet / Chamfer 复杂参数组合、链式 `SupportTransform` ownership、复杂引用变更后的完整稳定恢复。
- transformed family 的完整 AddSubShape ownership 和 MapperHistory。

## cad-core 落点

| 文件 | 职责 |
| --- | --- |
| `features/datum_coordinate_system.*` | CoordinateSystem / Origin |
| `features/hole.*` | Hole subtractive executor、基础 tapered / head-cut / drill-point 轮廓和 ISO metric thread diameter |
| `features/dress_up.*` | Fillet / Chamfer 和基础 DressUp AddSubShape cache |
| `features/transformed.*` | Mirrored / Pattern / Scaled / MultiTransform |
| `features/body.*` | replacement solid 和 Body Tip |
| `topo/named_shape.*` | maker history 和 transformed source alias |

## FreeCAD 依据

- `src/Mod/PartDesign/App/FeatureHole.cpp`
- `src/Mod/PartDesign/App/FeatureDressUp.cpp`
- `src/Mod/PartDesign/App/FeatureFillet.cpp`
- `src/Mod/PartDesign/App/FeatureChamfer.cpp`
- `src/Mod/PartDesign/App/FeatureTransformed.cpp`
- `src/Mod/PartDesign/App/FeatureMirrored.cpp`
- `src/Mod/PartDesign/App/FeatureLinearPattern.cpp`
- `src/Mod/PartDesign/App/FeaturePolarPattern.cpp`
- `src/Mod/PartDesign/App/FeatureScaled.cpp`
- `src/Mod/PartDesign/App/FeatureMultiTransform.cpp`

## 验收

- `fixtures/p7` 覆盖 Datum / Origin、Refine、Hole 基础孔、Hole point profile、Hole tapered、Hole head-cut / drill-point、Hole cosmetic threaded、Hole thread clearance、Fillet、Chamfer、DressUp SupportTransform、Mirrored、LinearPattern、PolarPattern、Scaled、MultiTransform。
- Hole ModelThread、Refine 等缺口必须明确 diagnostics。
- transformed copy 继续传播 original stable key，再进入 Body boolean history。
