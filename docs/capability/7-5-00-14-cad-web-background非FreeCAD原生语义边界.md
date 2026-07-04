# cad-web-background 非 FreeCAD 原生语义边界

## 背景

本文记录 `3c5ccff1fe2b1dea8a143a15755069ee33151913` 之后，`/Users/li/Chili3DProject/FreeCAD/cad-core` 迁移到 `/Users/li/Chili3DProject/cad-web-background/cad-core` 后仍保留的差异中，不能声明为 FreeCAD 原生语义的部分。

这些差异可以作为 `cad-web-background` 的产品契约或前端体验修复存在，但能力声明、fixture expected、roadmap 文档和后续迁移任务中必须标成 `cad_core_product_contract` / `non_native_parity`，不能写成 FreeCAD native parity。

## 结论

当前确认有三类差异不符合 FreeCAD 原生语义：

1. `SubtractivePipe` product PipeLaw 分支发布工具体，而不是 FreeCAD feature `Shape`。
2. PartDesign 轴引用接受几何共线的 BSpline / 非 Line 曲线作为轴。
3. `DatumPlane` 用 `Length` / `Width` 构造有限平面 shape，而不是 FreeCAD 原生的 infinite datum plane shape。

`fullSubname`、Datum frame 响应、无法解析 `StableSubList` 时返回结构化 diagnostic、Body 内 dress-up 链接到早期特征时的 Body-local 子路径解析，属于 cad-core Web DTO / 无状态后端契约或 FreeCAD-compatible 修复；它们不在本文的“不符合 FreeCAD 原生语义”范围内。

本文只记录需要在 capability、fixture expected 或 roadmap 中标成 `cad_core_product_contract` / `non_native_parity` 的行为差异。源码结构重排、helper 抽取、严格复现 FreeCAD 判断的诊断改进、以及 `cad-core/build` / `Testing/Temporary` 这类生成物不作为语义边界。

## 1. SubtractivePipe product PipeLaw 发布工具体

### 目标仓行为

目标仓：

- `/Users/li/Chili3DProject/cad-web-background/cad-core/src/part_design/feature_pipe.cpp`
- 函数：`executePipeFeature()`
- 差异点：`publishToolContractShape = bodyPrefix && !additive && usesCadCoreProductPipeLaw(pipeLaw)`

该分支在 `SubtractivePipe` 位于 Body 内、且 PipeLaw 来源是 `cad_core_product_contract` 时，把响应中的 `mesh`、`subshapes`、`shape`、`bbox`、`volume` 和 `namedShape` 发布为 `toolShape`，而不是布尔切割后的 `featureShape`。

目标仓已有 expected 也明确把该分支标成非 FreeCAD 原生：

- `/Users/li/Chili3DProject/cad-web-background/cad-core/fixtures/c6m3/expected/partdesign-pipe-interpolation-law-subtractive-product.freecad.json`
- 关键字段：`"reference": "CAD Core C6-M3 SubtractivePipe Interpolation LawSamples product-contract oracle; not FreeCAD native parity."`
- 关键字段：`"freecad_native_parity": false`

### FreeCAD 原生依据

