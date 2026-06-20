# C5-M6-S1 Loft Profile / PostProcess 复核收口【已实现】

## 目标

复核并收口 `Part::Loft` face / vertex profile 与 `Linearize=true` 后处理。当前 live 代码已显示 expected-backed 支持；本步骤优先验证和补文档，只有证据缺失时才进入 oracle-first 实现。

## 必读

- `src/Mod/Part/App/PartFeatures.cpp::Loft::execute()`
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementLoft()`
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::linearize()`
- `cad-core/src/part/part_loft.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/tests/test_p8_features.py`
- `cad-core/src/adapters/c_api/c_api.cpp`
- 本包矩阵。

## 工作内容

1. 验证 `part-loft-linearize-profile-face` 和 `part-loft-linearize-profile-vertex` fixtures / expected / focused tests。
2. 若缺失，先采集 native FreeCAD expected，再补 cad-core，不得从 cad-core 输出倒推 expected。
3. 确认 capability 只发布 face / vertex profile 与 `linearize_faces_no_edges_post_processing`。
4. 保留 `complex_profile_family` 为 remaining gap / non-goal。
5. 更新本包矩阵和 CADCore3.0 文档中 Loft 发布口径。

## S1 live 记录

基线命令输出：

```text
pwd
/Users/li/Chili3DProject/FreeCAD

git rev-parse --short HEAD
07a0b3903d

git log -1 --oneline
07a0b3903d docs: 冻结 C5-M6 S0 live 基线

git -c core.quotepath=false status --short -uall
<clean>
```

复核结论：

- FreeCAD `src/Mod/Part/App/PartFeatures.cpp::Loft::execute()` 在 `result.makeElementLoft(...)` 后仅以 `LinearizeFace::linearizeFaces` / `LinearizeEdge::noEdges` 做后处理。
- FreeCAD `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementLoft()` 对 profile 走 `AddVertex` / `AddWire`，并以 `MapperThruSections` 消费 maker history。
- cad-core `cad-core/src/part/part_loft.cpp` 与 `cad-core/src/part/topo_shape_expansion.cpp` 已对应发布 `linearize` request property、face / vertex profile 和 `part_loft:linearize*` history status。
- `cad-core/fixtures/c4m1/part-loft-linearize-profile-face.json` 与 `part-loft-linearize-profile-vertex.json` 均有 `cad-core/fixtures/c4m1/expected/*.freecad.json` 原生 expected；`tests.test_p8_features` 对两者调用 `assert_object_matches_expected`。
- `cad-core/src/adapters/c_api/c_api.cpp` 的 `part_workbench.loft.status` 为 `supported_profile_linearize_expected_backed`，仅声明 `face_vertex_profile_expected_backed` 与 `linearize_faces_no_edges_post_processing`；`complex_profile_family` 仍在 remaining gaps / non-goals。
- 本步未修改 C++ 或 fixture，关闭 `C5M6-BLK-002`。

## 非目标

- 不处理 PartDesign Loft。
- 不实现复杂 profile family。
- 不修改 Sweep、Filling、GeomPlate。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters

cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线 docs/CADCore3.0 cad-core
```

完成后重命名为 `6-20-22-05-【已实现】C5-M6-S1-LoftProfilePostProcess复核收口.md`。
