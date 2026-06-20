# PROJSURF-S2 ModeAll 与 Height 实体路径

## 目标

在 S1 face rebuild 通过后，实现 `ProjectOnSurface::createSolidIfHeight()` 对应语义：`Mode=All` 且 `Height >= Precision::Confusion()` 时，把 rebuilt face 沿 `Direction` 反向拉伸成 solid。

## 必读

- S1 已实现文件和本包矩阵。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::createSolidIfHeight()`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/part/part_project_on_surface.cpp`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/tests/test_p8_features.py`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/adapters/c_api/c_api.cpp`

## 工作内容

1. 采集 native expected：至少包含 `Mode=All, Height>0` 的 solid fixture，以及 `Mode=Faces, Height>0` 不生成 solid 的边界 fixture 或 focused 断言。
2. 用 OCCT `BRepPrimAPI_MakePrism` 对齐 FreeCAD：`Direction` 反向、乘以 `Height`，只在 `Mode=All` 触发。
3. 保持 `Height < Precision::Confusion()` 走 face 路径，不新增静默容错。
4. 更新 result metadata、subshape/named shape 断言和 diagnostics。
5. 只把 Height solid 对应 gap 从 deferred 迁出；Offset 和 multi-projection 仍保持 deferred。

## 非目标

- 不改变 S1 face rebuild 的 helper 边界。
- 不实现 offset placement。
- 不支持多个 Projection item。
- 不做 full Part surface family 发布。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线 cad-core

cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures
```

完成后把本文件重命名为 `6-20-19-53-【已实现】PROJSURF-S2-ModeAll与Height实体路径.md`，并按仓库规则提交本轮相关改动。
