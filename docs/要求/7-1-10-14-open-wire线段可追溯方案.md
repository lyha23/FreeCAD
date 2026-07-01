# open wire 线段可追溯方案

## 背景

当前 CAD Core 已经能在 open-only Sketch 的 FreeCAD-style `InternalShape` 为空时，把 raw sketch `Shape` 的边作为运行态 mesh 返回：

- `cad-core/src/sketcher/sketch_internal_result.cpp`：空 `InternalShape` 但 raw shape 含 `TopAbs_EDGE` 时，调用 `part::meshForShape(input.rawShape)`。
- `cad-core/src/part/shape_exporter.cpp`：`edgeSegments` 来自 `TopExp::MapShapes(shape, TopAbs_EDGE, ...)`。
- `cad-core/src/runtime/recompute.cpp`：`responseMesh()` 把 mesh edge id 加上对象前缀，形成 `Sketch:EdgeN`。

这解决的是“前端能拾取 open wire 的可见边”，但还没有解决“线段修改后仍能追溯到同一条草图几何”。现在的 `EdgeN` 是本次请求内的拓扑枚举名，不是跨编辑稳定身份。只要新增、删除、重排几何，或 raw shape 组合顺序变化，旧 `Edge2` 就不能严格证明等于新 `Edge2`。

## 目标

让 open wire 的可见边在以下场景中可追溯：

1. 修改同一条线段的端点、长度、角度后，前端能从旧引用恢复到新请求里的对应 `Sketch:EdgeN`。
2. 在该线段前后新增或删除其它线段后，只要原线段的 FreeCAD geometry id 未变，仍能找到对应 raw edge。
3. 响应中同时保留本次显示拾取名 `Sketch:EdgeN` 和跨编辑稳定身份 `g<GeometryId>`。
4. open-only sketch 的 `InternalShape` 仍保持 empty；不得伪造 `InternalFaceN` / `InternalEdgeN`。

## FreeCAD 依据

FreeCAD 不把普通 Sketch edge 的稳定身份建立在 `EdgeN` 枚举上，而是建立在 Sketch geometry id 上：

- `src/Mod/Sketcher/App/SketchGeometryExtension.cpp::SketchGeometryExtension::saveAttributes()` 保存 `"id"`。
- `src/Mod/Sketcher/App/SketchObject.cpp::onSketchRestore()` 为没有 id 的 geometry 生成 id，并维护 `geoMap`。
- `src/Mod/Sketcher/App/SketchObject.cpp::convertSubName()` 把普通 `EdgeN` 转为 `g<GeometryFacade::getId(geo)>`。
- `src/Mod/Sketcher/App/GeometryFacade.cpp::GeometryFacade::copyId()` 在替换几何时复制旧 geometry id。
- `src/Mod/Sketcher/App/SketchObjectOperations.cpp::replaceGeometries()`、B-spline 替换等路径会调用 `GeometryFacade::copyId(...)`，说明“修改同一条几何”时 id 应延续。

因此 CAD Core 的追溯合同也应以 geometry id 为主，不能把 `TopExp::MapShapes()` 的 `EdgeN` 顺序当成稳定身份。

## 核心合同

### 输入合同

Sketch `Geometry[]` 每个非构造 profile edge 应携带 FreeCAD geometry id：

```json
{
  "kind": "LineSegment",
  "id": 305,
  "start": [0, 0],
  "end": [10, 0]
}
```

兼容字段可接受 `Id` / `geometryId`，但进入 cad-core 内部后统一成 `geometryId`。没有 id 的旧 fixture 可临时生成 `index_fallback` 身份，例如 `index:0`，但该 fallback 只能用于测试兼容和诊断，不能标记为 stable。

前端责任：

- 新建 geometry 时生成并持久化 id。
- 修改同一条线段时保留 id。
- 删除后重建一条语义上不同的线段时必须给新 id。
- 不得靠旧 `EdgeN` 猜测新 `EdgeN`。

### 输出合同

`results[].subshapes[]` 中普通 Sketch edge 应发布：

```json
{
  "id": "Sketch:Edge1",
  "indexed": "Edge1",
  "subname": "Edge1",
  "stableSubname": "g305",
  "sourceGeometryId": 305,
  "sourceStableSubname": "g305",
  "identityStatus": "stable"
}
```

