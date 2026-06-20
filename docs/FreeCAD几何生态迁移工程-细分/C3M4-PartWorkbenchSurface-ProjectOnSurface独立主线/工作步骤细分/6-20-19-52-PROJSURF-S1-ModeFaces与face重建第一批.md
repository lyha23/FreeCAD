# PROJSURF-S1 ModeFaces 与 face 重建第一批

## 目标

实现 `Part::ProjectOnSurface` 的 face projection 第一批：`Mode=Faces` / `Mode=All`、`Height=0`、`Offset=0`、单 face projection 到单 support face，并覆盖外 wire 与 hole inner wire 的 FreeCAD expected。

## 必读

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/part/part_project_on_surface.cpp`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/tools/collect_freecad_expected.py`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/tests/test_p8_features.py`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/tests/test_expected_fixtures.py`
- 本目录 `矩阵/part_project_on_surface_second_batch_queue.tsv`

## FreeCAD 依据

- `ProjectOnSurface::createProjectedWire()`：face 输入进入 `projectFace()`，再 `createFaceFromWire()` 和 `createSolidIfHeight()`。
- `ProjectOnSurface::projectFace()`：逐个 wire 使用 `BRepProj_Projection`，并对 projected wire 调用 `fixWire()`。
- `ProjectOnSurface::createFaceFromParametricWire()`：第一个 wire 作为 outer wire，后续 wires 作为 inside wires，失败时尝试反向并用 `ShapeFix_Face` / `BRepCheck_Analyzer` 校验。
- `ProjectOnSurface::filterShapes()`：`Faces` 只保留 face；`Edges` 会把 face 拆成 wires；`All` 保留非空结果。

## 工作内容

1. 先采集 native FreeCAD expected，再写 cad-core 代码；不得从 cad-core 输出倒推 expected。
2. 新增最小完整 fixtures：普通 face projection、带 hole face projection、face input 在 `Mode=Edges` 下拆 wire 的语义边界。
3. 在 `cad-core/src/part/part_project_on_surface.cpp` 中抽出正式 helper：project face wires、parametric-space edge/wire rebuild、face fix/reverse retry；禁止按 fixture 名或输出顺序猜。
4. 更新 focused tests，断言 face/wire/inner wire 数量、metadata、diagnostics 和 expected parity。
5. 更新本包矩阵状态，但 capability 不宣称 Height / Offset / multi-projection 已完成。

## 非目标

- 不实现 `Height > 0` solid。
- 不实现 `Offset != 0`。
- 不支持多个 `Projection` item。
- 不发布 projected edge provenance mapper/history。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线 cad-core

cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures
```

完成后把本文件重命名为 `6-20-19-52-【已实现】PROJSURF-S1-ModeFaces与face重建第一批.md`，并按仓库规则提交本轮相关改动。
