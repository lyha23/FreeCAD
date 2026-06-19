# PARTSURF-S3 RuledSurface 首批实现

## 目标

实现 source-backed `Part::RuledSurface` 第一批能力：edge/edge 输入、orientation enum、expected-backed fixtures、稳定 diagnostics 和最小 subshape provenance，不实现 full surface family。

## 必读

- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/6-19-18-21-C3M4-PartWorkbenchSurface-RuledProjection收口方案.md`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/矩阵/part_surface_scope_review_matrix.tsv`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/矩阵/part_surface_fixture_oracle_matrix.tsv`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp`

## 预期代码落点

- `cad-core/include/cad_core/part/part_feature.h`
- `cad-core/src/runtime/feature_registry.cpp`
- `cad-core/src/part/part_ruled_surface.cpp` 或同等 Part-owned executor 文件
- `cad-core/src/part/topo_shape_expansion.*` 或同等 FreeCAD `makeElementRuledSurface` helper 落点
- `cad-core/CMakeLists.txt`
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/fixtures/p8/part-ruled-surface-*.json`
- `cad-core/fixtures/p8/expected/part-ruled-surface-*.freecad.json`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_expected_fixtures.py`

## 工作内容

1. 新增 `Part::RuledSurface` executor 注册和属性解析，按 FreeCAD `Curve1` / `Curve2` / `Orientation` 语义读取 link-sub shape。
2. 实现 edge/edge `BRepFill::Face` 分支与 `Automatic` / `Forward` / `Reversed` orientation 行为；若 S2 没有提升 wire/wire，则遇到 wire 或复杂输入时输出明确 diagnostics。
3. 实现或调用 topo helper 记录源 edge 到输出 edge 的 relation；不得只比较 final shape 后补猜 subname。
4. 扩展 FreeCAD expected collector 和 p8 fixtures，覆盖 line-line、conic-line、orientation-reversed、invalid-input。
5. 更新 S3 相关 scope/blocker/fixture 矩阵状态，写清仍未支持的 wire/wire 或 ProjectOnSurface 分支。

## 非目标

- 不实现 ProjectOnSurface。
- 不实现 Loft/Sweep/Filling/GeomPlate。
- 不用 BREP 或 polyline 替代 typed input。
- 不提交或回退既有 Sketcher 改动。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures

cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线 cad-core
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/工作步骤细分 --format markdown
```

完成后把本文件重命名为 `6-19-18-25-【已实现】PARTSURF-S3-RuledSurface首批实现.md`。
