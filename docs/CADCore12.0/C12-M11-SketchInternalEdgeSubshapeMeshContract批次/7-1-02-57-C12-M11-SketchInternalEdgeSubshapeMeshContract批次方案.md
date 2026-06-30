# C12-M11 Sketch Internal Edge Subshape / Mesh Contract 批次方案

## 目标

C12-M11 要把草图边的后端响应契约说清楚并落成可验收实现路径：

- `mesh.edgeSegments[]` 必须稳定给出草图可显示 / 可拾取边段。
- `subshapes[]` 必须给出同名 `InternalEdgeN` / `EdgeN` token。
- `stableSubname` 必须由后端 topology naming / internal element map / geometry id history 生成，不能由前端猜。
- `mesh.edgeSegments[].indexed` 与 `subshapes[].indexed` 必须一一对齐。

这解决的是草图提交后边引用保存失败的问题；面能保留下来通常是因为 `InternalFaceN` 已被 profile / pad 路径消费，而边缺少同等明确的 response channel 或前端没有消费该 channel。

## FreeCAD 调用链

FreeCAD 的关键流程是：

1. `SketchObject::execute()` 求解后调用 `buildShape()`。
2. `buildShape()` 遍历 solved geometry，非 construction 曲线生成 raw `EdgeN`，点生成 raw `VertexN`。
3. `getEdge()` 把 `Edge1` 映射为 sketch geometry mapped name，并给端点写 `VertexN` mapped name。
4. `makeElementWires(..., Part::OpCodes::Sketch)` 生成 public `Shape`。
5. `buildInternals()` 基于 public `Shape` 的 wires 生成 `InternalShape`：闭合区域进 `FaceMakerBuildFace`，open wire 通过 `WireJoiner::getOpenWires()` 保留。
6. `getInternalElementMap()` 只对 internal `Vertex` / `Edge` 找回 raw `VertexN` / `EdgeN`，生成双向 `InternalEdgeN <-> EdgeN` 映射。
7. `updateGeoHistory()` / `generateId()` 负责更高层的 geometry id 复用，让 mapped name 不只是依赖当前数组序号。

## CAD Core 落点

当前 `cad-core` 已有主要落点，本包要复核并收敛：

- `cad-core/src/sketcher/sketch_internal_result.cpp`：决定 Sketch response 使用 raw shape 还是 internal shape；internal shape 应调用 `meshForShape(*internalShape, "InternalFace", "InternalEdge", "InternalVertex")`。
- `cad-core/src/part/shape_exporter.cpp`：`edgeSegmentsForShape()` 必须使用 `TopExp::MapShapes(shape, TopAbs_EDGE, edges)`，不得从三角网格反推边。
- `cad-core/src/part/property_topo_shape.cpp`：`subshapeMapForShape(shape, "Internal")` 必须和 mesh edgeSegments 使用同一 topology shape 与 prefix。
- `cad-core/src/app/element_map.cpp`：`internalElementMapForSketch()` 负责 request-local `InternalEdgeN <-> EdgeN`。
- `cad-core/src/part/topo_shape.cpp`：`namedShapeForSketchInternalShape()` 应发布 `InternalFaceN` / `InternalEdgeN` / `InternalVertexN`，并把 internal edge/vertex alias retag 到 raw edge/vertex。
- `cad-core/src/runtime/recompute.cpp`：`responseMesh()` / `responseSubshapes()` 是最终 HTTP/adapter response contract，必须保证 object-qualified id 和 stableSubname 对齐。

## 契约草案

对一个草图对象 `Sketch`，后端 response 至少应满足：

```json
{
  "mesh": {
    "edgeSegments": [
      {
        "id": "Sketch:InternalEdge1",
        "indexed": "InternalEdge1",
        "points": [[0, 0, 0], [10, 0, 0]]
      }
    ]
  },
  "subshapes": [
    {
      "id": "Sketch:InternalEdge1",
      "kind": "edge",
      "indexed": "InternalEdge1",
      "subname": "InternalEdge1",
      "stableSubname": "Edge1"
    }
  ]
}
```

其中：

- `id` 是 response 层 object-qualified token，用于拾取命中与前端选择对象绑定。
- `indexed` 是当前对象内部 topology index，`edgeSegments` 与 `subshapes` 必须一致。
- `subname` 是本次可请求的当前 subname。
- `stableSubname` 是后端给出的稳定引用写回依据；最低层可为 raw `EdgeN`，FreeCAD-grade 完整稳定应进一步升级为 geometry id mapped name。

## 分阶段步骤

- 入口：确认 C12-M11 包结构、队列和矩阵可读。
- S0：冻结 C12-M11 live baseline，并明确 C12-M10 pending 不影响本包作为用户单独方案存在。
- S1：复核 FreeCAD source authority 和 current `cad-core` response source，补齐 source matrix。
- S2：跑或复核 focused current response，证明 `edgeSegments` / `subshapes` 是否已对齐。
- S3：裁决后端契约缺口：current-supported、backend implementation required、frontend consumer required 或 stable geometry id follow-up。
- S4：若需要后端实现，定义最小完整语义批次和代码落点；若无需后端实现，定义前端同步边界。
- S5：发布闸门，更新 README、矩阵和后续分流。

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M11-SketchInternalEdgeSubshapeMeshContract批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M11-SketchInternalEdgeSubshapeMeshContract批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M11-SketchInternalEdgeSubshapeMeshContract批次 docs/CADCore12.0/README.md
git diff --check
```

Focused response contract：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_returns_sketch_internal_profile_mesh
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_mesh_edge_segments_reference_result_subshapes
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_sketch_exports_internal_edge_vertex_stable_subnames
```

阶段回归只在 S4/S5 决定修改 production code 后执行；开包方案本身不跑 full build 或 full FreeCAD CI。

## 非目标

- 不解决完整 Topological Naming。
- 不把 `InternalFaceN` 稳定性和 `InternalEdgeN` 稳定性混为一个问题。
- 不新增前端兼容分支或旧字段映射。
- 不在 adapter 层伪造 edge token。
- 不保存 BREP / TopoDS / persistent `NamedShape` / persistent `ElementMap`。
