# C12-M15 Sketch stable geometry id / mapped-name ledger 设计批次

C12-M15 承接 C12-M11 S4/S5 发布的 `C12-M11-StableGeometryIdMappedNameLedger设计批次` follow-up。C12-M11 已确认 closed internal profile 的 `InternalEdgeN` edgeSegments / subshapes / request-local `stableSubname=EdgeN` 当前支持；C12-M15 不重开 closed profile response contract，而是专门设计 FreeCAD-grade sketch geometry id / mapped-name 账本。

本包回答一个问题：当前请求里发布的 `EdgeN` / `InternalEdgeN` 应该如何映射到 sketch geometry 的稳定身份，使后续引用更新、前端选择持久化和 `StableSubList` 不再依赖 `EdgeN` 顺序。

## 当前基线

- 创建基线：`pwd=/Users/li/Chili3DProject/FreeCAD`。
- 创建基线：`HEAD=dff911d299`（`dff911d299 实现 SubShapeBinder CopyOnChange request-local 支持`）。
- 创建前 `git -c core.quotepath=false status --short -uall` 无输出。
- 本轮只新增 `docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次/` 并更新 `docs/CADCore12.0/README.md`。
- C12-M14 `工作步骤细分` 队列为空；live capability 递归检查到的 `remaining_gaps` 均为空，因此 C12-M15 不是 capability remaining gap，而是 C12-M11 明确拆出的 stable-id 设计 follow-up。
- S0 live 冻结：`HEAD=b3d2df945a`（`b3d2df945a docs: 关闭 C12-M15 工作步骤总入口`），`pwd=/Users/li/Chili3DProject/FreeCAD`，起点 `git -c core.quotepath=false status --short -uall` 无输出。
- S0 dirty boundary：docs=`<none>`；`cad-core/src`=`<none>`；tests=`<none>`；fixtures / expected=`<none>`；adapters=`<none>`；other=`<none>`。非本包 dirty 未发现。
- S0 队列冻结：C12-M11 与 C12-M14 `工作步骤细分` 队列均只输出表头；C12-M15 队列从 S0 开始，S0 关闭后下一步进入 S1 FreeCAD source 与 current identity 管线复核。
- S0 capability gap 递归检查：所有 live `remaining_gaps` 均为 `[]`，root `known_gaps=[]`，`part_design.sub_shape_binder.known_gaps={}`；C12-M15 是 C12-M11 stable geometry id ledger follow-up，不是 capability remaining gap / known gap，也不是 C12-M14 helper lifecycle 后续。

## 问题定义

`Edge1` / `Edge2` 是当前 shape 枚举出来的座位号，不是长期身份证。草图编辑后插入、删除、拆分、重排几何时，同一个 `Edge2` 名字可能指向另一条边。

FreeCAD 的更稳定路径是：

1. `SketchObject::generateId()` 给 sketch geometry 分配或复用 `GeometryFacade::getId()`。
2. `SketchObject::updateGeoHistory()` 记录几何端点到旧 id 的关系，后续重建时尽量复用 id。
3. `SketchObject::convertSubName()` 把 `EdgeN` 转为 `g<ID>` / `e<ID>` 这类 mapped name。
4. `SketchObject::getEdge()` 仍能从 mapped name 回到当前实际 edge shape。

CAD Core 当前已有 `geometryId`、`sourceGeometryId`、`sourceStableSubname`、`identityStatus`、`rawSketchEdgeIdentity` 和 reference resolution 管线，但还缺一份明确产品契约：哪些场景可以称为 stable，哪些只能降级为 `index_fallback`，以及未来实现应把账本的 seam 放在哪里。

## 设计目标

- 定义一个深模块：`SketchGeometryIdentityLedger`。它的 interface 只暴露“当前 indexed edge 到 source geometry identity 的映射”和“稳定名解析结果”；内部隐藏 TopExp 枚举、raw/internal edge 关联、fallback 规则和 diagnostics。
- 把 `geometryId -> stableSubname(g<ID>) -> current EdgeN/InternalEdgeN` 的账本设计成 request-local 输出，不引入 persistent backend document、TopoDS cache、NamedShape cache 或 BREP cache。
- 明确 response contract：`mesh.edgeSegments[]`、`subshapes[]`、`rawSketchEdgeIdentity`、`elementReferenceUpdates` 必须使用同一套 identity。
- 明确前端边界：前端只消费后端返回的 `id`、`subname`、`stableSubname`、`sourceGeometryId`、`sourceStableSubname`、`identityStatus`，不得靠 prefix guessing 或 mesh 顺序发明长期拓扑身份。

