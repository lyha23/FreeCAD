# P7：PartDesign 常用生态

P7 的目标是在 P3-P6 底座稳定后，扩展前端参数化建模最常用的 Body 特征：Datum、Refine、Hole、Fillet、Chamfer、Pattern、Mirror、MultiTransform、Scaled。

## 前置条件

- P4 的 LinkSub / Placement 模型稳定。
- P6 的 `NamedShape` / `ElementMap` / MapperHistory 主路径可用。
- P3b 的 `FeatureExtrude` 方向、双侧和 placement 不回退。

不要在这些前置条件缺失时先铺开 Pattern / Mirror / DressUp，否则会复制不完整的 source feature 和 subshape 引用语义。

## FreeCAD 语义来源

| 能力 | FreeCAD 参考位置 |
| --- | --- |
| Datum | `src/Mod/PartDesign/App/Datum*.cpp` |
| Hole | `src/Mod/PartDesign/App/FeatureHole.cpp` |
| DressUp 基类 | `src/Mod/PartDesign/App/FeatureDressUp.cpp` |
| Fillet | `src/Mod/PartDesign/App/FeatureFillet.cpp` |
| Chamfer | `src/Mod/PartDesign/App/FeatureChamfer.cpp` |
| Refine | `src/Mod/PartDesign/App/FeatureRefine.cpp` |
| Transformed | `src/Mod/PartDesign/App/FeatureTransformed.cpp` |
| Pattern / Mirror / MultiTransform | `src/Mod/PartDesign/App/Feature*.cpp` 对应子类 |

## Step 44：Datum / Origin

目标：

- 支持 Datum Plane / Line / Point / CoordinateSystem。
- 支持 Body / Origin 下的基础坐标关系。
- Datum 可作为 sketch support、extrude direction、pattern axis。

fixtures：

```text
fixtures/p7/
  datum-plane-sketch-support.json
  datum-line-reference-axis.json
  datum-point-placement.json
  datum-coordinate-system-sketch-support.json
  datum-coordinate-system-reference-axis.json
  origin-identity-placement.json
```

当前状态：

- `PartDesign::Plane` / `PartDesign::Line` / `PartDesign::Point` 已在 P4 作为基础 Datum executor 接入。
- `PartDesign::CoordinateSystem` 已接入 P7：默认输出 FreeCAD `DatumCS` 的 Z 平面，按 Placement 输出 `x_axis` / `y_axis` / `z_axis`，并可作为 sketch support 或 `ReferenceAxis` 的 `X_Axis/Y_Axis/Z_Axis` 目标。
- `App::Origin` 已接入 P7：本地 Placement 按 FreeCAD identity 固定语义忽略，但仍继承 parent Part / Body group placement。
- Origin controlled features、`OriginFeatures` 自动建轴建面、AttachEngine 复杂 attachment 和 CoordinateSystem 子平面 `X/Y` support 仍未完整迁移。

## Step 45：Refine

目标：

- 迁移 FreeCAD refine maker / object chain。
- Refine 属性参与 Body 链和 topo naming。
- 删除输出端 refine fallback。

当前状态：

- `PartDesign::Pad` / `PartDesign::Pocket` 已接受 `Refine` 和 `FuzzyTolerance` 属性。
- `Refine=false` 已按 FreeCAD `FeatureRefine::refineShapeIfActive()` 的 no-op 语义执行，不改变已有 shape / topo 输出。
- `Refine=true` 当前返回 `unsupported_property`，明确指向未迁移的 `BRepBuilderAPI_RefineModel` / `FaceUniter` maker path；不得用 `ShapeUpgrade_UnifySameDomain` 或输出端清理冒充完成。

fixtures：

```text
fixtures/p7/
  pad-refine-false.json
  pad-refine-true-known-gap.json
  body-refine-pad-pocket.json
  refine-reference-stability.json
```

## Step 46：Hole

目标：

- 支持常用 Hole 参数：Profile、Depth、Diameter、Type、Threading 基础边界。
- Hole 通过 Body subtractive 通道执行。
- Hole placement / support 与 Sketch / Datum 对齐。

fixtures：

```text
fixtures/p7/
  hole-blind-depth.json
  hole-through-all.json
  hole-without-base.json
  hole-threaded-known-gap.json
```

当前状态：

- `PartDesign::Hole` 已接入基础 subtractive executor：按 FreeCAD `Hole::execute()` / `Hole::findHoles()`，从 `Profile` 的 Sketch raw shape 中读取 circle / arc center，结合 `Diameter`、`DepthType=Dimension/ThroughAll`、`Depth`、`Reversed` 和 `BaseProfileType` 生成平底圆柱 tool。
- Hole 不直接修改 Body；executor 只写 `addSubShapes[Hole].subShape`，由 Body 继续走 P6 的 `makeElementBooleanFromSources(..., Cut)` 主路径。
- 当前明确支持 `DrillPoint=Flat`。`Threaded=true`、`ModelThread=true`、`Tapered=true`、非 `None` `HoleCutType` 和 FreeCAD 默认 `DrillPoint=Angled` 都返回 `unsupported_property`，等待 `FeatureHole.cpp` 的螺纹、沉孔 / 锥孔和角钻尖轮廓完整迁移。
- 点驱动 Hole、孔口 profile 高级过滤、thread clearance 表和 Hole 自身完整 maker history 仍未迁移。

## Step 47：Fillet / Chamfer

目标：

- DressUp 通过 LinkSub 读取 base edge / face。
- 半径 / chamfer 参数进入 OCCT builder。
- 修改 base feature 后引用通过 P6 topo naming 恢复或 diagnostics。

fixtures：

```text
fixtures/p7/
  fillet-pad-edge.json
  chamfer-pad-edge.json
  fillet-missing-edge.json
  chamfer-invalid-size.json
  fillet-reference-after-length-change.json
  chamfer-missing-edge.json
```

