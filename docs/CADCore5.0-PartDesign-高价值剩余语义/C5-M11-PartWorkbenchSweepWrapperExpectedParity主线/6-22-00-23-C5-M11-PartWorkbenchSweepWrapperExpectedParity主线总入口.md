# C5-M11 Part Workbench Sweep Wrapper Expected Parity 主线

本包承接 C5-M10 已关闭的 `part_workbench.sweep` advanced PipeShell 字段级合同。C5-M10 已把 `AuxiliarySpine`、`AuxiliaryCurvilinear`、`SpineSupport` / `SupportMode`、`Binormal` / `BiNormal`、`SectionOptions[].Location` / `WithContact` / `WithCorrection`、`Tolerance.tol3d` / `boundTol` / `tolAngular` 和组合压力落成 source / diagnostic-backed 合同；唯一剩余缺口是 `part_sweep_wrapper_expected_collector`。

C5-M11 的目标不是新增字段，而是围绕同一 `Part.BRepOffsetAPI_MakePipeShell` wrapper API 做 expected-backed 批量闭环：新增 FreeCADCmd request-local wrapper collector，一次替换 C5-M10 六个 source-backed known_gap expected，并把 capability、focused tests、fixture expected 和文档矩阵同步成 expected-backed 状态。

## 目标

- 冻结 C5-M10 当前 source / diagnostic-backed 状态，确认剩余 gap 只剩 `part_sweep_wrapper_expected_collector`。
- 在 `cad-core/tools/collect_freecad_expected.py` 增加 `Part.BRepOffsetAPI_MakePipeShell` request-local helper，不走 native `Part::Sweep` DocumentObject，也不依赖 persistent wrapper lifecycle。
- 批量采集并替换六个 C5-M10 expected：auxiliary spine、binormal、support diagnostics metadata、located profile、tolerance、advanced combined。
- 固定 expected schema：`shape_summary` + `object_fields.advanced`，字段 metadata 必须来自 FreeCAD wrapper 调用过程，不从 cad-core 输出倒推。
- 用 `tests.test_p8_features`、`tests.test_expected_fixtures`、`tests.test_adapters` 和 capability docs 收口 `part_sweep_wrapper_expected_collector`。

## 入口文件

- 方案：`6-22-00-23-C5-M11-PartWorkbenchSweepWrapperExpectedParity方案.md`
- scope 矩阵：`矩阵/c5m11_sweep_wrapper_expected_parity_scope.tsv`
- source / DTO / oracle 合同：`矩阵/c5m11_sweep_wrapper_expected_parity_source_dto_oracle_contract.tsv`
- fixture / oracle 矩阵：`矩阵/c5m11_sweep_wrapper_expected_parity_fixture_oracle_matrix.tsv`
- blocker 队列：`矩阵/c5m11_sweep_wrapper_expected_parity_blocker_queue.tsv`
- non-goal registry：`矩阵/c5m11_sweep_wrapper_expected_parity_non_goal_registry.tsv`
- validation 矩阵：`矩阵/c5m11_sweep_wrapper_expected_parity_validation_matrix.tsv`
- 工作步骤：`工作步骤细分/`

## 最小完整语义批次

| 批次 | 代表场景 | 产物 |
| --- | --- | --- |
| live gap guard | C5-M10 六个 source-backed known_gap expected 与 capability `part_sweep_wrapper_expected_collector` | S0 冻结当前 gap、fixture 列表、delete condition 和不新增字段边界 |
| wrapper collector | `Part.BRepOffsetAPI_MakePipeShell` helper：spine/profile/support/location/auxiliary/tolerance/transition | S1 设计 collector 调用顺序、输入 shape builder、metadata schema 和 unsupported/diagnostic 分流 |
| expected batch | auxiliary spine、binormal、support mode metadata、located profile、tolerance、advanced combined | S2 一次采集 expected，替换 known_gap payload，保留 diagnostic-only 场景的诊断验收 |
| tests / capability | expected fixture assertions、adapter capability、remaining gap 清理 | S3 focused tests 和 capability metadata 把字段从 source-backed known_gap 晋级 expected-backed |
| docs closeout | C5 README、root C5 矩阵、C3 capability gap、本包矩阵 | S4 收口 `C5-BLK-1101`，工作步骤全部标记完成后 queue 为空 |

## expected-backed 批量闭环标准

- 六个 C5-M10 fixture 不能只替换其中一个；除非 FreeCAD wrapper 调用链分叉或 FreeCADCmd 明确无法采集，S2 必须批量处理。
- expected 必须由 FreeCADCmd 执行 `Part.BRepOffsetAPI_MakePipeShell` wrapper 得到，不能从 cad-core result 或 bbox 差异反推。
- `object_fields.advanced` 至少记录 helper、field source、profile/section order、auxiliary/support/binormal/tolerance/location metadata、builder result status 和 `topo_naming_history=maker_history:pipeshell`。
- support diagnostics fixture 若仍包含 invalid target/subshape payload，可以保留 diagnostic-backed 子项，但 wrapper 能稳定返回的 support metadata 不能继续留在 source-backed known_gap。
- native `Part::Sweep` direct property 边界不变：仍只声明 `Sections`、`Spine`、`Solid`、`Frenet`、`Linearize`、`Transition`。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M11-PartWorkbenchSweepWrapperExpectedParity主线/工作步骤细分 --format markdown
```

## 非目标

- 不新增 C5-M10 之外的 advanced PipeShell 字段。
- 不把 advanced wrapper 字段说成 upstream native `Part::Sweep` 直接属性。
- 不实现 GUI、TaskPanel、ViewProvider、command UI、persistent Python wrapper lifecycle。
- 不把 PartDesign Pipe / Hole 产品能力并入 Part Workbench Sweep。
- 不处理 Loft `complex_profile_family`、Filling native helper expected、GeomPlate native oracle 或 ProjectOnSurface GUI/advanced 分支。
- 不修改 FreeCAD upstream source。