`results[].mesh.edgeSegments[]` 同步发布同一份身份信息：

```json
{
  "id": "Sketch:Edge1",
  "indexed": "Edge1",
  "stableSubname": "g305",
  "sourceGeometryId": 305,
  "points": [[0, 0, 0], [10, 0, 0]]
}
```

`id/indexed` 仍表示本次响应里实际可拾取的渲染边；`stableSubname/sourceStableSubname` 表示跨编辑追溯用的稳定身份。前端保存引用时应保存 `stableSubname = g305`，下一次 recompute 后用响应里的 stable identity 找到新的 `Sketch:EdgeN`。

## 模块落点

### 1. `sketcher`：源几何身份账本

新增一个小接口、深实现的模块，例如：

- `cad-core/include/cad_core/sketcher/sketch_edge_identity.h`
- `cad-core/src/sketcher/sketch_edge_identity.cpp`

建议核心类型：

```cpp
struct SketchGeometryIdentity {
    std::size_t geometryIndex;
    std::optional<long> geometryId;
    std::string stableSubname; // "g305"; fallback 时为 "" 或 "index:0" 诊断值
    std::string status;        // "stable" | "index_fallback" | "missing"
};

struct RawSketchEdgeIdentity {
    std::string indexed;       // "Edge1"
    SketchGeometryIdentity source;
};
```

外部 seam 保持小：

```cpp
RawSketchEdgeIdentityLedger buildRawSketchEdgeIdentityLedger(
    const TopoDS_Shape& rawShape,
    const std::vector<SketchProfileEdge>& profileEdges
);
```

实现内部可以使用 `TopoDS_Edge::IsSame()` 或构建时保留的 source edge 顺序，把 raw shape 中的 `EdgeN` 映射回 `SketchProfileEdge` 的 geometry identity。调用方不直接做几何猜测。

### 2. `sketch_object_geometry`：解析 geometry id

当前 `SketchSegment`、`SketchArc`、`SketchBSpline` 等只有 `geometryIndex`。需要增加可选 `geometryId` 字段，并在 `parseSketchGeometry()` 中读取：

- 优先读 `id`。
- 兼容读 `Id` / `geometryId`。
- id 必须是正整数；重复 id 产生 diagnostic。
- 旧数据无 id 时保留 `geometryIndex` fallback，但诊断/对象字段要暴露 fallback 数量。

### 3. `sketch_object_operations`：保留 profile edge 来源

当前 `profileEdges(...)` 会把不同几何类型统一成 `SketchProfileEdge`，但会丢掉原始 `geometryIndex`。应把 `geometryIndex/geometryId` 写入 `SketchProfileEdge`。

`makeProfileWiresFromEdges()` 已经有 `sourceEdges`、`openWireSourceEdgeIndices` 等账本，可在同一层增加 parallel identity ledger，确保：

- 线段方向反转时 identity 不变。
- open wire 重新组 wire 时 identity 跟随 source edge，而不是跟随 wire 内位置。
- 后续 `buildRawSketchShape()` 和 `buildOptionalProfileFace()` 使用同一份 profile edge identity。

### 4. `part` / `runtime`：响应注入身份，不改 edgeSegments 主枚举逻辑

`shape_exporter.cpp` 继续负责从 shape 生成 `edgeSegments`。不要在这里读取 Sketch DTO 或做 source matching。

建议在 `buildSketchInternalResult()` 或新增的 sketcher publication helper 中，把 identity ledger 合并到：

- `result.mesh["edgeSegments"]`
- `result.subshapes["EdgeN"]`

`runtime::responseMesh()` 和 `responseSubshapes()` 只做字段透传和对象前缀包装，不承载追溯推理。

### 5. 引用恢复

现有 `StableSubList` / `ReferenceShadow` 已有稳定引用语义。open wire raw edge 追溯应复用这个方向：

- 前端保存 `StableSubList: ["g305"]`，不是保存 `["Edge1"]`。
- cad-core 解析引用时，先用 source identity ledger 把 `g305` 解析到当前 raw shape 的 `EdgeN`。
- 找不到 `g305` 时，再进入已有 `ReferenceShadow` 恢复或返回 deleted/ambiguous diagnostic。

## 实施批次

