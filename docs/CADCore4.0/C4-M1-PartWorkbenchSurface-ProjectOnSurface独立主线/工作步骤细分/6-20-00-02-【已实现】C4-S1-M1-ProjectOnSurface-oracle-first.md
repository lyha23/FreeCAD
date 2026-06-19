# C4-S1 M1 ProjectOnSurface oracle-first

## 目标

为 `Part::ProjectOnSurface` 建立 oracle-first 可实现边界。先读 FreeCAD 源码和现有 C3M4 ProjectOnSurface 草案，确定是否进入 cad-core executor，或保留明确 deferred / diagnostic。

## 完成状态

已完成 expected-backed first slice：

- FreeCAD authority 修正为 `src/Mod/Part/App/FeatureProjectOnSurface.cpp/.h`；`PartFeatures.cpp` / `TopoShapeExpansion.cpp` 只是 C4 初稿 stale path。
- FreeCAD 调用链已记录：`ProjectOnSurface::execute -> tryExecute -> getSupportFace/getProjectionShapes -> createProjectedWire -> projectWire/projectFace -> filterShapes -> createCompound/getOffsetPlacement`。
- cad-core 已支持 `Mode=Edges`、`Height=0`、`Offset=0`、单 `SupportFace`、单 edge/wire `Projection`。
- native expected：`cad-core/fixtures/c4m1/expected/part-project-on-surface-edge-plane.freecad.json`。
- deferred diagnostics：GUI task panel、`Mode=Faces/All`、height/offset solid、face rebuild、multi projection、advanced branches。

## 必读文件

- `docs/CADCore4.0/C4-M1-PartWorkbenchSurfaceFamily总览/6-19-23-54-C4-M1PartWorkbenchSurfaceFamily补完方案.md`
- `docs/CADCore4.0/矩阵/cadcore4_source_candidates.tsv`
- `docs/CADCore4.0/矩阵/cadcore4_fixture_oracle_matrix.tsv`
- `docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-ProjectOnSurface独立主线/矩阵/part_project_on_surface_plan_matrix.tsv`
- `src/Mod/Part/App/FeatureProjectOnSurface.cpp`
- `src/Mod/Part/App/FeatureProjectOnSurface.h`
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/src/runtime/feature_registry.cpp`
- `cad-core/src/adapters/c_api/c_api.cpp`

## 产物

- 在矩阵中补 `ProjectOnSurface` source / DTO / fixture / diagnostic 行。已完成。
- 设计 `cad-core/fixtures/c4m1/part-project-on-surface-*.json` 草案。已完成。
- 若实现，新增或更新 collector、executor、capability metadata 和 focused tests。已完成 first slice。
- 若暂不实现，写入稳定 diagnostic、remaining boundary 和 next owner。advanced branches 已 deferred。

## 非目标

- 不把 GUI Projection task panel 行为迁入 cad-core。
- 不用 cad-core 当前输出当 expected。
- 不把 `ProjectOnSurface` 塞回 conic 或 RuledSurface capability。

## 验收

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore4.0 docs/FreeCAD几何生态迁移工程-细分 cad-core
```

实现后：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

## 完成口径

`ProjectOnSurface` 已进入 expected-backed first slice，并以明确 unsupported/deferred diagnostic 进入 4.0 矩阵；不得留下 broad surface gap。
