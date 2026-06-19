# 【已实现】C4-S5A M2 PartDesign Loft 单族主线

## 目标

把 `PartDesign::AdditiveLoft` / `PartDesign::SubtractiveLoft` 从 C4-S5 deferred 边界推进为独立可验收包。先按 FreeCAD `FeatureLoft.cpp::Loft::execute()` 拆 profile、Sections、Ruled、Closed、sewing / solidification、AddSubShape 与 Body boolean fuse/cut history，再决定第一批 supported slice。

## 必读文件

- `src/Mod/PartDesign/App/FeatureLoft.cpp`
- `src/Mod/PartDesign/App/Body.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/src/part_design`
- `docs/CADCore4.0/C4-M2-PartDesign-LoftPipeBooleanDatum主线/矩阵/loft_pipe_boolean_datum_scope.tsv`

## 产物

- Loft source / fixture / diagnostic matrix rows。
- Native expected-backed fixture 或稳定 deferred diagnostic。
- 若实现，补 `feature_loft.*`、registry、collector、fixtures、tests、capability metadata。

## 非目标

- 不复用 Part Workbench `Part::Loft` capability 直接宣称 PartDesign Loft 支持。
- 不绕过 Body replay / AddSubShape / maker history。
- 不迁移 GUI task panel。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 完成记录

- Supported first slice：`PartDesign::AdditiveLoft` / `PartDesign::SubtractiveLoft` 的 full-profile Sketch `Profile` + one Sketch `Sections`，`Ruled=false`、`Closed=false`，通过 `Loft::execute/getSectionShape` 对齐 profile/section wire 提取、`makeElementLoft(... IsSolid::notSolid ...)`、front/back face、sewing、solidification、`AddSubShape` 和 Body add/cut replay。
- Native expected：新增 `cad-core/fixtures/c4m2/partdesign-loft-additive-body.json`、`partdesign-loft-subtractive-body.json` 及对应 `expected/*.freecad.json`；expected 由本机 FreeCADCmd collector 采集，不由 cad-core 输出倒推。
- cad-core 落点：新增 `cad-core/src/part_design/feature_loft.cpp` / `include/cad_core/part_design/feature_loft.h`，注册 `PartDesign::AdditiveLoft` / `PartDesign::SubtractiveLoft`，复用 `cad-core/src/part/topo_shape_expansion.cpp` 的正式 Loft / solidification history，Body 继续消费 `AddSubShape`。
- Tests / capability：`tests.test_p7_features` 增加 AdditiveLoft / SubtractiveLoft Body expected 对比；`tests.test_adapters` 暴露 `part_design.loft` 和 `maker_history:part_design_loft`。
- Deferred：显式 subelement selection、multi-section `Closed=true`、multi-wire ordering、AllowCompound / multi-solid fuse、完整 sewing `MapperHistory` 到 `ElementMap` 传播，以及 GUI TaskPanel / ViewProvider 不在本步支持范围；后续必须以 locatable diagnostic 或 native expected-backed fixture 推进。