### S1：只做可观测身份发布

目标：不改变引用解析，只让响应证明 `EdgeN -> g<ID>`。

改动：

- 解析 `Geometry[].id`。
- `SketchProfileEdge` 保留 `geometryId`。
- open wire raw edge response 发布 `sourceGeometryId/sourceStableSubname/identityStatus`。
- 保留无 id fixture 的 `index_fallback`，但不把它当 stable。

测试：

- open wire 三条线都有 id，断言 `Sketch:Edge1..3` 分别带 `g101..g103`。
- 修改第二条线段端点但保留 id，断言返回中仍存在 `sourceStableSubname == "g102"`，即使当前 `EdgeN` 可变化。
- 新增一条线段插到前面，断言原 `g102` 仍能找到对应 segment。

### S2：引用解析接入 `g<ID>`

目标：`StableSubList: ["g102"]` 能在当前请求解析到 raw `EdgeN`。

改动：

- 在 sketch raw shape 的 reference resolution 中新增 `g<ID> -> EdgeN` 查找。
- `results[].subshapes[].stableSubname` 对有 id 的普通 Sketch edge 发布 `g<ID>`。
- 保留短期兼容：请求里传 `EdgeN` 仍按当前请求枚举解析，但诊断中标明它不是稳定引用。

测试：

- 外部对象引用 open wire 的 `g102`，修改线段端点后仍解析成功。
- 插入新线段导致 `EdgeN` 顺序变化后，`g102` 仍解析到正确几何。
- 删除 `g102` 后返回 deleted/unsupported stable subname diagnostic，不悄悄落到其它 `EdgeN`。

### S3：ReferenceShadow 与前端合同收口

目标：追溯失败时有明确恢复路径，前端不需要猜。

改动：

- `ReferenceShadow` 记录 `stableSubname: "g<ID>"`、旧 edge snapshot、旧 geometry kind。
- 若当前 geometry id 存在但几何类型不兼容，返回 `ambiguous` 或 `geometry_kind_changed` diagnostic。
- 若 id 不存在但 shadow 可几何恢复，返回更新建议；否则返回 deleted。

测试：

- 修改端点：不需要 shadow，直接 id 命中。
- 删除后重建新 id：旧 `g<ID>` 不应误命中新线。
- id 存在但从 LineSegment 改成 Arc：按兼容规则处理，不能靠 bbox 猜。