FreeCAD 源码：

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::execute()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureAddSub.cpp::FeatureAddSub::getAddSubShape()`

关键语义：

- `FeaturePipe.cpp::Pipe::execute()` 先把 pipe 工具体写入 `AddSubShape`：
  - `AddSubShape.setValue(result.makeElementCompound(...))`
- 如果存在 base，`Pipe::execute()` 对 `{base, result}` 执行 `Fuse` 或 `Cut`：
  - `boolOp.makeElementBoolean(maker.c_str(), {base, result}, ...)`
- 真正的 feature `Shape` 写入布尔后的结果：
  - `Shape.setValue(getSolid(boolOp))`
  - 后续还会再次 `Shape.setValue(boolOp)`
- `FeatureAddSub::getAddSubShape()` 只把 `AddSubShape` 作为 additive/subtractive tool cache 暴露给下游 consumer：
  - additive 写入 `addShape`
  - subtractive 写入 `subShape`

因此 FreeCAD 的边界是：

- `AddSubShape` = 预布尔工具体 / removed-volume cache。
- `Shape` = feature 当前形状；Body 内 SubtractivePipe 时是 post-cut 结果。
- Body final shape 继续由 Body Tip / replay 语义决定。

### 判定

目标仓把 `SubtractivePipe` product PipeLaw 分支的响应主形状发布成 `toolShape`，不等同于 FreeCAD 的 feature `Shape` 语义。

这可以作为 `cad-web-background` 产品契约保留，前提是：

- capability / expected / docs 必须标成 `cad_core_product_contract` 或 `non_native_parity`。
- 不得把该分支作为 FreeCAD native expected。
- 若后续目标是 FreeCAD parity，响应主形状必须回到 `featureShape`，`toolShape` 只能保留在 AddSubShape / preview / product-only 字段中。

## 2. PartDesign 轴引用接受几何共线 BSpline / 非 Line 曲线

### 目标仓行为

目标仓：

- `/Users/li/Chili3DProject/cad-web-background/cad-core/src/part/edge_axis.cpp`
- `/Users/li/Chili3DProject/cad-web-background/cad-core/src/part_design/feature_extrude.cpp`
- `/Users/li/Chili3DProject/cad-web-background/cad-core/src/part_design/feature_revolved.cpp`
- `/Users/li/Chili3DProject/cad-web-background/cad-core/src/part_design/feature_transformed.cpp`

差异点：

- `EdgeAxisOptions::allowGeometricallyLinearCurve = true`
- `resolveEdgeAxis()` 对非 `GeomAbs_Line` / `GeomAbs_Circle` 的边做采样共线判定。
- 只要采样点落在首尾点定义的 3D 直线容差内，就接受为 `EdgeAxisKind::GeometricallyLinearCurve`。

目标仓相关回归包括：

- `pad-reference-axis-linear-bspline-edge`
- `pad-reference-axis-nonlinear-bspline-rejected`
- `partdesign-revolution-pipe-linear-bspline-axis`

目标仓文档 `/Users/li/Chili3DProject/cad-web-background/docs/BUG处理/7-4-10-49-【已实现】Revolution线性BSpline轴解析修复方案.md` 也把动机写成用户体验修复：显示采样点在直线上时，不因 OCCT 底层类型是 `GeomAbs_BSplineCurve` 而拒绝。

### FreeCAD 原生依据

FreeCAD 源码：

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp::ProfileBased::getAxis()`
- 使用方包括：
  - `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp::FeatureExtrude::computeDirection()`
  - `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolved.cpp`

`ProfileBased::getAxis()` 中 `getAxisFromEdge` 的原生判断是：

- `BRepAdaptor_Curve::GetType() == GeomAbs_Line`：接受为直线轴。
- `BRepAdaptor_Curve::GetType() == GeomAbs_Circle`：接受圆 / 圆弧轴。
- 其它类型直接抛错：
  - `Edge must be a straight line, circle or arc of circle`

该路径没有调用 `Part::GeomCurve::isLinear()`，也没有对 BSpline / Bezier 做“几何共线即可”的补充判定。

### 判定

目标仓接受几何共线 BSpline / 非 Line 曲线作为 PartDesign ReferenceAxis，是 CAD Core / backend 产品扩展，不是 FreeCAD 原生语义。

这可以作为前端体验修复保留，前提是：

- capability 必须标成 product extension，不得声明 FreeCAD native parity。
- FreeCAD expected fixture 不应把该行为写成原生 oracle。
- 相关诊断文案应避免暗示 FreeCAD 本身接受这类轴。
- 若后续目标是严格 FreeCAD parity，应只接受 `GeomAbs_Line`、`GeomAbs_Circle`，并拒绝 BSpline 轴，即使它几何共线。

## 3. DatumPlane 用 Length / Width 构造有限平面 shape

### 目标仓行为

目标仓：

