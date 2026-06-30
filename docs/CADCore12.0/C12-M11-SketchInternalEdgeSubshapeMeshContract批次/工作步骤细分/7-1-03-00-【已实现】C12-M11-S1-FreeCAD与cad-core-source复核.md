# C12-M11 S1 【已实现】FreeCAD 与 cad-core source 复核

## 目标

复核 FreeCAD sketch edge / internal edge 命名调用链，并把当前 `cad-core` 中对应落点记录到 source matrix。

## 必读文件

- `../README.md`
- `../7-1-02-57-C12-M11-SketchInternalEdgeSubshapeMeshContract批次方案.md`
- `../矩阵/c12m11_sketch_edge_source_candidates.tsv`
- `../矩阵/c12m11_sketch_edge_contract_matrix.tsv`

## 操作

1. 复核 FreeCAD source：`SketchObject::buildShape()`、`getEdge()`、`buildInternals()`、`getInternalElementMap()`、`updateGeoHistory()` / `generateId()`。
2. 复核 current `cad-core` source：`buildSketchInternalResult()`、`meshForShape()`、`edgeSegmentsForShape()`、`subshapeMapForShape()`、`internalElementMapForSketch()`、`namedShapeForSketchInternalShape()`、`responseMesh()`、`responseSubshapes()`。
3. 更新 source matrix 的 `review_status`、`landing` 和 `next_action`。
4. 明确 closed internal profile 与 open wire profile 的 source 差异，不把 open wire mesh null 直接当成 closed profile edge 丢失。
5. 将本 S1 step 文件重命名为带 `【已实现】` 的同名文件。

## 关闭条件

- `C12M11-BLOCKER-101` 关闭：FreeCAD / cad-core source authority 已复核。
- contract matrix 能明确 `edgeSegments` 与 `subshapes` 的同源要求。

## 非目标

- 不从现有 fixture 输出倒推 FreeCAD 语义。
- 不修改 C++。
- 不做前端消费修复。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n "buildShape\\(|getInternalElementMap|buildInternals|getEdge\\(|updateGeoHistory|generateId" src/Mod/Sketcher/App
rg -n "edgeSegments|InternalEdge|meshForShape|responseSubshapes|internalElementMapForSketch" cad-core/src cad-core/include cad-core/tests
git diff --check
```
