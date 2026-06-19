# C3M4 Part Workbench Surface ProjectOnSurface 独立主线草案

## 当前基线

- 来源主线：`docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/`。
- S4 裁决：RuledProjection 主线不实现 `Part::ProjectOnSurface`，只把它发布为 source-audited / planned；`Part::RuledSurface` edge/edge 第一批保持 supported。
- 当前缺口：`cad-core` 未注册 `Part::ProjectOnSurface` executor；`cad-core/tools/collect_freecad_expected.py` 未启用 `Part::ProjectOnSurface` native type，且 `set_property()` 还不支持普通 `App::PropertyLinkSubList`，不能稳定设置 FreeCAD `Projection` 属性。

## FreeCAD 依据

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.h::Part::ProjectOnSurface`：声明 `Mode`、`Height`、`Offset`、`Direction`、`SupportFace`、`Projection`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::tryExecute()`：`getSupportFace()` -> `getProjectionShapes()` -> `createProjectedWire()` -> `filterShapes()` -> `createCompound()` -> restore `Placement`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::projectWire()`：`BRepProj_Projection(wire, supportFace, dir)`，取最近 projected wire，再拆成 edges。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::projectFace()`、`createFaceFromParametricWire()`、`fixWire()`、`createSolidIfHeight()`、`getOffsetPlacement()`：分别覆盖 face rebuild / holes、wire repair、solid height 和 offset placement，不能混入第一批。

## 第一批候选

第一批只允许 `Mode=Edges`、`Height=0`、`Offset=0`、单 projection item、单 support face：

- `part-project-on-surface-edge-plane`：`Part::Line.Edge1` 沿 `Direction=(0,0,1)` 投影到 `Part::Plane.Face1`。
- `part-project-on-surface-wire-plane`：单 wire 投影到 `Part::Plane.Face1`，只在 edge fixture 通过后加入。
- `part-project-on-surface-invalid-boundaries`：拒绝 `Mode=Faces`、`Height != 0`、`Offset != 0`、多个 `Projection` item、非 face support、非 edge/wire projection。

## 实施顺序

1. collector/input：在 `collect_freecad_expected.py` 启用 native `Part::ProjectOnSurface`，并让 `set_property()` 支持普通 `App::PropertyLinkSubList`；用 FreeCAD native object 采集 checked-in expected。
2. executor：新增 `cad-core/src/part/part_project_on_surface.cpp`、`part_feature.h` 声明、`feature_registry.cpp` 注册、`CMakeLists.txt` source；只接受第一批边界。
3. geometry：用 OCCT `BRepProj_Projection` 对齐 FreeCAD `projectWire()`；输出 compound/edge 结果时明确 `Offset=0`、`Height=0`、`Mode=Edges`。
4. topo policy：先裁决 projected edge 是否只保留普通 `NamedShape` 枚举，还是记录 source edge/support face provenance event；不得按输出顺序或 fixture 名补猜。
5. tests/docs：新增 fixture、expected、`tests.test_p8_features` focused tests、`tests.test_expected_fixtures` expected parity，再更新 capability 发布文档。

## 非目标

- 不覆盖 `projectFace()`、holes、face rebuild、`ShapeFix_Wire` / `ShapeFix_Wireframe` history。
- 不覆盖 `Mode=Faces` / `Mode=All`、`createSolidIfHeight()`、`getOffsetPlacement()`、多个 projection shape 的 compound 顺序。
- 不把 full Part surface family 或 full ProjectOnSurface 写成 supported。

## 验收命令

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线 cad-core
```

第一批实现回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures
```

发布闸门：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

## 晋升 / 删除条件

- 晋升条件：第一批 native expected、cad-core executor、focused tests、expected parity 和 capability 文档全部通过后，才能把 `part-project-on-surface-edge-plane` 从 planned 提升为 supported。
- 删除条件：若 FreeCAD native collector 无法稳定创建 `Part::ProjectOnSurface` expected，或 `BRepProj_Projection` 在当前 OCCT 基线无法稳定复现，保持 planned / blocked，不在 RuledSurface 主线中补窄路径。
