# C4-S5B M2 PartDesign Pipe 单族主线

## 目标

把 `PartDesign::AdditivePipe` / `PartDesign::SubtractivePipe` 从 C4-S5 deferred 边界推进为独立可验收包。先按 FreeCAD `FeaturePipe.cpp::Pipe::execute()` 拆 Profile、Spine、Sections、Mode、Transformation、Transition、PipeShell maker history 与 Body boolean fuse/cut，再决定第一批 supported slice。

## 必读文件

- `src/Mod/PartDesign/App/FeaturePipe.cpp`
- `src/Mod/PartDesign/App/FeatureSketchBased.cpp`
- `src/Mod/PartDesign/App/Body.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/src/part_design`

## 产物

- Pipe source / fixture / diagnostic matrix rows。
- Native expected-backed fixture 或稳定 deferred diagnostic。
- 若实现，补 `feature_pipe.*`、registry、collector、fixtures、tests、capability metadata。

## 非目标

- 不把 Hole internal PipeShell 或 Part Workbench `Part::Sweep` 直接当作 PartDesign Pipe 支持。
- 不先实现 Auxiliary / Binormal / scaling law 全量分支。
- 不迁移 GUI task panel。

## C4-S5B 完成状态

- Supported first slice：`PartDesign::AdditivePipe` / `PartDesign::SubtractivePipe` 的 full-profile Sketch `Profile` + Body 内 open-line Sketch `Spine`，覆盖 `Mode=Standard`、`Transformation=Constant`、`Transition=Transformed`、PipeShell maker history、`AddSubShape` 和 Body fuse/cut replay。
- Expected：`cad-core/fixtures/c4m2/partdesign-pipe-additive-body.json`、`cad-core/fixtures/c4m2/partdesign-pipe-subtractive-body.json` 已采集 native FreeCAD expected。
- Deferred diagnostics：`cad-core/fixtures/c4m2/partdesign-pipe-deferred-diagnostics.json` 覆盖 `Sections`、AuxiliarySpine、非 Standard `Mode`、非 Constant `Transformation`、非默认 `Transition`，诊断带 `object` / `property` / `target`。
- Deferred / non-goal：AuxiliarySpine、Binormal、scaling law、完整 `Transformation`、非默认 `Transition`、SpineTangent、完整 sewing MapperHistory、GUI task panel、Hole internal PipeShell 和 Part Workbench `Part::Sweep` 不计入本 slice 支持。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```
