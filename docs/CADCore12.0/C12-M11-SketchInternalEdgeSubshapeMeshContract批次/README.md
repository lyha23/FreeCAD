# C12-M11 Sketch Internal Edge Subshape / Mesh Contract 批次

C12-M11 是用户单独打开的并行方案包，用来解决草图提交后“面能保留，边保留不下来”的后端契约问题。它不声明 C12-M10 已关闭，也不把当前 pending 的 CopyOnChange 队列当作已完成事实；执行 C12-M11 前需要由用户明确选择暂停、绕过或关闭 C12-M10。

本包目标是把 FreeCAD `SketchObject` 的 `Shape` / `InternalShape` / `InternalEdgeN` 语义转成 CAD Core 后端可稳定返回的 `subshapes[]` 与 `mesh.edgeSegments[]` 契约：同一条草图边必须在 response 中同时有可拾取显示段和可持久化引用 token，前端不得从三角网格或绘制线段猜 `EdgeN` / `InternalEdgeN`。

## 当前基线

- 创建基线：`pwd=/Users/li/Chili3DProject/FreeCAD`。
- 创建基线：`HEAD=3662c8ff81`（`3662c8ff81 文档：新增 C12-M10 CopyOnChange oracle 解锁包`）。
- 创建时 C12-M10 队列仍 pending；C12-M11 是用户点名主题的并行方案包。
- 当前 `cad-core` 已有 `results[].mesh.edgeSegments`、`subshapes[]`、`InternalEdgeN` / `InternalVertexN` 和 sketch `internal_element_map` 的基础管线，但本包要把它收敛成可验收产品契约并补齐 FreeCAD 级稳定性边界。
- 工作步骤总入口已关闭：`工作步骤细分/7-1-02-58-【已实现】C12-M11工作步骤总入口.md` 已核对包结构、入口 + S0-S5 队列顺序和 6 个 TSV 字段数。
- S0 live 基线与并行开包冻结已关闭：执行基线为 `pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=fdea7997eb`（`fdea7997eb 文档：关闭 C12-M11 工作步骤总入口`），起点 dirty boundary 为 `<clean>`。C12-M10 队列仍 pending（工作步骤总入口、S0-S6），C12-M11 是用户单独点名的并行主题，不继承为 C12-M10 后的自然下一包，也不修改 C12-M10 队列结论。`C12M11-BLOCKER-001/002` 与 `C12M11-VAL-002` 已关闭；本步未运行 focused tests，未做 backend/frontend 缺口裁决，未改 code、fixtures、expected、tests、adapters。S1 已关闭为 `7-1-03-00-【已实现】C12-M11-S1-FreeCAD与cad-core-source复核.md`。
- S1 FreeCAD 与 `cad-core` source 复核已关闭：执行基线为 `pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=1abd1c7df5`（`1abd1c7df5 文档：冻结 C12-M11 S0 live 基线`），起点 dirty boundary 为 `<clean>`。已复核 FreeCAD `buildShape()` / `getEdge()` / `buildInternals()` / `getInternalElementMap()` / `updateGeoHistory()` / `generateId()` 与 `cad-core` `buildSketchInternalResult()` / `meshForShape()` / `edgeSegmentsForShape()` / `subshapeMapForShape()` / `internalElementMapForSketch()` / `namedShapeForSketchInternalShape()` / `responseMesh()` / `responseSubshapes()`；`C12M11-BLOCKER-101` 已关闭，`C12M11-VAL-101` 已记录。本步只更新 docs/矩阵，未运行 focused tests，未修改 C++、tests、fixtures、expected 或 adapters。
- S1 结论：closed internal profile 与 open wire profile 是不同 source 路径；open wire mesh/null 不可直接判为 closed profile `InternalEdge` 丢失。`edgeSegments` 与 `subshapes` 必须来自同一 `TopoDS_Shape` 与 `TopExp::MapShapes(..., TopAbs_EDGE, ...)` topology source。FreeCAD-grade geometry id stability 只记录为后续 S3/S4 follow-up 候选，不在 S1 裁决。
- S2 current response contract 复核已关闭：执行基线为 `pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=385937da37`（`385937da37 文档：关闭 C12-M11 S1 source 复核`），起点 dirty boundary 为 `<clean>`。focused tests `C12M11-VAL-201..203` 全部通过；`p5/sketch-internal-face` 当前 FFI response 返回 `Sketch:InternalEdge1..4` mesh `edgeSegments` 与同名 edge `subshapes`，且 request-local `stableSubname=Edge1..4`；`mvp/rect-pad` alignment evidence 为 12 条 edgeSegments / 12 条 Edge subshapes / mismatchCount=0。`p5/sketch-open-wire-internal-empty` 当前行为单独分类为 raw `Sketch:Edge1..3` subshapes 可见且 `mesh=null`，不混入 closed internal profile contract。`C12M11-BLOCKER-201` 已关闭。
- S3 contract gap 分流裁决已关闭：执行基线为 `pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=c3ddb3d7b4`（`c3ddb3d7b4 文档：关闭 C12-M11 S2 response contract 复核`），起点 dirty boundary 为 `<clean>`。closed internal edge backend response 裁为 `current_supported`，edgeSegments/subshapes alignment 裁为 `mismatch_absent`，request-local stableSubname 已 passed；FreeCAD-grade geometry id stability 分流为 `followup_required_or_deferred`。若前端仍丢边，责任边界为 `my-chili3d` consumer sync，禁止 frontend prefix guessing；open wire raw `EdgeN` mesh/null 单独分流为 `open_wire_product_contract_required_or_deferred`。`C12M11-BLOCKER-301` 已关闭，`C12M11-VAL-301` 已记录；S4 输入为前端同步、stable-id follow-up、open-wire 产品契约，不是 closed profile backend C++ 大改。
- S4 后续最小语义批次已关闭：执行基线为 `pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=1ff42cb52a`（`1ff42cb52a 文档：关闭 C12-M11 S3 contract gap 分流裁决`），起点 dirty boundary 为 `<clean>`。`C12M11-BLOCKER-401` 已关闭，`C12M11-VAL-401` 已记录；后续最小完整语义批次定义为 `my-chili3d-C12M11-SketchEdgeTokenConsumerSync批次`、`C12-M11-StableGeometryIdMappedNameLedger设计批次` 和 `C12-M11-OpenWireRawEdgeMeshProductContract裁决批次`。S5 输入是发布 current-supported closed backend contract 与三条 follow-up 分流，不重开 closed profile backend C++ implementation，不修改 C12-M10。

