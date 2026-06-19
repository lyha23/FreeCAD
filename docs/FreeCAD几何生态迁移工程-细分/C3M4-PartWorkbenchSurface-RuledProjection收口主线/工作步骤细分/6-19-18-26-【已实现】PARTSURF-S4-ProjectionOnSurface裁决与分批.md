# 【已实现】PARTSURF-S4 ProjectionOnSurface 裁决与分批

## 目标

在 RuledSurface 首批实现后，对 `Part::ProjectOnSurface` 做 source-backed 裁决：要么实现一个窄的 edge projection 批次，要么生成后续独立主线并把本主线发布口径固定为 RuledSurface。

## 必读

- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/6-19-18-21-C3M4-PartWorkbenchSurface-RuledProjection收口方案.md`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/矩阵/part_surface_fixture_oracle_matrix.tsv`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.h`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/part/part_ruled_surface.cpp` 或 S3 实际落点

## 工作内容

1. 复核 `ProjectOnSurface::projectWire()`、`projectFace()`、`filterShapes()`、`createSolidIfHeight()` 的分支复杂度。
2. 若 S4 实现窄批次，只允许 `Mode=Edges`、`Height=0`、`Offset=0`、单 edge/wire 到单 support face，且必须有 FreeCAD expected。
3. 若 S4 不实现代码，必须新增后续 ProjectOnSurface 主线草案或矩阵条目，明确拆分原因、第一批 candidate fixture 和 non-goals。
4. 无论是否实现 ProjectOnSurface，都要更新本主线矩阵，防止 S5 发布 full ProjectOnSurface supported。

## live 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD`：`f4bd07ecbf`。
- `git log -1 --oneline`：`f4bd07ecbf feat: 实现PARTSURF S3 RuledSurface首批`。
- `git -c core.quotepath=false status --short -uall`：既有未暂存 `src/Mod/Sketcher/App/SketchObject.h`、`src/Mod/Sketcher/App/SketchObjectPyImp.cpp`；另有未跟踪 Family / Filling / GeomPlate / Loft / Sweep / CADCore 总览等其它 Surface/CADCore 主线 docs。S4 不回退、不覆盖、不暂存这些既有改动。

## FreeCAD 调用链复核

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.h::Part::ProjectOnSurface` 声明 `Mode`、`Height`、`Offset`、`Direction`、`SupportFace`、`Projection`；私有步骤包括 `getSupportFace()`、`getProjectionShapes()`、`createProjectedWire()`、`projectWire()`、`projectFace()`、`filterShapes()`、`createSolidIfHeight()`、`createCompound()`、`getOffsetPlacement()`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::tryExecute()` 先 `getSupportFace()`，再 `getProjectionShapes()`，读取 `Direction`，逐个 `createProjectedWire()`，再 `filterShapes()`，最后 `Shape.setValue(createCompound(results))` 并恢复原 `Placement`。
- `getSupportFace()` 要求 `SupportFace` 存在且只有一个 subname，使用 `ShapeOption::NeedSubElement | ShapeOption::ResolveLink | ShapeOption::Transform` 取单个 face；关键错误为 `"No support face specified"`、`"Expect exactly one support face"`。
- `getProjectionShapes()` 读取 `Projection.getValues()` / `Projection.getSubValues()`，要求对象和 sub-name 数量一致；关键错误为 `"Number of objects and sub-names differ"`。
- `createProjectedWire()` 对 face 输入进入 `projectFace()`、`createFaceFromWire()`、`createSolidIfHeight()`；对 wire/edge 输入进入 `projectWire()`；其它 shape 静默为空。
- `projectWire()` 是窄批次候选：`BRepProj_Projection(wire, supportFace, dir)` 后用 `BRepExtrema_DistShapeShape` 取距离源 shape 最近的 projected wire，再拆 edge 输出。
- `projectFace()` / `createFaceFromParametricWire()` / `fixWire()` 会投影外/内 wires、补 pcurve、`ShapeFix_Wire` / `ShapeFix_Wireframe` 修复、按 support surface 重建 face，并用 `ShapeFix_Face` / `BRepCheck_Analyzer` 选择方向；这是 holes / face rebuild 的独立语义。
- `createSolidIfHeight()` 只在 `Height >= Precision::Confusion()` 且 `Mode == All` 时沿反向 `Direction` 做 `BRepPrimAPI_MakePrism`；`filterShapes()` 会按 `All` / `Faces` / `Edges` 改变输出集合；`createCompound()` 还会通过 `getOffsetPlacement()` 应用 offset。

