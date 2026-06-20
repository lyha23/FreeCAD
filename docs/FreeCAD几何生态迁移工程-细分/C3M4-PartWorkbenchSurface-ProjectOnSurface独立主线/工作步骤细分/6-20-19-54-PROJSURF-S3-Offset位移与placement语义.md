# PROJSURF-S3 Offset 位移与 placement 语义

## 目标

实现 `ProjectOnSurface::getOffsetPlacement()` 与 `createCompound()` 的 offset 语义：投影和过滤完成后，对 compound 中每个 child shape 按归一化 `Direction * Offset` 进行移动。

## 必读

- S1 / S2 已实现文件和本包矩阵。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::createCompound()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::getOffsetPlacement()`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/part/part_project_on_surface.cpp`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/tests/test_p8_features.py`

## 工作内容

1. 采集 offset expected：至少覆盖 edge projection offset、face projection offset；若 S2 已有 Height solid，则补一条 Height+Offset 组合边界。
2. 在 cad-core helper 中按 FreeCAD 顺序执行：先投影、face/solid/filter，再 `createCompound()` 时移动 child shape。
3. `Direction` 必须沿用已校验的非零方向，先 normalize 再按 Offset scale。
4. tests 断言 bbox/shape metadata、offset 后 subshape 稳定性和 expected parity。
5. 更新 capability remaining gap，只迁出 offset 已覆盖部分。

## 非目标

- 不改变 source graph 或长期保存 moved BREP。
- 不支持多个 Projection item，除非 S4 同步完成。
- 不发布 mapper/history provenance。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线 cad-core

cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures
```

完成后把本文件重命名为 `6-20-19-54-【已实现】PROJSURF-S3-Offset位移与placement语义.md`，并按仓库规则提交本轮相关改动。
