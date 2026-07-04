# 【已实现】C12-M15 S1 FreeCAD source 与 current identity 管线复核

## 目标

复核 FreeCAD sketch geometry id / mapped-name 语义和 cad-core 当前 `sketch_edge_identity` / response / reference resolution 管线，确认 S2 ledger interface 的 source authority。

## live 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD`：`5a5464fb96`。
- `git log -1 --oneline`：`5a5464fb96 文档：关闭 C12-M15 S0 基线冻结`。
- `git -c core.quotepath=false status --short -uall`：无输出。

父进程给出的 `HEAD=5a5464fb96` 与本地命令一致；本步起点 worktree clean。

## 必读文件

- `../README.md`
- `../7-4-15-44-C12-M15-SketchStableGeometryIdMappedNameLedger设计批次方案.md`
- `../矩阵/c12m15_sketch_geometry_id_source_matrix.tsv`
- `../矩阵/c12m15_sketch_geometry_id_scope_matrix.tsv`
- `../矩阵/c12m15_sketch_geometry_id_contract_matrix.tsv`
- `src/Mod/Sketcher/App/SketchObject.cpp`
- `src/Mod/Sketcher/App/SketchObjectGeometry.cpp`
- `src/Mod/Sketcher/App/GeoEnum.h`
- `cad-core/include/cad_core/sketcher/sketch_edge_identity.h`
- `cad-core/src/sketcher/sketch_edge_identity.cpp`
- `cad-core/src/sketcher/sketch_object_geometry.cpp`
- `cad-core/src/sketcher/sketch_object_operations.cpp`
- `cad-core/src/runtime/recompute.cpp`
- `cad-core/src/runtime/reference_resolution.cpp`

## 操作

1. 复核 FreeCAD `updateGeoHistory()`、`generateId()`、`convertSubName()`、`getEdge()` 的调用含义和关键字段。
2. 复核 cad-core 当前如何读取 `id` / `Id` / `geometryId`，如何发布 `sourceGeometryId`、`sourceStableSubname`、`identityStatus`。
3. 复核 reference resolution 是否已经使用 `rawSketchEdgeIdentity.byStableSubname`。
4. 更新 source / scope / contract / blocker / validation 矩阵。
5. 将本步骤重命名为 `【已实现】`。

## FreeCAD source 结论

- `GeoEnum.h` 明确 `GeoId` 是 sketcher 几何列表索引：`GeoId >= 0` 是普通 geometry，负值保留给轴 / root point / external geometry；它不是 geometry extension id，也不足以表达一个 vertex / edge element，元素身份还要配合 `PointPos`。
- `SketchObjectGeometry.cpp::addGeometry()` 在写入 `Geometry` 列表前调用 `generateId()`；`setGeometryId()` / `getGeometryId()` 读写的是 `GeometryFacade` 上的 extension id。
- `SketchObject::updateGeoHistory()` 遍历 internal geometry，用 `GeometryFacade::getId()` 把起点记录为 `id`、终点记录为 `-id`，再 `finishUpdate(geoMap)`。这是 FreeCAD deleted geometry history 的 id 复用证据，但它依赖 FreeCAD document 内的 sketch state，不等于 CAD Core 可以保存 backend session。
- `SketchObject::generateId()` 在 `geoHistoryLevel=0` 时分配 `++geoLastId`；否则先确保 `geoHistory`，按 start/end endpoint 查找 deleted geometry id，优先两端都匹配的旧 id，跳过已在 `geoMap` 中占用的 id，找不到再分配新 id。
- `SketchObject::checkSubName()` 能把 mapped `g<ID>` / `e<ID>` 解析回当前 `geoMap` / `externalGeoMap` 中的 `GeoId`，并确认当前 geometry 的 `GeometryFacade::getId()` 仍等于该 id；失败时不猜测当前 `EdgeN`。
- `SketchObject::convertSubName()` 对普通 geometry 输出 `g<ID>`，对 external geometry 输出 `e<ID>`，vertex 追加 `v<PointPos>`；内部元素先走 `InternalShape.getShape().getMappedName()`。
- `SketchObject::getEdge()` 从当前 geometry 生成 `TopoShape`，把 shape 内 `Edge1` 命名为传入 mapped name，并用 `namev<PointPos>` 给顶点建立 mapped name。最终 `getSubObject()` 仍在当前请求/当前 document state 内拿实际 shape。

## cad-core current 结论