## S4 裁决

选择出口 B：本轮不写 cad-core 代码，把 `Part::ProjectOnSurface` 拆出后续独立主线。本 RuledProjection 主线发布口径固定为 `Part::RuledSurface` edge/edge source-backed supported；`Part::ProjectOnSurface` 只能写为 source-audited / planned，不能写为 supported。

拆分原因：

- 当前 `cad-core/src/runtime/feature_registry.cpp` 未注册 `Part::ProjectOnSurface`，`cad-core/include/cad_core/part/part_feature.h` 也没有 executor 声明；新建 executor、CMake、OCCT link、diagnostics、metadata 和 compound/named shape 策略会超过 RuledSurface 收口的语义边界。
- `cad-core/tools/collect_freecad_expected.py` 的 `SUPPORTED_NATIVE_TYPES` 未启用 `Part::ProjectOnSurface`，并且 `set_property()` 只支持 `App::PropertyLinkSubListHidden`，不支持 FreeCAD `Projection` 需要的普通 `App::PropertyLinkSubList`。没有 native expected 之前，窄批次不能闭环。
- `projectWire()` 虽窄，但仍依赖 support face link-sub、projection link-sub-list、direction、Mode filter、compound 输出和 offset 为 0 的边界断言；若在本主线硬塞实现，很容易让 S5 误发布 full ProjectOnSurface。
- FreeCAD `ProjectOnSurface` 未在该源码片段内提供 `ElementMap` / `MapperHistory` 等价账本；后续主线必须先裁决 projected edge 是否只保留普通 `NamedShape` 枚举，还是需要 source edge/support face provenance 事件，不能在 executor 中靠输出顺序补猜。

## 后续主线草案

- 新建后续草案：`docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线/6-19-19-18-C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线草案.md`。
- 第一批 candidate fixture：`part-project-on-surface-edge-plane`，只覆盖 `Mode=Edges`、`Height=0`、`Offset=0`、单 `Part::Line.Edge1` 或单 wire 投影到单 `Part::Plane.Face1`，expected 必须来自 native `Part::ProjectOnSurface` object。
- 第一批前置条件：collector 启用 `Part::ProjectOnSurface`；`set_property()` 支持普通 `App::PropertyLinkSubList`；cad-core executor 明确拒绝 face input、Height 非 0、Offset 非 0、多 projection shape 和非 Edges mode；focused tests 同时断言 diagnostics、output topology 和发布口径。
- 后续 deferred：face rebuild / holes、`Mode=Faces` / `Mode=All`、solid height、offset placement、多 projection compound 顺序、projected edge topo provenance 晋升。

## 非目标

- 不实现完整 face rebuild、holes、solid height、offset placement、多 projection compound 顺序。
- 不把 ProjectOnSurface 写成 full supported。
- 不修改 unrelated PartDesign / Sketcher / Assembly 代码。
- 不触碰既有未暂存 Sketcher 两文件，也不触碰其它未跟踪 Surface/CADCore 主线 docs。

## 验收

若只做裁决文档：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/工作步骤细分 --format markdown
```

若实现窄批次：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures
```

完成后把本文件重命名为 `6-19-18-26-【已实现】PARTSURF-S4-ProjectionOnSurface裁决与分批.md`。

完成状态：本文件已按完成规则命名为 `6-19-18-26-【已实现】PARTSURF-S4-ProjectionOnSurface裁决与分批.md`。
