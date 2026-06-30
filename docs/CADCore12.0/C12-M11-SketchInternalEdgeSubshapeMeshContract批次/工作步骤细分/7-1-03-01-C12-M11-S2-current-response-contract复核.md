# C12-M11 S2 current response contract 复核

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

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_returns_sketch_internal_profile_mesh
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_mesh_edge_segments_reference_result_subshapes
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_sketch_exports_internal_edge_vertex_stable_subnames
```