当前状态：

- `PartDesign::Fillet` / `PartDesign::Chamfer` 已接入基础 DressUp executor：`Base` 使用 `App::PropertyLinkSub` 指向 solid 的 `EdgeN` / `FaceN`，Edge 会按 FreeCAD `DressUp::getContinuousEdges()` 要求过滤为两相邻面且 C0 连续，Face 会展开为其边。
- Fillet 支持 `Radius` 和 `UseAllEdges`，调用 `BRepFilletAPI_MakeFillet` 并将 maker history 写入 `NamedShape`；Chamfer 支持 `ChamferType=Equal distance` / `Two distances` / `Distance and Angle`、`Size`、`Size2`、`Angle`、`FlipDirection` 和 `UseAllEdges`，调用 `BRepFilletAPI_MakeChamfer`。
- DressUp executor 产出完整替换 solid；Body 遍历 Group 时遇到无 add/sub 但产出 solid 的 DressUp Tip，会用该 solid 替换当前 body 结果，而不是继续沿用前一个 Pad/Pocket。
- `SupportTransform=true` 当前返回 `unsupported_property`，因为 FreeCAD `DressUp::getAddSubShape()` 的 transformed-family AddSubShape 缓存路径还未迁移。Fillet / Chamfer 复杂引用变更后的完整稳定恢复仍依赖 P6 MapperHistory 继续补齐。

## Step 48：Transformed family

目标：

- 支持 `FeatureTransformed` 基础语义。
- LinearPattern、PolarPattern、Mirrored、MultiTransform、Scaled 复用 source feature。
- source feature 的 NamedShape / ElementMap history 传播到 transformed result。

fixtures：

```text
fixtures/p7/
  mirrored-pad-datum-plane.json
  mirrored-whole-shape-known-gap.json
  linear-pattern-pad-datum-line.json
  linear-pattern-pad-two-directions.json
  linear-pattern-custom-spacings.json
  linear-pattern-spacing-pattern.json
  polar-pattern-pad-datum-line.json
  polar-pattern-spacing-pattern.json
  polar-pattern-whole-shape-known-gap.json
  scaled-pad-factor-two.json
  scaled-invalid-factor.json
  scaled-whole-shape-known-gap.json
  multi-transform-linear-mirror.json
  multi-transform-scaled-diagonal.json
  multi-transform-scaled-divisor-known-gap.json
```

当前状态：

- `PartDesign::Mirrored` 已接入 `TransformMode=Features` 基础路径：`Originals` 必须指向已产生 `AddSubShape` 的 additive / subtractive feature，support 按 FreeCAD `Transformed::getBaseObject()` 使用 `BaseFeature` 或第一个 original。
- `MirrorPlane` 支持 `PartDesign::Plane` DatumPlane 和 solid 的 planar `FaceN`；镜像变换按 FreeCAD `Mirrored::createTransformations()` 使用 `gp_Trsf::SetMirror(gp_Ax2(point, normal))`。
- Mirrored executor 对 original 的 add/sub tool shape 做 transform 后，按 `Transformed::execute()` 的 Features 模式 fuse / cut 到 support solid；Mirrored 自身产出完整 replacement solid，Body Tip 指向 Mirrored 时使用该结果。
- `PartDesign::LinearPattern` 已接入 `TransformMode=Features` 的双方向基础路径：`Direction` / `Direction2` 支持 DatumLine / DatumPlane / solid Edge / Face，`Mode=Extent` 按 `Length / (Occurrences - 1)` 计算步长，`Mode=Spacing` 支持全局 `Offset`、自定义 `Spacings` 和 `SpacingPattern`，`Occurrences` 包含原件。
- `PartDesign::PolarPattern` 已接入 `TransformMode=Features` 基础路径：`Axis` 支持 DatumLine 和 shape `EdgeN` 的直线 / 圆 / 圆弧轴，`Mode=Extent` 保留 FreeCAD `Angle=360` 时按 `Angle / Occurrences` 分布的特例，`Mode=Spacing` 支持全局 `Offset`、自定义 `Spacings` 和 `SpacingPattern`。
- `PartDesign::Scaled` 已接入 `TransformMode=Features` 基础路径：使用第一个 original `AddSubShape` 的体积质心作为缩放中心，`Factor` 到 `Occurrences` 做线性插值，Factor 过小或 occurrence 不足返回结构化 diagnostics。
- `PartDesign::MultiTransform` 已接入 `TransformMode=Features` 基础路径：`Transformations` 中的 Mirrored / LinearPattern / PolarPattern / Scaled 子特征作为 request-local template 执行，非 Scaled 子项按 FreeCAD 乘法组合，Scaled 子项按 diagonal 规则组合并围绕当前 slice COG 重新构造 scale；Scaled occurrence 不整除前序 transform 数量时返回结构化 diagnostics。
- transformed copy 当前按 FreeCAD `TopoShape::makeElementTransform()` 的 `copyElementMap(tmp, op)` 思路复制 original stable key，再交给 boolean maker history 继续传播。`TransformMode=Whole shape` 已通过 Mirrored / PolarPattern / Scaled known-gap fixture 固定为显式 `unsupported_property`；LinearPattern / PolarPattern sketch axis 方向尚未迁移。

## 完成定义

P7 完成需要同时满足：

- 每个支持的 PartDesign feature 有成功 fixture、错误 fixture、FreeCAD oracle。
- DressUp 和 Transformed family 都走统一 LinkSub / topo naming。
- Refine 不再靠输出 fallback。
- Body Tip / Group / BaseFeature 顺序和 FreeCAD 对齐。
- Pattern / Mirror 不复制不完整的 source feature 几何实现。
