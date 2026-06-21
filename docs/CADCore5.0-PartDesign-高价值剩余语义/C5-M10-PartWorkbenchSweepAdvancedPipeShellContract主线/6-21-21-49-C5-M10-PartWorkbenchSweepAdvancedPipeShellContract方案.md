# C5-M10 Part Workbench Sweep Advanced PipeShell Contract 方案

## 当前基线

`part_workbench.sweep` 当前已经有 C5-M6 live support：multi-profile、`Linearize=true`、基础 `Solid/Frenet/Transition` 由 `cad-core/fixtures/c4m1/part-sweep-multi-profile-linearize.json` 和 expected 覆盖；`cad-core/fixtures/c4m1/part-sweep-advanced-deferred.json` 目前只断言 `AuxiliarySpine`、`Tolerance` 等 advanced 字段会给出 locatable `unsupported_property` diagnostics。

S0 live guard 已在 `cd4a092d9a` 基线冻结：当前 `part_workbench.sweep` capability 仍是 `supported_multi_profile_linearize_expected_backed`，advanced wrapper 字段只作为 diagnostic-backed deferred baseline 记录，不发布新 support，也不关闭 `future_sweep_advanced_contract`。

C5-M10 的目标不是重做基础 Sweep，而是把这个 deferred broad contract 拆成同一 request-local `PartSweepAdvancedPipeShellDTO` 下的一组可实现 / 可诊断场景：AuxiliarySpine、spine support、Binormal、profile location、tolerance 和组合压力。这样一轮能覆盖同一调用链与同一 expected schema，避免长期单 fixture 推进。

## FreeCAD 调用链

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` 读取 `Sections`、`Spine`、`Solid`、`Frenet`、`Linearize`、`Transition`，调用 `result.makeElementPipeShell(shapes, isSolid, isFrenet, transMode, Part::OpCodes::Sweep)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakePipeShell.pyi` 与 `BRepOffsetAPI_MakePipeShellPyImp.cpp` 暴露 advanced wrapper 方法：`setFrenetMode`、`setSpineSupport`、`setAuxiliarySpine`、`add(Profile, Location, WithContact, WithCorrection)`、`setTolerance(tol3d, boundTol, tolAngular)`、`setTransitionMode`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::setupAlgorithm()` 使用同一个 OCCT `BRepOffsetAPI_MakePipeShell` builder，按 `Mode=Fixed/Frenet/Auxiliary/Binormal` 调 `SetMode(...)`，读取 `AuxiliarySpine`、`AuxiliaryCurvilinear`、`Binormal`、`Transformation`、`Transition` 和 `SpineTangent`。
- 因此 C5-M10 的发布口径是：Part Workbench Sweep advanced request-local contract 参考 wrapper 与 PartDesign Pipe 的 builder 语义；native `Part::Sweep` 直接属性仍保持基础六项。

## cad-core 落点

- `cad-core/src/part/part_sweep.cpp`：解析 request-local advanced fields，删除或收窄 `rejectDeferredSweepAdvancedProperties()` 的 broad 拒绝；字段错误必须保留 object/property/target diagnostics。
- `cad-core/include/cad_core/part/topo_shape_expansion.h` 与 `cad-core/src/part/topo_shape_expansion.cpp`：复用现有 `PipeShellOptions`、`PipeShellMode::Auxiliary/Binormal`、`auxiliarySpine`、`auxiliaryCurvilinear`、`binormal`、`transition`、`linearizeFaces` 等能力，补缺失的 support/location/tolerance 表达。
- `cad-core/tools/collect_freecad_expected.py`：新增或扩展 FreeCAD wrapper expected 采集；不能采集的字段写 source-backed known_gap，不得从 cad-core 输出倒推 expected。
- `cad-core/fixtures/c5m10`：按同一 DTO schema 建批量 fixtures，而不是一 case 一套临时字段。
- `cad-core/tests/test_p8_features.py`、`tests/test_expected_fixtures.py`、`tests/test_adapters.py`：覆盖 advanced fields、invalid diagnostics、expected metadata、capability remaining_gaps。
- `cad-core/src/adapters/c_api/c_api.cpp`：只同步 capability/schema/diagnostics，不承接 PipeShell 业务逻辑。

## DTO / API 字段冻结

| 字段组 | 建议字段 | FreeCAD 依据 | C5-M10 策略 |
| --- | --- | --- | --- |
| auxiliary | `AuxiliarySpine`、`AuxiliaryCurvilinear` | wrapper `setAuxiliarySpine`；PartDesign Pipe `Auxiliary` mode | S2 批量实现或 source-backed known_gap，invalid target/subname diagnostic |
| support | `SupportMode` 或 `SpineSupport` | wrapper `setSpineSupport` | S2 先定义 DTO 与 diagnostics；可 oracle 时 expected-backed |
| binormal | `Binormal`（canonical），`BiNormal` 仅保留 legacy deferred alias | PartDesign Pipe `Mode=Binormal` -> `SetMode(gp_Dir(...))` | S2 批量覆盖 valid vector、zero/nonfinite vector diagnostic、combo pressure |
| location | `SectionOptions[].Location`、`SectionOptions[].WithContact`、`SectionOptions[].WithCorrection`，按 `Sections` 顺序匹配 | wrapper `add(Profile, Location, WithContact, WithCorrection)` | S3 覆盖 located profile、contact/correction 与 invalid location payload |
| tolerance | `Tolerance` object -> `tol3d/boundTol/tolAngular`；旧 scalar `Tolerance` 只保留 S0 deferred compatibility | wrapper `setTolerance` | S3 覆盖 valid tolerance metadata 与 malformed/nonfinite diagnostics |
| combination | auxiliary/support/binormal/location/tolerance mixed | same `BRepOffsetAPI_MakePipeShell` builder | S3/S4 验证字段组合不会互相吞 diagnostics 或回退基础 support |

