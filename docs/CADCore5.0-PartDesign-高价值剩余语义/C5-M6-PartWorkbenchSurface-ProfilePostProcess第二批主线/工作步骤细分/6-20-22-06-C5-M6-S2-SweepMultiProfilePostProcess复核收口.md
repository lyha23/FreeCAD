# C5-M6-S2 Sweep MultiProfile / PostProcess 复核收口

## 目标

复核并收口 `Part::Sweep` multi-profile `Sections` 与 `Linearize=true` 后处理。当前 live 代码已显示 expected-backed 支持；本步骤优先验证和补文档，只有证据缺失时才进入 oracle-first 实现。

## 必读

- `src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()`
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementPipeShell()`
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::linearize()`
- `cad-core/src/part/part_sweep.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/tests/test_p8_features.py`
- `cad-core/src/adapters/c_api/c_api.cpp`
- 本包矩阵。

## 工作内容

1. 验证 `part-sweep-multi-profile-linearize` fixture / expected / focused test。
2. 验证 `part-sweep-advanced-deferred` 仍输出 locatable diagnostics，不被误发布为 supported。
3. 若 expected-backed fixture 缺失，先采集 native FreeCAD expected，再补 cad-core。
4. 确认 capability 不再保留 `linearize_post_processing` 或 `multi_profile_sections_expected` 旧 gap。
5. 保留 auxiliary spine / support mode / binormal / location mode / tolerance contract 为后续 owner。

## 非目标

- 不实现 advanced `BRepOffsetAPI_MakePipeShell` wrapper。
- 不把 Hole internal PipeShell 计入 Part::Sweep support。
- 不修改 Loft、Filling、GeomPlate。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters

cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线 docs/CADCore3.0 cad-core
```

完成后重命名为 `6-20-22-06-【已实现】C5-M6-S2-SweepMultiProfilePostProcess复核收口.md`。
