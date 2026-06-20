# PROJSURF-S4 多 Projection 与 compound 顺序

## 目标

移除第一批 `Projection` 只能单 item 的限制，对齐 FreeCAD `getProjectionShapes()` 和 `tryExecute()` 的多 projection 顺序：按 `Projection.getValues()` / `getSubValues()` 顺序逐个投影，再按结果顺序进入 `filterShapes()` 和 `createCompound()`。

## 必读

- S1-S3 已实现文件和本包矩阵。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::getProjectionShapes()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::tryExecute()`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/part/part_project_on_surface.cpp`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/include/cad_core/document`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/tests/test_p8_features.py`

## 工作内容

1. 采集 native expected：至少覆盖多 edge projection、多 face projection或 face+edge 混合投影；若 FreeCAD expected 表现分叉，先记录矩阵并拆小。
2. 把 `Projection` 解析改成列表语义，保留对象/subname 数量不一致、空 subname、missing target、unsupported kind 的稳定 diagnostics。
3. compound 顺序必须由请求 LinkSubList 顺序和 FreeCAD result append 顺序决定，不得按 subshape 名称、bbox、几何类型重排。
4. focused tests 断言输出顺序、diagnostics、expected parity 和 first-slice 回归。
5. 更新矩阵和 capability remaining gap，为 S5 发布做准备。

## 非目标

- 不实现 GUI projection list 编辑。
- 不引入跨对象 projected source ownership mapper，除非另开 topo history 专题。
- 不靠 fixture 名称或输出排序修正结果。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线 cad-core

cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures
```

完成后把本文件重命名为 `6-20-19-55-【已实现】PROJSURF-S4-多Projection与compound顺序.md`，并按仓库规则提交本轮相关改动。