## 问题定义

草图边不能保留下来通常有三类原因：

1. 后端没有把草图 `InternalShape` 的真实 `TopoDS_Edge` 发布到 `mesh.edgeSegments[]`。
2. `mesh.edgeSegments[].indexed` 与 `subshapes[].indexed` 不来自同一套 `TopExp::MapShapes(..., TopAbs_EDGE, ...)` 枚举，导致拾取 token 对不上。
3. `InternalEdgeN` 只有显示名，没有稳定回写到 raw `EdgeN` 或 FreeCAD-style geometry id mapped name，导致重算或提交后引用丢失。

本包只接受后端拓扑源输出：`InternalEdgeN` / `EdgeN` 必须来自 OCCT topology enumeration、`NamedShape` / `ElementMap` 或 request-local history ledger；禁止从 mesh triangle adjacency、前端线段顺序、点坐标近似或 display object name 推断长期 topology identity。

## FreeCAD / CAD Core 依据

| 语义 | 依据 | C12-M11 用法 |
| --- | --- | --- |
| 草图 raw Shape 构建 | `src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::buildShape()` | 非 construction geometry 生成 raw `EdgeN`，随后 `makeElementWires(..., Part::OpCodes::Sketch)`。 |
| Edge / Vertex mapped name | `src/Mod/Sketcher/App/SketchObjectGeometry.cpp::SketchObject::getEdge()` | 每条草图 edge 写入 `ElementMap`，vertex 用 `name + v + pos` 绑定端点。 |
| 草图 InternalShape | `src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::buildInternals()` | `FaceMakerBuildFace` 生成内部面，`WireJoiner::getOpenWires()` 保留 open wire。 |
| InternalEdge 映射 | `src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::getInternalElementMap()` | 只映射 `InternalVertexN` / `InternalEdgeN` 到 raw `VertexN` / `EdgeN`；`InternalFaceN` 不走这个简单映射。 |
| FreeCAD stable geometry id | `src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::updateGeoHistory()` 与 `generateId()` | 完整稳定命名需要 geometry id 复用，不只是当前请求中的 `EdgeN` 顺序。 |
| 当前 edgeSegments 生成 | `cad-core/src/part/shape_exporter.cpp::edgeSegmentsForShape()` | `TopExp::MapShapes(shape, TopAbs_EDGE, edges)` 是唯一允许的 edge segment 枚举源。 |
| 当前 sketch internal 发布 | `cad-core/src/sketcher/sketch_internal_result.cpp::buildSketchInternalResult()` | 对 internal shape 调用 `meshForShape(..., "InternalFace", "InternalEdge", "InternalVertex")` 并合并 internal subshapes。 |
| 当前 response 契约 | `cad-core/src/runtime/recompute.cpp::responseMesh()` / `responseSubshapes()` | response 层给 `edgeSegments.id` / `subshapes.id` 加对象名前缀，并为 internal edge 补 `stableSubname`。 |

## 解锁目标

C12-M11 的最终发布状态应是以下之一：

1. `contract_current_supported`：当前 `cad-core` 已经稳定返回 `InternalEdgeN` edgeSegments/subshapes，缺口只在前端消费或运行态发布。
2. `implementation_package_authorized`：发现当前后端实际缺失 response contract，需要打开最小完整实现包。
3. `stable_geometry_id_followup_required`：request-local `InternalEdgeN -> EdgeN` 已成立，但 FreeCAD-grade 跨编辑稳定性仍缺 geometry id / history ledger；S1 只把该项记录为后续 S3/S4 follow-up 候选，不提前裁决。
4. `blocked_by_missing_oracle`：缺少能证明 FreeCAD 行为的 expected / fixture / oracle，先补采集。

## 入口

- 总入口：`7-1-02-57-C12-M11-SketchInternalEdgeSubshapeMeshContract批次总入口.md`
- 方案：`7-1-02-57-C12-M11-SketchInternalEdgeSubshapeMeshContract批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 非目标

- 不从前端 mesh triangle、线段绘制顺序或点坐标猜 `EdgeN` / `InternalEdgeN`。
- 不把 `InternalFaceN` 简化成 raw face stable alias；FreeCAD 的 internal face 需要 FaceMaker / WireJoiner history 支撑。
- 不引入 persistent `NamedShape` / `ElementMap` / TopoDS / BREP cache。
- 不修改 C12-M10 CopyOnChange 队列结论。
- 不把当前请求枚举稳定误写成 FreeCAD 级跨编辑稳定；两者必须分层验收。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M11-SketchInternalEdgeSubshapeMeshContract批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M11-SketchInternalEdgeSubshapeMeshContract批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M11-SketchInternalEdgeSubshapeMeshContract批次 docs/CADCore12.0/README.md
git diff --check
```
