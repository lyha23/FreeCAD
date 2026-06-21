# C5-M11 Part Workbench Sweep Wrapper Expected Parity 方案

## 当前基线

C5-M10 已把 `part_workbench.sweep` advanced PipeShell broad bucket 收口成字段级合同。当前 capability 中基础 `Part::Sweep` 字段仍是 expected-backed，advanced wrapper 字段是 source / diagnostic-backed，剩余 gap 只有 `part_sweep_wrapper_expected_collector`。C5-M11 只负责删除这个 collector gap：为同一 `Part.BRepOffsetAPI_MakePipeShell` wrapper API 建 expected 采集路径，并把 C5-M10 六个代表 fixture 一次替换成 FreeCAD expected-backed。

当前需要替换的 expected 文件：

- `cad-core/fixtures/c5m10/expected/part-sweep-auxiliary-spine-contract.freecad.json`
- `cad-core/fixtures/c5m10/expected/part-sweep-binormal-contract.freecad.json`
- `cad-core/fixtures/c5m10/expected/part-sweep-support-mode-diagnostics.freecad.json`
- `cad-core/fixtures/c5m10/expected/part-sweep-located-profile-contract.freecad.json`
- `cad-core/fixtures/c5m10/expected/part-sweep-tolerance-contract.freecad.json`
- `cad-core/fixtures/c5m10/expected/part-sweep-advanced-combined-contract.freecad.json`

## FreeCAD 调用链

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakePipeShell.pyi` 暴露 Python wrapper API：`setAuxiliarySpine`、`setSpineSupport`、`setBiNormalMode`、`add(Profile, Location, WithContact, WithCorrection)`、`setTolerance(tol3d, boundTol, tolAngular)`、`setTransitionMode`、`build` / `shape`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp` 是 C5-M11 oracle 的直接 source authority；collector 必须按这个 wrapper 调用路径生成 expected。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::setupAlgorithm()` 只作为同一 OCCT builder 的语义旁证，用来校验 auxiliary / binormal / transition 语义，不作为本包产品支持声明。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` 仍限定 native `Part::Sweep` direct properties；C5-M11 不把 wrapper 字段写成 native `Part::Sweep` 属性。

## cad-core 落点

- `cad-core/tools/collect_freecad_expected.py`：新增 request-local `Part.BRepOffsetAPI_MakePipeShell` helper 分支，识别 C5-M10 advanced sweep fixtures，构造 spine/profile/support/location/auxiliary shape，调用 wrapper 并输出 expected。
- `cad-core/fixtures/c5m10/expected/*.freecad.json`：把当前 `known_gap` payload 替换为 FreeCADCmd 采集 payload；diagnostic-only 子项可保留明确 diagnostic metadata。
- `cad-core/tests/test_p8_features.py`：从断言 `expected["known_gap"]["kind"]` 改为断言 `object_fields.advanced` 和 shape summary。
- `cad-core/tests/test_expected_fixtures.py`：确保 C5-M10 advanced expected 被纳入 compare，且 check 模式不会跳过 wrapper helper。
- `cad-core/src/adapters/c_api/c_api.cpp` 与 `cad-core/tests/test_adapters.py`：把 advanced fields 从 `source_backed_known_gap` 晋级 expected-backed，删除 `remaining_gaps=["part_sweep_wrapper_expected_collector"]`。
- `docs/CADCore3.0/capabilities-gap对照表.md` 与 C5 root / package matrices：记录 collector gap 关闭。

## expected schema

每个 wrapper expected 至少包含：

```json
{
  "freecad_version": "...",
  "schema": "cad-core.freecad-expected.v1",
  "shape_summary": {
    "shape_type": "...",
    "solids": 0,
    "shells": 1,
    "faces": 0,
    "edges": 0,
    "vertices": 0,
    "bbox": {},
    "area": 0.0,
    "volume": 0.0
  },
  "object_fields": {
    "helper": "Part.BRepOffsetAPI_MakePipeShell",
    "dto": "PartSweepAdvancedPipeShellDTO",
    "advanced": {}
  },
  "reference": "FreeCADCmd wrapper expected from Part.BRepOffsetAPI_MakePipeShell"
}
```