## 验收命令

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_returns_open_sketch_raw_edge_segments
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_open_sketch_keeps_raw_shape_without_profile_face
git diff --check
```

追溯语义新增后应增加 focused tests，建议命名：

```bash
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_open_wire_edge_identity_survives_segment_edit
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_open_wire_edge_identity_survives_insert_before
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_open_wire_stable_sublist_resolves_geometry_id
```

阶段回归：

```bash
python3 -m unittest tests.test_adapters.CadCoreAdapterTest
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest
```

## 禁止事项

- 不用 fixture 名称、bbox、点坐标相等、边长度或几何类型排序反推身份。
- 不在前端保存旧 `EdgeN` 后猜新 `EdgeN`。
- 不在 adapter 层补 `EdgeN` 重映射。
- 不为了 open wire 可追溯伪造 `InternalShape` 或 `InternalEdgeN`。
- 不把无 id 的 `index_fallback` 宣称为 stable。

## 完成定义

完成不是“响应里多一个字段”，而是以下事实都成立：

1. 每个可追溯 open wire edge 都能从 `Sketch:EdgeN` 回到 `g<GeometryId>`。
2. 修改线段参数后，`g<GeometryId>` 仍能解析到当前请求里的正确 raw `EdgeN`。
3. 插入、删除其它线段导致 `EdgeN` 改号时，引用仍通过 `g<GeometryId>` 恢复。
4. 删除目标 geometry 后不会误命中新边，而是给出 deleted/ambiguous 诊断或走 `ReferenceShadow`。
5. `InternalShape` 为空的 FreeCAD parity 保持不变，open-only sketch 不产生 synthetic `Internal*`。

## 当前实现状态（7-1 S3 收口）

- 已完成 S1/S2 后端闭环：Sketch `Geometry[]` 解析 `id` / `Id` / `geometryId` 为正整数，重复 id 返回 `duplicate_geometry_id`；普通 profile edge 保留 `geometryIndex + geometryId`，open-only raw `EdgeN` 通过 sketcher 层 identity ledger 发布 `sourceGeometryId/sourceStableSubname/identityStatus`。
- `results[].subshapes[]` 与 `mesh.edgeSegments[]` 对有 geometry id 的 open wire edge 发布 `stableSubname = g<ID>`、`sourceGeometryId`、`sourceGeometryKind`、`sourceStableSubname`；无 id 旧 fixture 只发布 `identityStatus = index_fallback`，响应 `stableSubname` 保持空，不把请求内 `EdgeN` 宣称为稳定引用。
- `Sketch` raw `NamedShape` 注册 `g<ID> -> 当前 EdgeN`，现有 `App::Link` / `PropertyXLink` 的 `StableSubList=["g<ID>"]` 可解析到当前 raw Edge；插入新线导致 `EdgeN` 改号时，`g102` 仍解析到对应线段。
- S3 的 raw Sketch `Shape` open-wire Edge `ReferenceShadow` 已接入同一份 source identity：`PropertyLinkSubList` / `ExternalGeometry` 使用 `StableSubList=["g<ID>"]` 时，当前 id 命中会刷新 `SubList`、`ShadowSub`、`ReferenceShadow` 和 `elementReferenceUpdates`，并保留 `sourceGeometryId/sourceGeometryKind/sourceStableSubname`。
- 修改端点但保留 geometry id 时，不再用旧 `ReferenceShadow` fingerprint 判定语义漂移；以 `g<ID>` 为权威身份刷新当前 EdgeN 和 shadow。若同一 `g<ID>` 的 `sourceGeometryKind` 从 `LineSegment` 变为 `ArcOfCircle` 等不兼容类型，返回 `geometry_kind_changed` 诊断。
- 删除目标 geometry id 后，`g<ID>` 不会落到其它当前 EdgeN；带 `ReferenceShadow` 的 open-wire LinkSubList 路径返回 `deleted_stable_subname`，无 shadow 的既有 `App::Link` 路径保持 `unsupported_stable_subname`。删除后用相同几何重建新 id 不会自动恢复旧 `g<ID>`。
- S3 本身已收口，但阶段回归仍有独立 blocker：`tests.test_adapters.CadCoreAdapterTest.test_c_api_body_direct_tip_subshapes_publish_tip_qualified_stable_names` 当前失败，`Body.Face5 stableSubname` 实际为 `Pad.Face5`，测试期望 `Sketch.Face1`。复核 `rect-pad` live NamedShape 后，当前 Pad/Body 仅有 `Sketch.Edge1..4 -> Face1..4`，没有 `Sketch.Face1 -> Face5` 或 `Sketch.Edge1 -> Edge3` 的 profile source 映射；该问题属于 PartDesign Body/Pad `makeElementPrism` + RefineModel topo history 传播缺口，不属于 open-wire S3，本文件暂不改名为 `【已实现】`。

本轮 focused 验收覆盖：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_returns_open_sketch_raw_edge_segments
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_open_sketch_keeps_raw_shape_without_profile_face
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_open_wire_edge_identity_publishes_geometry_ids
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_open_wire_edge_identity_survives_segment_edit
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_open_wire_edge_identity_survives_insert_before
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_open_wire_stable_sublist_resolves_geometry_id
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_open_wire_deleted_geometry_id_does_not_hit_new_edge
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_open_wire_reference_shadow_refreshes_geometry_id_update
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_open_wire_reference_shadow_refreshes_after_segment_edit
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_open_wire_reference_shadow_reports_geometry_kind_drift
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_open_wire_reference_shadow_deleted_geometry_id_does_not_hit_new_edge
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_open_wire_reference_shadow_deleted_geometry_id_same_shape_new_id_does_not_rebind
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_sketch_rejects_duplicate_geometry_id
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_open_wire_reference_shadow_source_identity_contract
git diff --check
```

阶段回归结果：

```bash
python3 -m unittest tests.test_adapters.CadCoreAdapterTest
# 当前仅剩 test_c_api_body_direct_tip_subshapes_publish_tip_qualified_stable_names 失败：
# AssertionError: 'Pad.Face5' != 'Sketch.Face1'
```