- `/Users/li/Chili3DProject/cad-web-background/cad-core/src/part_design/datum_plane.cpp`
- 函数：`executeDatumPlane()`
- 差异点：
  - `datumPlaneSize(object, "Length")`
  - `datumPlaneSize(object, "Width")`
  - `BRepBuilderAPI_MakeFace(plane, -length / 2.0, length / 2.0, -width / 2.0, width / 2.0, Precision::Confusion())`

该路径把 `Length` / `Width` 读成实际建模参数，并构造一个有限矩形平面 face。返回结果中还发布：

- `length`
- `width`
- `origin`
- `x_axis`
- `normal`

其中 `origin` / `x_axis` / `normal` 是 cad-core Web DTO 需要的 datum frame 响应字段；单独发布 frame 字段不是 FreeCAD 原生几何差异。真正不符合 FreeCAD 原生语义的是：目标仓把 `Length` / `Width` 用进了 `DatumPlane` 的 shape 构造、bbox / mesh / subshape 结果和相关 fixture expected。

目标仓相关回归包括：

- `partdesign-datum-user-offset-plane-sketch`

### FreeCAD 原生依据

FreeCAD 源码：

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/DatumPlane.cpp::Plane::Plane()`

关键语义：

- `Length` / `Width` 是 `Size` 分组下的 `App::Prop_Output` 属性。
- FreeCAD 源码注释写明这些属性只和视觉外观有关：
  - `These properties are only relevant for the visual appearance.`
- 原生 shape 构造是无界平面：
  - `BRepBuilderAPI_MakeFace builder(gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)))`
  - `myShape.Infinite(Standard_True)`
  - `Shape.setValue(myShape)`

因此 FreeCAD 的边界是：

- `Length` / `Width` = datum plane 的显示尺寸 / ViewProvider 相关输出属性。
- `Shape` = 标记为 infinite 的 datum plane shape。
- Attachment / Placement 可以驱动草图或局部坐标系，但有限矩形 face 不是 FreeCAD 原生 DatumPlane 的建模 shape 语义。

### 判定

目标仓用 `Length` / `Width` 构造有限 `DatumPlane` shape，是 cad-core Web / product display contract，不等同于 FreeCAD 原生 DatumPlane shape。

这可以作为前端展示和无状态后端 DTO 便利保留，前提是：

- capability / expected / docs 必须把有限平面 shape、bbox、mesh、subshape 结果标成 product extension 或 `non_native_parity`。
- `origin` / `x_axis` / `normal` 这类 frame 响应字段可以标成 Web DTO 扩展，但不能用它们掩盖有限 shape 与 FreeCAD native 的差异。
- 若后续目标是严格 FreeCAD parity，应保持 DatumPlane shape 的 infinite plane 语义；`Length` / `Width` 只能影响显示或 product-only preview 字段，不应作为 native shape expected。

## 已核查但不归入非原生语义

以下差异在目标仓存在，但不要归入本文前三类 `non_native_parity`。它们要么是 Web DTO / 无状态后端契约，要么是为了更接近 FreeCAD Body / Tip / 引用解析语义的修复。

### 1. `fullSubname`、结构化 diagnostic 和响应字段扩展

目标仓：

- `/Users/li/Chili3DProject/cad-web-background/cad-core/src/runtime/recompute.cpp`
- `/Users/li/Chili3DProject/cad-web-background/cad-core/src/runtime/capability_contract.cpp`

这些差异包括：

- 在 `results[].subshapes[]` 中发布 `fullSubname`。
- 无法解析 `StableSubList` 时返回结构化 diagnostic。
- 对 Datum 对象发布 `origin` / `x_axis` / `normal` 等 frame 字段。

这些字段不是 FreeCAD 原生 API 的逐字镜像，但它们是 Web recompute DTO 和无状态后端引用恢复所需的协议层扩展。只要不把这些字段本身声明成 FreeCAD native API，它们不构成几何语义不一致。

需要特别区分：

- Datum frame 字段 = DTO 扩展，不归入非原生几何语义。
- DatumPlane 有限 shape = 几何 shape 语义差异，归入本文第 3 类。

### 2. Body 内 dress-up 链接早期特征的 Body-local 子路径解析

目标仓：

- `/Users/li/Chili3DProject/cad-web-background/cad-core/src/part_design/feature_dress_up.cpp`
- 相关函数：
  - `objectLocalSubnameOrOriginal()`
  - `stableSubnameForBodyTopoShapeLink()`
  - `bodyTopoShapeAtFeature()`
  - `previousBodyTopoShape()`

相关回归包括：

- `body-revolution-filletpreview-tip-edge`
- `body-revolution-filletpreview-stable-collision`

这组差异处理的是：Body 内后续 dress-up 特征引用同一 Body 中较早特征的边 / 面时，目标仓需要在无状态请求里重建“Body 在目标特征处的累计 shape”，并把 `stableSubname` / `fullSubname` 候选还原成目标特征本地可解析的子路径。

FreeCAD 依据：

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp::Body::execute()`
  - Body 执行时按 Group / Tip 发布累计 shape。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.h::DressUp::Base`
  - dress-up 的 Base 链接和同一 Body 内特征链有关。

判定：

- 这不是为了偏离 FreeCAD 的产品扩展。
- 它是 cad-core 在无状态 DocumentObject graph 输入下补齐 FreeCAD Body / Tip / 早期特征引用语义的实现细节。
- 相关 expected 可以声明 FreeCAD-compatible / backend contract，但不应标成 `non_native_parity`。

### 3. `edge_axis` helper 的严格模式使用点

目标仓新增：

- `/Users/li/Chili3DProject/cad-web-background/cad-core/include/cad_core/part/edge_axis.h`
- `/Users/li/Chili3DProject/cad-web-background/cad-core/src/part/edge_axis.cpp`

这个 helper 同时支持严格 FreeCAD 模式和产品扩展模式。不能因为存在 `resolveEdgeAxis()` 就把所有使用点都归为非原生。

归类规则：

- `EdgeAxisOptions::allowGeometricallyLinearCurve = true` 的 PartDesign ReferenceAxis 使用点，归入本文第 2 类非原生产品扩展。
- 未开启 `allowGeometricallyLinearCurve` 的使用点仍按 `GeomAbs_Line` / `GeomAbs_Circle` 严格判断，不归入非原生语义。

已核查的严格使用点包括：

- `/Users/li/Chili3DProject/cad-web-background/cad-core/src/part/part_extrusion.cpp`
  - `Part::Extrusion` 的 `DirLink` 仍应按 FreeCAD `GeomAbs_Line` 语义处理。
- `/Users/li/Chili3DProject/cad-web-background/cad-core/src/part_design/feature_draft.cpp`
  - `Draft` 的 `PullDirection` 仍应按 FreeCAD 直线方向语义处理。
- `/Users/li/Chili3DProject/cad-web-background/cad-core/src/part_design/feature_transformed.cpp`
  - 只有开启几何共线曲线容忍的 axis 路径属于第 2 类；其它 direction / linear-pattern 类严格路径不属于第 2 类。

## 文档维护规则

后续如果继续保留上述三类目标仓扩展，应同步维护：

- `cad-web-background/cad-core/fixtures/**/expected/*.freecad.json` 中的 `freecad_native_parity=false` 或等价说明。
- `cad-web-background/cad-core/src/runtime/capability_contract.cpp` 的 capability 描述。
- `cad-web-background/docs/接口规定` 或 `docs/BUG处理` 中的产品契约说明。

后续如果要把它们升级为 FreeCAD parity，必须先补可复现的 FreeCADCmd native oracle。没有 native oracle 前，不得仅凭目标仓运行结果或用户可见显示效果把这些行为归类为 FreeCAD 原生语义。

维护时还要避免两类误判：

- 不要把 Web DTO 字段、结构化 diagnostic、`fullSubname`、Datum frame 响应这类协议扩展写成 FreeCAD native API。
- 不要把为了恢复 FreeCAD Body / Tip / 早期特征引用语义的 backend 修复误标成 `non_native_parity`。