`object_fields.advanced` 按代表 fixture 写入字段：

- `auxiliary_spine={target, subname, curvilinear, contact}`
- `spine_support={target, subname, set_mode_ok}` 与 `support_mode`
- `binormal=[x,y,z]` 与 `binormal_property`
- `sections[]={profile, location, with_contact, with_correction}`
- `tolerance={tol3d, boundTol, tolAngular}`
- `topo_naming_history=maker_history:pipeshell`
- `builder_status={build_ok, shape_access_ok}` 或等价字段

## 代表 fixtures

| fixture | C5-M11 目标 |
| --- | --- |
| `part-sweep-auxiliary-spine-contract` | 用 wrapper `setAuxiliarySpine` expected 替换 source-backed known_gap，记录 auxiliary metadata 与 shape summary |
| `part-sweep-binormal-contract` | 用 wrapper `setBiNormalMode` expected 替换 known_gap，记录 canonical `Binormal` 和 legacy alias 边界 |
| `part-sweep-support-mode-diagnostics` | 对 valid support metadata 做 wrapper expected；invalid support / mode payload 继续保留 locatable diagnostics |
| `part-sweep-located-profile-contract` | 用 wrapper `add(Profile, Location, WithContact, WithCorrection)` expected 替换 known_gap |
| `part-sweep-tolerance-contract` | 用 wrapper `setTolerance(tol3d,boundTol,tolAngular)` expected 替换 known_gap，记录 tolerance metadata |
| `part-sweep-advanced-combined-contract` | 用同一 helper 组合 auxiliary + located profile + tolerance，证明字段组合不会互相吞 metadata 或 diagnostics |

## 实施顺序

1. S0：冻结当前 live gap、六个 fixture、capability remaining gap 和 delete condition；不改源码。
2. S1：设计并探测 `Part.BRepOffsetAPI_MakePipeShell` wrapper collector，确定 FreeCADCmd 调用方式、shape builder、metadata schema 和失败分流。
3. S2：批量采集六个 expected 并替换 `known_gap` payload；若某个代表场景无法采集，必须写清原因、保留范围和下一批。
4. S3：更新 focused tests 和 capability metadata，把 wrapper fields 从 source-backed known_gap 晋级 expected-backed，清空 `remaining_gaps` 中的 collector 项。
5. S4：同步 C3 capability gap、C5 root matrices、本包局部矩阵和 README；本包队列为空后关闭 root `C5-BLK-1101`。

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M11-PartWorkbenchSweepWrapperExpectedParity主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M11-PartWorkbenchSweepWrapperExpectedParity主线/工作步骤细分 --format markdown
```

实现短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

collector 检查：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/collect_freecad_expected.py --phase c5m10 --check --skip-unsupported
```

阶段收口：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
python3 tools/collect_freecad_expected.py --phase c5m10 --check --skip-unsupported
```

## 收口标准

- 六个 C5-M10 advanced wrapper fixtures 不再以 `known_gap.kind=part_sweep_*_wrapper_oracle_missing` 作为 expected 主体。
- capability metadata 中 advanced wrapper 字段进入 expected-backed 或 expected-backed-with-diagnostics；`remaining_gaps` 不再包含 `part_sweep_wrapper_expected_collector`。
- source-backed known_gap 若仍保留，必须只针对明确无法采集的子场景，并写清 FreeCADCmd 失败证据、下一批范围和不批量关闭的原因。
- native `Part::Sweep` direct property 边界不变，non-goals 不被误删。
- 队列脚本返回空后，才能把本包方案和 S4 步骤改名为 `【已实现】`。