S1 字段级合同以 `矩阵/c5m10_sweep_advanced_pipeshell_source_dto_oracle_contract.tsv` 为准。该矩阵冻结：

- native `Part::Sweep` 直接属性只包括 `Sections`、`Spine`、`Solid`、`Frenet`、`Linearize`、`Transition`；advanced 字段不能写成 upstream native `Part::Sweep` 属性。
- `AuxiliarySpine`、`AuxiliaryCurvilinear`、`SpineSupport`、`SupportMode`、`Binormal`、`SectionOptions[].Location`、`SectionOptions[].WithContact`、`SectionOptions[].WithCorrection`、`Tolerance.tol3d`、`Tolerance.boundTol`、`Tolerance.tolAngular` 都必须有 FreeCAD source 依据、cad-core 落点、expected schema、diagnostic property/target/subname 规则和 known_gap 删除条件。
- 当前 `cad-core/tools/collect_freecad_expected.py` 只对原生 `Part::Sweep` DocumentObject 走 `sweep_payload()`；尚未有 `Part.BRepOffsetAPI_MakePipeShell` wrapper helper 分支。S2/S3 可以批量新增 FreeCADCmd wrapper expected 采集；在该 collector 分支落地前，只能把对应 expected 标成 source-backed known_gap，删除条件是 wrapper helper 返回稳定 shape summary 与字段 metadata。
- FreeCADCmd wrapper expected 的批量采集目标是同一 request-local helper：创建 spine/profile/support/location/auxiliary shape，调用 `setAuxiliarySpine`、`setSpineSupport`、`setBiNormalMode`、`add(Profile, Location, WithContact, WithCorrection)`、`setTolerance(tol3d,boundTol,tolAngular)`、`setTransitionMode`，再 `Build()` / `Shape()` 汇总；不能用 cad-core 输出倒推 expected。

## 代表 fixtures

| 分组 | 目标 fixture | 验收重点 |
| --- | --- | --- |
| live guard | `c4m1/part-sweep-multi-profile-linearize`、`c4m1/part-sweep-advanced-deferred` | 基础 Sweep expected support 不回退；旧 advanced deferred 不再作为 broad gap 留空 |
| mode / support | `c5m10/part-sweep-auxiliary-spine-contract`、`part-sweep-binormal-contract`、`part-sweep-support-mode-diagnostics` | AuxiliarySpine、AuxiliaryCurvilinear、Binormal、SpineSupport 字段解析、builder options、expected/source-backed evidence、locatable diagnostics |
| location / tolerance | `c5m10/part-sweep-located-profile-contract`、`part-sweep-tolerance-contract` | section location、WithContact/WithCorrection、tol3d/boundTol/tolAngular、invalid payload diagnostics |
| combination | `c5m10/part-sweep-advanced-combined-contract` | 同一 DTO 多字段组合、capability metadata、remaining gap 精确化 |

## 实施顺序

1. S0：冻结当前 live baseline、capability 状态、root matrix 与 existing advanced-deferred diagnostics。
2. S1：读 FreeCAD source 与 cad-core 当前实现，写清 DTO / API 字段、oracle 可采路径、expected schema、source-backed known_gap 删除条件和不声明 upstream `Part::Sweep` advanced 属性的边界。
3. S2：实现 AuxiliarySpine / spine support / Binormal 一组 builder mode；补 fixtures、expected 或 source-backed known_gap、diagnostics 和 focused tests。
4. S3：实现 profile location / tolerance / combination 一组字段；补 DTO parser、builder option、diagnostic matrix、fixtures、expected 或 source-backed known_gap。
5. S4：同步 capability/docs/root matrices，关闭 broad `future_sweep_advanced_contract`，保留精确 remaining gaps 与 non-goals。

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M10-PartWorkbenchSweepAdvancedPipeShellContract主线/工作步骤细分 --format markdown
```

实现短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

阶段收口：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

## 收口标准

- 基础 `part_workbench.sweep` C5-M6 live guard 保持 expected-backed。
- AuxiliarySpine、spine support、Binormal、profile location、tolerance 和组合场景至少都有 expected-backed、source-backed known_gap 或 diagnostic-backed 证据，不能只保留 broad unsupported。
- DTO / API 字段与 FreeCAD wrapper / PartDesign Pipe builder 依据清楚；native `Part::Sweep` 属性边界没有过度声明。
- capability metadata 中 `future_sweep_advanced_contract` 不再是一个宽泛 future bucket；剩余项必须有字段级 owner、delete_condition 和 non-goal 说明。
- 本包 `工作步骤细分` 队列为空后才能把 root `C5-BLK-1001` 标记为 done。
