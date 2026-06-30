# C12-M11 S2 current response contract 复核【已实现】

## 目标

用 focused current evidence 判断当前 `cad-core` 是否已经返回对齐的草图边 `mesh.edgeSegments[]` 与 `subshapes[]`。

## 必读文件

- `../README.md`
- `../7-1-02-57-C12-M11-SketchInternalEdgeSubshapeMeshContract批次方案.md`
- `../矩阵/c12m11_sketch_edge_contract_matrix.tsv`
- `../矩阵/c12m11_sketch_edge_gap_classification.tsv`
- `../矩阵/c12m11_sketch_edge_validation_matrix.tsv`

## 操作

1. 优先运行 focused adapter/sketch tests，确认 closed internal profile 是否返回 `Sketch:InternalEdgeN` edgeSegments 与 subshapes。
2. 复核 open wire profile 的当前行为：raw `EdgeN` 是否在 subshapes 中存在，mesh 是否 intentional null，是否需要产品契约另开。
3. 如 tests 不可运行，读取 checked-in tests / fixtures / expected 作为 current evidence，并记录未运行原因。
4. 更新 contract / gap / validation matrix。
5. 将本 S2 step 文件重命名为带 `【已实现】` 的同名文件。

## 关闭条件

- `C12M11-BLOCKER-201` 关闭：current response contract 有明确 evidence。
- closed internal profile 的 `mesh.edgeSegments[].indexed == subshapes[].indexed` 结果已记录。
- open wire profile 被单独分类，不混入 closed profile contract。

## 非目标

- 不直接判断前端消费是否正确。
- 不把测试缺失当作 backend 已失败。
- 不刷新 expected。

## 闭合记录

S2 current response contract 复核已关闭：

- 执行基线为 `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `HEAD=385937da37`（`385937da37 文档：关闭 C12-M11 S1 source 复核`）。
- 起点 dirty boundary 为 `<clean>`。
- focused tests `C12M11-VAL-201..203` 全部通过。
- closed `p5/sketch-internal-face` 当前 FFI response 返回 `Sketch:InternalEdge1..4` mesh `edgeSegments`，每条 `indexed=InternalEdgeN`，并有同名 edge `subshapes`；request-local `stableSubname` 为 `InternalEdge1..4 -> Edge1..4`。
- alignment evidence 中 `mvp/rect-pad` 当前 FFI response 为 12 条 `edgeSegments` / 12 条 Edge `subshapes`，`mismatchCount=0`。
- open `p5/sketch-open-wire-internal-empty` 当前 FFI response 单独分类：`mesh=null`，raw `Sketch:Edge1..3` subshapes 可见且 `stableSubname=Edge1..3`；这不混入 closed internal profile contract。
- `C12M11-BLOCKER-201` 已关闭，后续从 S3 `7-1-03-02-C12-M11-S3-contract-gap分流裁决.md` 开始。
- 本步只更新 docs/矩阵并重命名 step，未修改 C++、fixtures、expected、tests 或 adapters。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_returns_sketch_internal_profile_mesh
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_mesh_edge_segments_reference_result_subshapes
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_sketch_exports_internal_edge_vertex_stable_subnames
```
