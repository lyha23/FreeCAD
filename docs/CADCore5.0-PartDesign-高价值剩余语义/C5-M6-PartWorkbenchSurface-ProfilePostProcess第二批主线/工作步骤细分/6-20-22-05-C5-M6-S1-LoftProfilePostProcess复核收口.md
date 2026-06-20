# C5-M6-S1 Loft Profile / PostProcess 复核收口

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
