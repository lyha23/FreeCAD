# C12-M15 S1 FreeCAD source 与 current identity 管线复核

## 目标

复核 FreeCAD sketch geometry id / mapped-name 语义和 cad-core 当前 `sketch_edge_identity` / response / reference resolution 管线，确认 S2 ledger interface 的 source authority。

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
