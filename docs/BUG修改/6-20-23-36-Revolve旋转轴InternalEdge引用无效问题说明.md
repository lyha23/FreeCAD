# Revolve 旋转轴 InternalEdge 引用无效问题说明

## 当前结论

当前旋转失败的直接原因不是 `Profile`、角度或 OCCT 旋转构造失败，而是 `ReferenceAxis` 传入了错误的子元素名：

```json
"ReferenceAxis": {
  "PropertyType": "App::PropertyLinkSub",
  "value": "草图 11:28:09 PM",
  "SubList": ["InternalEdge2"]
}
```

后端诊断：

```json
{
  "code": "invalid_axis",
  "message": "ReferenceAxis subshape InternalEdge2 is not available on 草图 11:28:09 PM",
  "object": "RevolvePreview",
  "property": "ReferenceAxis",
  "subname": "InternalEdge2",
  "target": "草图 11:28:09 PM"
}
```

`InternalFace1` 用在 `Profile.SubList` 上是合理的；问题只在 `ReferenceAxis.SubList=["InternalEdge2"]`。

## 现象复盘

输入里同一个 Sketch 同时包含：

- 一个闭合矩形轮廓，用作 `Profile.SubList=["InternalFace1"]`。
- 一条额外 `LineSegment`，用户意图作为旋转轴。
- 前端最终把轴写成 `ReferenceAxis.SubList=["InternalEdge2"]`。

后端执行 `PartDesign::Revolution` 时，已经解析到 profile，随后进入轴解析。轴解析失败后，Body 结果为空：

```json
"results": [
  {
    "mesh": null,
    "object": "RevolvePreviewBody",
    "subshapes": []
  }
]
```

## FreeCAD 语义依据

FreeCAD 的 `Revolved::updateAxis()` 从 `ReferenceAxis` 读取对象和 subname，然后调用 `ProfileBased::getAxis(...)`：

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolved.cpp::Revolved::updateAxis`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp::ProfileBased::getAxis`

同一草图作为轴源时，`ProfileBased::getAxis()` 先识别这些草图轴名：

- `V_Axis`
- `H_Axis`
- `N_Axis`
- `AxisN`

其中 `AxisN` 来自草图里的构造线。`SketchObject::getAxisCount()` 和 `SketchObject::getAxis(int axId)` 只统计 `construction == true` 且类型是 `GeomLineSegment` 的几何。

也就是说，如果用户选择的是草图构造线作为旋转轴，提交给后端的 subname 应该是 `Axis0`、`Axis1` 这类草图轴名，而不是 `InternalEdgeN`。

## 当前 cad-core 行为

当前几何服务权威实现位于：

- `/Users/li/Chili3DProject/cad-web-background/cad-core/src/part_design/feature_revolved.cpp`

关键流程：

1. `resolveReferenceAxis()` 读取 `ReferenceAxis`。
2. 如果轴对象就是 profile 所在草图，先调用 `sketchAxis(subname, ...)`。
3. `sketchAxis()` 只接受 `H_Axis`、`V_Axis`、`N_Axis`、`AxisN`。
4. `InternalEdge2` 不属于上述草图轴名，于是继续走普通 subshape 查找。
5. 普通查找使用当前 sketch raw shape 的 subshape；`InternalEdge2` 是 sketch internal/profile 结果里的请求局部名字，不在 raw shape 上，于是报：

```text
ReferenceAxis subshape InternalEdge2 is not available on 草图 11:28:09 PM
```

后端已有正确 fixture 示例：

- `/Users/li/Chili3DProject/cad-web-background/cad-core/fixtures/c51m1/partdesign-revolution-sketch-axisn.json`

该 fixture 中构造线写法是：

```json
{ "kind": "LineSegment", "start": [0, 0], "end": [0, 1], "construction": true }
```

旋转轴引用写法是：

```json
"ReferenceAxis": {
  "PropertyType": "App::PropertyLinkSub",
  "value": "SketchRevolution",
  "SubList": ["Axis0"]
}
```

## 前端当前问题点

当前前端落点：

- `/Users/li/Chili3DProject/my-chili3d/src/lib/command/create/resolve/revolve.svelte.ts`
- `/Users/li/Chili3DProject/my-chili3d/src/lib/cad-document/revolve.ts`

`axisRefFromShape()` 当前对 edge token 直接取：

```ts
subname: token.subname || token.subshapeName
```

这会把用户在 profile/internal mesh 上点到的 `InternalEdgeN` 原样写进 `ReferenceAxis.SubList`。对 `Profile` 来说，`InternalFaceN` 是合法的请求局部 profile 区域；但对 `ReferenceAxis` 来说，`InternalEdgeN` 当前不是合法草图轴契约。

## 推荐修改方向

优先修前端提交契约，不建议先在后端无条件把 `InternalEdgeN` 当轴兜底。

