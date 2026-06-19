# C4-M1 Part Workbench Surface Family 补完方案

## 目标

C4-M1 接在 C3M4 surface family first batch 之后，只从明确边界推进 Part Workbench surface 能力，不恢复“完整 surface family”这种宽泛 gap。

## 当前基线

C3.0 已发布：

- `Part::RuledSurface` edge/edge 第一批。
- `Part::Loft` expected-backed 第一批。
- `Part::Sweep` / PipeShell expected-backed 第一批。
- `Part.makeFilledFace()` source-backed helper 第一批。
- `Part.GeomPlate.BuildPlateSurface` source-backed geometry helper 第一批。

当前 remaining boundary：

- `Part::ProjectOnSurface`
- `RuledSurface` wire/wire `BRepFill::Shell`
- `Loft` `Linearize=true`、face / vertex profile、复杂 profile family
- `Sweep` `Linearize=true`、multi-profile、advanced PipeShell wrapper
- Filling `surface`、`supports`、`orders`、non-default params、non-boundary constraints、compound optional case
- GeomPlate initial surface、G1 curve-on-surface、projected 2D curve、2D point-on-surface、custom criteria、`Part.PlateSurface.Curves` wrapper

## 实施批次

| 批次 | 范围 | FreeCAD 入口 | cad-core 落点 |
| --- | --- | --- | --- |
| C4-M1-S1 | ProjectOnSurface oracle-first | `src/Mod/Part/App/FeatureProjectOnSurface.cpp/.h` 中 `ProjectOnSurface::execute/tryExecute/getSupportFace/getProjectionShapes/filterShapes/createProjectedWire/projectWire/projectFace/createSolidIfHeight/getOffsetPlacement` | `cad-core/src/part/part_project_on_surface.cpp`、collector、adapter capability |
| C4-M1-S2 | RuledSurface wire/wire | `TopoShape::makeElementRuledSurface()` 的 `BRepFill::Shell` 分支 | `part_ruled_surface.cpp`、`topo_shape_expansion.cpp`、MapperHistory |
| C4-M1-S3 | Loft profile family | `PartFeatures.cpp::Loft::execute()` 与 `makeElementLoft()` | `part_loft.cpp`、`topo_shape_expansion.cpp`、expected fixtures |
| C4-M1-S4 | Sweep / PipeShell advanced family | `PartFeatures.cpp::Sweep::execute()` 与 `makeElementPipeShell()` | `part_sweep.cpp`、PipeShell helper、spine / section resolver |
| C4-M1-S5 | Filling advanced helper | `AppPartPy.cpp::makeFilledFace()` 与 `TopoShape::makeElementFilledFace()` | `part_filling.cpp`、BRepOffsetAPI_MakeFilling wrapper |
| C4-M1-S6 | GeomPlate advanced helper | `Tools.cpp::makeSurface()`、`GeomPlate/BuildPlateSurfacePyImp.cpp`、`Geometry.cpp` | `part_geomplate.cpp`、constraint DTO、surface adapter |

## 必须保持的边界

- Filling / GeomPlate 如果仍不是原生 FreeCAD `DocumentObject`，capability 必须写成 helper，不得伪装成 DocumentObject executor。
- ProjectOnSurface 已完成 C4-S1 first slice：只发布 `Mode=Edges`、`Height=0`、`Offset=0`、单 `SupportFace`、单 edge/wire `Projection`；expected 来自 native FreeCADCmd collector，不得用 cad-core 当前输出倒推 expected。
- `PartFeatures.cpp` / `TopoShapeExpansion.cpp` 是 C4 初稿中的 stale ProjectOnSurface 路径；ProjectOnSurface 的实际 DocumentObject 源码是 `FeatureProjectOnSurface.cpp/.h`。
- Linearize、multi-profile、support/order/constraints 必须按 FreeCAD 调用链拆批，不能合成一个巨大 “surface complete” 任务。
- 不支持 GUI surface workbench 编辑器和 task panel。

## 交付物

- 每个批次至少一组 native expected fixture。
- 对应 focused tests：`tests.test_p8_features`、`tests.test_expected_fixtures`、`tests.test_adapters`。
- `capabilities-gap` / `cad_core_capabilities_json()` 中 capability、payload keys、covered fixtures、diagnostics、remaining gaps 同步。
- 若某项继续 deferred，写入明确 next owner 和 diagnostic，而不是 broad gap。

## 验收

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore4.0 docs/CADCore3.0 cad-core
```

阶段回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

## 可执行包入口

- ProjectOnSurface：`docs/CADCore4.0/C4-M1-PartWorkbenchSurface-ProjectOnSurface独立主线/工作步骤细分/6-20-00-02-【已实现】C4-S1-M1-ProjectOnSurface-oracle-first.md`
- RuledSurface / Loft：`docs/CADCore4.0/C4-M1-PartWorkbenchSurface-RuledSurface-Loft补完主线/工作步骤细分/6-20-00-03-C4-S2-M1-RuledSurface-Loft补完.md`
- Sweep / Filling / GeomPlate：`docs/CADCore4.0/C4-M1-PartWorkbenchSurface-SweepFillingGeomPlate补完主线/工作步骤细分/6-20-00-04-C4-S3-M1-SweepFillingGeomPlate补完.md`
- oracle 矩阵：`docs/CADCore4.0/矩阵/cadcore4_fixture_oracle_matrix.tsv`