## FreeCAD source authority

| 语义 | FreeCAD source | C12-M15 用法 |
| --- | --- | --- |
| geometry id 分配 / 复用 | `src/Mod/Sketcher/App/SketchObject.cpp::generateId()` | 解释为什么 `EdgeN` 不是稳定身份，稳定身份来自 `GeometryFacade::getId()`。 |
| deleted geometry history | `src/Mod/Sketcher/App/SketchObject.cpp::updateGeoHistory()` | 解释按端点匹配旧 geometry id 的 FreeCAD-grade 复用策略。 |
| indexed name 到 mapped name | `src/Mod/Sketcher/App/SketchObject.cpp::convertSubName()` | 解释 `EdgeN` 如何转为 `g<ID>` / `e<ID>` mapped name。 |
| mapped/current edge 获取 | `src/Mod/Sketcher/App/SketchObject.cpp::getEdge()` 与 `src/Mod/Sketcher/App/SketchObjectGeometry.cpp` | 解释最终仍需解析到当前请求内的实际 edge shape。 |
| GeoId 概念 | `src/Mod/Sketcher/App/GeoEnum.h` | 区分 FreeCAD 的列表索引 `GeoId` 与 geometry extension id。 |

## cad-core 落点

| 落点 | 角色 |
| --- | --- |
| `cad-core/include/cad_core/sketcher/sketch_edge_identity.h` | 当前 `SketchGeometryIdentity` / `RawSketchEdgeIdentityLedger` interface 候选。 |
| `cad-core/src/sketcher/sketch_edge_identity.cpp` | 当前 `stableSubnameForGeometryId()`、raw edge ledger、response identity fields 的实现候选。 |
| `cad-core/src/sketcher/sketch_object_geometry.cpp` | 当前输入 `id` / `Id` / `geometryId` 读取与 duplicate/invalid diagnostics。 |
| `cad-core/src/sketcher/sketch_object_operations.cpp` | 当前 geometry source identity 传入 raw/internal edge 构建路径。 |
| `cad-core/src/runtime/recompute.cpp` | 当前 response `edgeSegments[]` / `subshapes[]` 发布 identity 字段的出口。 |
| `cad-core/src/runtime/reference_resolution.cpp` | 当前按 `sourceStableSubname` / `sourceGeometryId` 做 reference resolution 的消费端。 |
| `cad-core/tests/test_p5_features.py` 或相关 sketch focused tests | 后续最小实现验证：重排/插入/删除后仍按 `g<ID>` 找回当前 edge。 |

## 预期出口

1. `design_published_implementation_ready`：source/current 复核证明 seam 已清晰，S2/S3 发布账本 interface 与最小 implementation 批次。
2. `design_published_no_code_current_sufficient`：当前实现已经满足 C12-M11 follow-up，只需补文档和 capability wording。
3. `design_blocked_need_oracle_or_product_decision`：FreeCAD source/current 仍不足以裁决 stable-id 语义，保留 blocker。

## 非目标

- 不重开 C12-M11 closed internal profile response contract。
- 不把 request-local `EdgeN` 顺序说成 FreeCAD-grade stable id。
- 不修改 `my-chili3d` 前端消费；前端同步属于 `my-chili3d-C12M11-SketchEdgeTokenConsumerSync批次`。
- 不裁决 open wire raw edge mesh 产品契约；它属于 `C12-M11-OpenWireRawEdgeMeshProductContract裁决批次`。
- 不引入 persistent backend sketch session、TopoDS cache、NamedShape cache、ElementMap cache 或完整 BREP cache。
- 不用 mesh triangle / polyline 顺序倒推 topology identity。

## 入口

- 总入口：`7-4-15-44-C12-M15-SketchStableGeometryIdMappedNameLedger设计批次总入口.md`
- 方案：`7-4-15-44-C12-M15-SketchStableGeometryIdMappedNameLedger设计批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次 docs/CADCore12.0/README.md
git diff --check
```