- `sketch_object_geometry.cpp::readGeometryIdField()` 当前按 `id`、`Id`、`geometryId` 顺序读取 geometry extension id，要求正整数；非法值报 `invalid_geometry_id`，重复值报 `duplicate_geometry_id`。
- `sketch_object_operations.cpp` 把解析到的 `geometryIndex`、`geometryId`、`geometryKind` 写入 `SketchGeometryIdentity`，再随 source edge 进入 raw shape / profile wire 构建。
- `sketch_edge_identity.h` 已有候选 interface：`SketchGeometryIdentity`、`RawSketchEdgeIdentity`、`RawSketchEdgeIdentityLedger`。它目前表达的是 raw sketch edge identity ledger，不是完整 `updateGeoHistory()` 式 id 复用器。
- `sketch_edge_identity.cpp::stableSubnameForGeometryId()` 输出 `g<ID>`；有 geometry id 时 `identityStatus=stable`，无 id 时 `identityStatus=index_fallback` 且 `sourceStableSubname=index:N`。`applyIdentityFields()` 只在 stable 时发布 durable `stableSubname`。
- `buildRawSketchEdgeIdentityLedger()` 先用 `TopoDS_Edge::IsSame()` 把 raw edge 匹配回 source edge；`sourceOrderMatchesPublishedShape` 只在明确允许时用 source order fallback。split / 一对多 fragment 尚未设计成稳定延续。
- `sketch_internal_result.cpp` 把 `raw_edge_identity` 放到 object fields，并把同一 ledger 发布到 mesh edgeSegments 与 subshapes。
- `runtime/recompute.cpp::responseMesh()` 透传 `stableSubname`、`sourceStableSubname`、`sourceGeometryKind`、`identityStatus`、`sourceGeometryId`、`sourceGeometryIndex`；`responseSubshapes()` 在 `identityStatus=stable` 时用 `sourceStableSubname` 覆盖对外 stableSubname，在 `index_fallback` 时清空 durable stableSubname。
- `runtime/recompute.cpp::normalizedInternalEdgeStableSubname()` 能通过 `internal_element_map` 从 `InternalEdgeN` 找 raw `EdgeN`，再查 `raw_edge_identity.byIndexed` 得到 `g<ID>`；这覆盖已存在的 closed/mixed internal profile alias，但不等于 full WireJoiner/ElementMap history 设计已完成。
- `runtime/reference_resolution.cpp` 先从 `StableSubList` / `ReferenceShadow.stableSubname` / `ReferenceShadow.sourceStableSubname` 取 `g<ID>`，再查 `raw_edge_identity.byStableSubname` 回到当前 indexed edge。找不到时报 `deleted_stable_subname`，kind 变化时报 `geometry_kind_changed`，解析成功后把 `sourceGeometryId`、`sourceGeometryKind`、`sourceStableSubname` 写回 reference shadow update。

## S2 handoff

- Source authority 已清晰：FreeCAD stable identity 候选是 `GeometryFacade` extension id 加 mapped `g<ID>` / `e<ID>`，不是 `GeoId` 列表索引，也不是当前 `EdgeN`。
- Current landing 已清晰：cad-core 当前已有 input id parser、raw edge identity ledger、response field publisher、internal edge alias normalization 和 reference resolution consumer。
- 仍需 S2 设计：`SketchGeometryIdentityLedger` 是否沿用 / 包装当前 `RawSketchEdgeIdentityLedger`；`index_fallback` 的对外字段规则；missing / invalid / duplicate / deleted / kind drift 的统一 diagnostic contract；split one-source-to-many-current fragments 是否 out-of-scope、diagnostic 还是后续 implementation batch。
- 仍需 S3 裁决：当前代码已有多项 focused tests 约束 raw open-wire reorder / insert / delete / kind drift，但本步没有运行测试，也不把 current coverage 宣称为完整设计闭环。

## 本步改动边界

- 更新 S1 文档、C12-M15 README / 总入口 / root README 当前状态，以及 source / scope / contract / blocker / validation 矩阵。
- 未修改 C++、fixtures、expected、tests、adapters 或 capability source。
- 未运行 FreeCADCmd、build 或重型回归。

## 关闭条件

- FreeCAD source authority 已写入 source matrix。
- cad-core current landing 已写入 scope matrix。
- S2 ledger interface 的输入、输出和 fallback 需要均可定位。

## 非目标

- 不修改 C++。
- 不新增 fixture。
- 不把 current coverage 直接宣称为完整设计闭环。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'updateGeoHistory|generateId|convertSubName|getEdge|GeometryFacade::getId|GeoId' src/Mod/Sketcher/App/SketchObject.cpp src/Mod/Sketcher/App/SketchObjectGeometry.cpp src/Mod/Sketcher/App/GeoEnum.h
rg -n 'SketchGeometryIdentity|RawSketchEdgeIdentity|stableSubnameForGeometryId|sourceGeometryId|sourceStableSubname|identityStatus|byStableSubname|duplicate_geometry_id|invalid_geometry_id' cad-core/include/cad_core/sketcher cad-core/src/sketcher cad-core/src/runtime/recompute.cpp cad-core/src/runtime/reference_resolution.cpp
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次/矩阵/*.tsv
git diff --check
```