### 1. 构造线作为旋转轴

如果产品语义是“这条草图线专门作为旋转轴”，应在草图几何里把该 `LineSegment` 标为构造线：

```json
{
  "kind": "LineSegment",
  "start": [-236.75216479736696, 530.7511679041904],
  "end": [536.2032079992418, -857.6824087821578],
  "construction": true
}
```

然后 `ReferenceAxis` 提交为对应构造线序号：

```json
"ReferenceAxis": {
  "PropertyType": "App::PropertyLinkSub",
  "value": "草图 11:28:09 PM",
  "SubList": ["Axis0"]
}
```

如果草图里有多条构造线，`AxisN` 的 `N` 按构造线在 sketch geometry 中的顺序编号。

### 2. 普通草图边作为旋转轴

如果产品语义允许选择普通草图边作为旋转轴，前端不能提交 `InternalEdgeN`，应尽量提交 raw sketch edge 名，如：

```json
"SubList": ["Edge5"]
```

可行策略：

- 当 selection token 有稳定 raw edge 名，例如 `stableSubname == "Edge5"`，优先用 `Edge5` 作为 `ReferenceAxis.SubList`。
- 如果只能拿到 `InternalEdgeN`，且无法证明它对应唯一 raw `EdgeN`，应在前端拒绝本次选择并提示“请选择草图构造线或可稳定引用的线边作为旋转轴”。
- 不要把 profile/internal mesh 的边界碎片直接当作长期轴引用写进 feature params。

### 3. 内置草图轴

如果使用草图自身坐标轴，继续提交：

```json
["H_Axis"]
["V_Axis"]
["N_Axis"]
```

这些是 FreeCAD / cad-core 已支持的同一草图轴名。

### 4. 外部线对象或实体边

如果轴来自 `Part::Line`、`App::Line`、`PartDesign::Line` 或已有实体边，应提交对应对象及其 raw subshape，例如：

```json
"ReferenceAxis": {
  "PropertyType": "App::PropertyLinkSub",
  "value": "PartAxis",
  "SubList": ["Edge1"]
}
```

已有后端 fixture：

- `/Users/li/Chili3DProject/cad-web-background/cad-core/fixtures/c5m1/partdesign-revolution-part-edge-axis.json`

## 后端是否需要兼容 InternalEdgeN

只有在明确决定“同一草图 internal edge 也可以作为旋转轴”时，才考虑改后端。改动不能做成静默兜底，必须先补 FreeCAD oracle 或至少写清 FreeCAD 行为差异。

候选后端方案：

1. 在 `resolveReferenceAxis()` 中识别同一 sketch 的 `InternalEdgeN`。
2. 只在 `shapeValue.kind == Sketch` 且存在 `internalShape` 时，从 `internalShape` 解析该 `InternalEdgeN`。
3. 解析结果必须是直线或圆/圆弧 edge，继续复用 `axisFromEdge()`。
4. 如果 `InternalEdgeN` 是 split/profile 边界碎片，且没有稳定 raw edge 证据，应返回明确 diagnostic，而不是自动猜轴。

风险：

- `InternalEdgeN` 是请求局部名字，不是长期稳定引用。
- profile split 后同一原始线可能被切成多个 internal fragments，直接选 fragment 可能导致轴不等于用户原始草图线。
- 这会扩大后端引用契约，需要同步前端 params、写盘、编辑回显和 reference update 语义。

因此当前更推荐：前端把轴引用规整成 `AxisN` 或 raw `EdgeN`；后端保持 `InternalEdgeN` 非轴引用的快速失败。

## 验收标准

本问题修复后，至少满足：

1. 复现请求不再提交 `ReferenceAxis.SubList=["InternalEdge2"]`。
2. 若轴线是构造线，请求中对应 `LineSegment` 带 `construction: true`，`ReferenceAxis.SubList` 使用 `AxisN`。
3. 若轴线是普通草图边，请求中 `ReferenceAxis.SubList` 使用可解析的 raw `EdgeN`；无法映射时前端应拒绝选择。
4. `Profile.SubList=["InternalFace1"]` 保持不变，不要把 profile 选择逻辑一起回退。
5. 后端不再返回 `invalid_axis: ReferenceAxis subshape InternalEdgeN is not available`。
6. 至少补一个针对该场景的回归用例：
   - 构造线轴：闭合 profile + construction line + `Axis0`。
   - 普通边轴：闭合 profile + raw `EdgeN` 轴，若产品确认支持。

## 非目标

- 不把 `InternalFaceN` 从 profile 选择中移除。
- 不把所有 `InternalEdgeN` 自动改写成 `EdgeN`。
- 不在后端用几何排序、bbox、线段坐标相似度猜 raw edge。
- 不新增旧 wasm / ShapeFactory 路径绕过 `/cad/recompute`。
