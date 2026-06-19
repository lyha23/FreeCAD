# 【已实现】SWEEP-S1 executor 与 fixture 实现

新增 source-backed `Part::Sweep` executor、PipeShell helper、fixtures、FreeCAD expected 和 focused tests。覆盖 S0 首批 `SWEEP-FIX-001..008`，但不发布 C ABI capability。

## 实现边界

- `cad-core/src/part/part_sweep.cpp` 注册并执行 `Part::Sweep`，从 `DocumentObject` 属性读取 `Spine`、`Sections`、`Solid`、`Frenet`、`Transition`、`Linearize`。
- `Spine` 按 FreeCAD `App::PropertyLinkSub` 语义解析；存在 `SubList` 时先解析 selected subshapes 并合成 compound，再作为 PipeShell 第一个 source。
- `Sections` 按 `App::PropertyLinkList` 解析；首批支持一个 profile，profile 可为闭合 wire / face-derived wire / open edge。
- `Solid` 路由到 `MakeSolid()`，`Frenet` 路由到 `BRepOffsetAPI_MakePipeShell::SetMode()`，`Transition` 支持 `Transformed`、`Right corner`、`Round corner`。
- `Linearize=true` 只返回 defer diagnostic，不声明支持。
- PipeShell helper 位于 `cad-core/src/part/topo_shape_expansion.cpp`，调用 `BRepOffsetAPI_MakePipeShell` 并用 maker `Modified/Generated` history 生成 `NamedShape`；没有 adapter 输出修补或 fixture 名称分支。

## FreeCAD 依据

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()`：`Sections` 空时报 `"No sections linked."`，`Spine` 空时报 `"No spine"`，`Spine.getSubValues()` 分支逐个 `getSubTopoShape()` 后 `makeElementCompound(... returnShape)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementPipeShell()`：第一项 `makeElementWires()` 成 single wire，后续 `prepareProfiles(shapes, 1)`，再 `SetMode()`、`SetTransitionMode()`、`Add()`、`Build()`、可选 `MakeSolid()`，最后 `makeElementShape(mkPipeShell, shapes, op)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::makeSweepSurface()`：说明 `GeomFill_Pipe` 没有 shape history，因此低层 helper 应走 `makeElementPipeShell()`。

## Fixture 与 expected

- 新增成功 fixtures：`part-sweep-right-corner-surface`、`part-sweep-solid`、`part-sweep-frenet-off`、`part-sweep-transition-transformed`、`part-sweep-transition-round-corner`、`part-sweep-spine-subedges`、`part-sweep-open-profile-surface`。
- 新增诊断 fixture：`part-sweep-invalid-inputs`，固定 empty Sections、missing Spine、invalid Spine SubList、disconnected spine、missing section target、non-profile section 等诊断。
- 成功 case 的 `.freecad.json` 均由 `FreeCADCmd` 通过 `cad-core/tools/collect_freecad_expected.py` 采集；invalid case 按 Loft 诊断 fixture 模式固定 diagnostic codes，不伪造 shape expected。
- 本轮补 `Part::Compound` collector 支持，是为了采集 source-backed sweep fixtures 的 spine compound 依赖；支持声明仍是 `Part::Sweep`。

## 验收

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c3m4_part_sweep_right_corner_surface_uses_pipeshell_history \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c3m4_part_sweep_solid_builds_solid_not_surface_only \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c3m4_part_sweep_frenet_false_routes_set_mode_false \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c3m4_part_sweep_transition_transformed_is_expected_backed \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c3m4_part_sweep_transition_round_corner_is_expected_backed \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c3m4_part_sweep_spine_subedges_compound_before_pipeshell \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c3m4_part_sweep_open_profile_surface_accepts_edge_profile \
  tests.test_p8_features.CadCoreP8FeatureTest.test_c3m4_part_sweep_invalid_inputs_have_stable_diagnostics
```

相关回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest
```

收口检查：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- cad-core docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-SweepPipeShell收口主线
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-SweepPipeShell收口主线/工作步骤细分 --format markdown
```

完成状态：本文件已按完成规则命名为 `6-19-18-40-【已实现】SWEEP-S1-executor与fixture实现.md`。
