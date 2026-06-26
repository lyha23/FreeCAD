# C7-M6 P8 Assembly Joint 完整 placement 与 constraint 实现方案

## 背景

C7-M5 已经把 P7 transformed / pattern 复杂 ownership 收口为 expected-backed closed / no backendGap。CAD Core 总览的下一类开放方向是 P8 扩展，其中 Assembly Joint 已有 request-local real Ondsel adapter、固定的 c3m6 native expected 和一批 focused tests，但完整 Joint placement / constraint、复杂 placement chain、remaining JointType 与产品化边界仍未收口。

C7-M6 的核心不是重写 Assembly solver，而是按 FreeCAD source authority 和 checked-in native expected 审计 current `cad-core` 是否还有真实 mismatch。没有 source-backed native oracle 时，只能保持 `oracle_pending` / `oracle_blocked` / `diagnostic_non_goal`，不能直接改 C++。

## 目标

- 从 P8 live 文档、FreeCAD Assembly source、current `cad-core` assembly 实现和 c3m6 fixtures/tests 中抽出 Joint placement / constraint 候选。
- 形成最小完整语义批次：source authority、fixture / expected 候选、collector 命令、focused tests、capability / docs 发布口径。
- 若 native oracle 证明 current `cad-core` mismatch，打开 S5 implementation gate。
- 若 current `cad-core` 已匹配，发布 `already_closed_expected_backed`。
- 若缺 oracle 或 lifecycle 不可复现，发布 `oracle_blocked` 或 `diagnostic_non_goal`，不改 C++。

## 最小完整语义批次

| 批次 | 代表项 | 判定方式 |
| --- | --- | --- |
| Baseline Joint placement | GroundedJoint、Fixed / Revolute / Cylindrical / Slider / Ball / Distance / Angle request-local placement writeback | c3m6 native expected + current P8 focused tests |
| Marker placement chain | face / edge / vertex marker、object-global 到 part-local 变换、`Placement1/2`、offset marker | FreeCAD `AssemblyObject::handleOneSideOfJoint()` + marker fixtures |
| Remaining JointType constraints | Parallel、Perpendicular、RackPinion、Screw、Gears、Belt、Angle zero / nonzero | FreeCAD `makeMbdJointOfType()` + current `makeOndselJointOfType()` evidence |
| Distance / current value scalar | DistanceType、radius-bearing geometry、signed distance / angle、`getJointCurrentValue()` | FreeCAD `AssemblyUtils::getDistanceType()` / `getJointCurrentValue()` + solver DTO expected |
| Writeback lifecycle | multi-component placement, `assembly_set_placement`, unsupported diagnostics | c3m6 expected + `documentObjectUpdates` tests |

## 实施纪律

- S0/S1 不改 C++、fixtures、expected 或 tests；只冻结状态、source 和 current coverage。
- S2 只输出 oracle 候选和最小批次；不能把 existing expected-backed rows 误写成 active gap。
- S3 可以新增 oracle fixture / expected / known_gap，但不能从 current `cad-core` 输出倒推 expected。
- S4 只做 parity 和 gate 裁决；只有 route=`backend_gap_requires_implementation` 才打开 S5 code edit gate。
- S5 若实现，必须落到正式 `assembly/*`、`document` parser 或 C ABI capability 路径，不允许 adapter 输出端修正、fixture 名称分支或 solver result 后处理特判。

## 步骤

### S0 live baseline 与 P8 边界冻结

已完成。冻结 live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=60876b2f6c`（`60876b2f6c docs: 完成 C7-M5 S6 release gate`）；开始状态只包含 root `docs/CADCore7.0/README.md` modified 和本 C7-M6 文档包 untracked 文件，未发现无关 dirty work。C7-M1..M5 队列均为空。

S0 已冻结 P8 Assembly / Joint 已发布边界：`cad-core/fixtures/c3m6/expected` 有 51 个 checked-in JSON，其中 50 个 Assembly expected；45 个 Assembly expected 无 `known_gap` / `backendGap`，5 个 DistanceType default / TODO / PointCurve expected 保持 `DTE-NG-003` diagnostic boundary；34 个 expected 带 `native_marker_oracle`。`cad-core/tests/test_p8_features.py` 已覆盖 grounded JointType matrix、DistanceType basic / extended / default diagnostic boundary、marker native oracle expected batch、single / multi-component `assembly_set_placement` writeback、invalid grounded、ungrounded、unsupported diagnostics、RackPinion / Screw sliding precondition 与 marker rewrite。S0 没有采 oracle、没有新增 fixture/expected/test、没有改 C++。

### S1 FreeCAD source 与 current coverage 复核

已完成。S1 只更新文档和矩阵，没有新增 fixtures/expected/tests，没有运行 FreeCAD oracle，没有改 C++。

FreeCAD source authority 已复核：

- `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::solve()`：request-local 顺序为 `ensureIdentityPlacements()`、`syncGroundedJoints()`、`makeMbdAssembly()`、`fixGroundedParts()`、`jointParts()`、`mbdAssembly->runPreDrag()`、`setNewPlacements()`、`redrawJointPlacements()`。
- `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::handleOneSideOfJoint()`：`Placement1/2` 先经 object-global，再转 moving part local，最后按 bundled `offsetPlc` 修正后创建 Ondsel marker。
- `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::makeMbdJointOfType()` / `makeMbdJointDistance()`：确认 Fixed、Revolute、Cylindrical、Slider、Ball、Distance、Parallel、Perpendicular、Angle、RackPinion、Screw、Gears、Belt 到 ASMT joint class 和 scalar 字段的映射。
- `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` / `getJointCurrentValue()`：确认 DistanceType 分类、`swapJCS()` solver ordering、radius correction、signed distance / angle current value 和 default / TODO 边界。
- `src/Mod/Assembly/JointObject.py`：确认 `JointType` 枚举、`Reference1/2`、`Placement1/2`、`Distance` / `Distance2`、`ObjectToGround` 和 GUI/session-only 行为边界。

current `cad-core` coverage 已复核：`buildAssemblySolveRequest()` 构造 request-local solver DTO，`solveAssemblyWithRealOndselAdapter()` 使用真实 Ondsel adapter，`makeOndselJointOfType()` 已覆盖当前 supported JointType / DistanceType 映射，`assembly_set_placement` 通过 `documentObjectUpdates` 发布 request-local writeback，unsupported / missing marker / default DistanceType 保持 diagnostic boundary。

PointLine 保留 source / native expected 差异边界：当前源码 `makeMbdJointDistance()` 写成 `ASMTCylSphJoint`，但已入库 FreeCADCmd 1.2.0 expected 和 current focused tests 对 request-local AssemblyLink PointLine 子集约束为 `ASMTLineInPlaneJoint` + `offset`。S2 若要重开该类，必须先拿到新的 source-backed native oracle，不能只凭源码 / current output 任一侧改口径。

fixtures / tests 已复核：`cad-core/fixtures/c3m6/expected` 当前 50 个 Assembly expected 中 45 个无 `known_gap` / `backendGap`，5 个保留 `DTE-NG-003` diagnostic boundary，34 个带 `native_marker_oracle`；`cad-core/tests/test_p8_features.py` 覆盖 grounded JointType matrix、DistanceType basic / extended / default boundary、marker native oracle、single / multi / partial writeback、invalid grounded、ungrounded、unsupported diagnostics、Screw / RackPinion sliding precondition 与 marker rewrite。

`C7M6-BLOCKER-101` 已关闭。S2 只能把上述已覆盖边界外、同时具备 FreeCAD source-backed request-local lifecycle 的内容列为 oracle candidate；已覆盖 expected-backed rows 不得重开。

### S2 oracle 候选矩阵与批次裁决

已完成。S2 只更新文档和矩阵，没有新增 fixtures/expected/tests，没有运行 FreeCAD oracle，没有改 C++。

S2 route 裁决：

| route | 范围 | S3 动作 |
| --- | --- | --- |
| `already_covered` | grounded JointType matrix、DistanceType basic / extended expected、DTE-NG-003 default / TODO diagnostic boundary、marker native oracle batch、single / multi / partial / next-request `assembly_set_placement` writeback | 保留 checked-in expected，不重采、不重开 |
| `oracle_candidate` | `assembly-marker-custom-placement-chain-real-solver`：非 identity `Placement1/2` connector 和 object-global 到 part-local marker chain | S3 创建 fixture，用 `collect_freecad_expected.py` 采 FreeCAD native expected，新增 focused test 名称见 `oracle_plan.tsv` |
| `oracle_candidate` | `assembly-angle-zero-and-signed-current-real-solver`：Angle zero fallback 与 placement-derived signed Distance / current value evidence | S3 创建 fixture，用 `collect_freecad_expected.py` 采 FreeCAD native expected，新增 focused test 名称见 `oracle_plan.tsv` |
| `oracle_blocker` | bundled `offsetPlc` 同时影响 `handleOneSideOfJoint()` marker 和 `setNewPlacements()` writeback，但当前 c3m6 只证明 identity offset boundary | S3 先做 source-backed probe；若无法构造 native lifecycle，记录 `native_oracle_blocked`，不得打开 implementation gate |
| `diagnostic_non_goal` | GUI / drag / persistent solver / cross-request backend state / full Link lifecycle / Worker / WASM / Web adapter | 不采 native golden，不进入 S4 backend gap |

S2 没有 `backend_gap_candidate`。S4 仍是第一个可以把 native oracle mismatch 裁成 `backend_gap_requires_implementation` 的步骤；S5 code gate 继续关闭。

把候选路由到：

- `already_covered`
- `oracle_candidate`
- `oracle_blocker`
- `backend_gap_candidate`
- `diagnostic_non_goal`

S2 已明确每个候选是否有 source-backed FreeCAD lifecycle、是否已有 checked-in expected、是否只是 request-local diagnostic 或 GUI/session 行为；细节以 `矩阵/c7m6_p8_assembly_joint_oracle_plan.tsv`、`scope.tsv`、`backend_gate.tsv` 和 `blocker_queue.tsv` 为准。

### S3 native oracle 采集

对 S2 的 `oracle_candidate` 批次采集 FreeCAD expected。合法结果：

| route | 条件 | 输出 |
| --- | --- | --- |
| `native_oracle_collected` | FreeCAD native fixture 可复现 placement / constraint evidence | expected / evidence JSON |
| `native_oracle_blocked` | collector、FreeCADCmd、solver runtime 或 lifecycle 不可观察 | known_gap JSON |
| `diagnostic_non_goal` | GUI / drag session / persistent solver / unsupported child type 等超边界 | diagnostic expected 或 docs row |

已完成。S3 新增 `cad-core/fixtures/c3m6/assembly-marker-custom-placement-chain-real-solver.json` 与 `cad-core/fixtures/c3m6/expected/assembly-marker-custom-placement-chain-real-solver.freecad.json`，FreeCAD collector 在 `freecad_version=1.2.0 revision 20260519` 上采到 `solver_adapter.status=solved`、1 条 `assembly_set_placement` writeback、非 identity `Placement1/2` connector、object-global / part-local / marker placement evidence，route=`native_oracle_collected`。collector 自动写入的 marker parity `backendGap` 只是 expected 元数据，S4 仍是 implementation gate。

S3 同时新增 `cad-core/fixtures/c3m6/assembly-angle-zero-and-signed-current-real-solver.json`，但 native probe 输出虽有 `SignedDistanceJoint` 负 signed current value `-4.190763653560053` 和 writeback，仍缺 S2 required 的 zero Angle fallback `solver_joint_class` / fallback evidence；未提交不完整 expected，route=`native_oracle_blocked`。`offsetPlc` 也保持 `native_oracle_blocked`：源码显示非 identity `MbDPartData.offsetPlc` 只在 `preDrag()` 设置 `bundleFixed=true` 后由 fixed bundle lifecycle 产生，当前 request-local collector 无法进入该生命周期。

### S4 cad-core parity 与 implementation gate

如果 S3 得到 native oracle，则比较 current `cad-core`：

- 匹配：`already_closed_expected_backed`
- 不匹配：`backend_gap_requires_implementation`
- 缺 oracle：`oracle_blocked`
- 超边界：`diagnostic_non_goal`

S4 必须写清 S5 是否允许改 C++、允许文件范围、focused tests 和 non-goals。

已完成。S4 从 `HEAD=cd3c93b873` 干净起点执行，未改 C++、adapter、tests、fixtures 或 expected。`C7M6-ORACLE-202` 的 current `cad-core` legacy recompute 对 S3 FreeCAD expected 裁为 `already_closed_expected_backed`：去掉 S3 collector 预写的 `known_gap/backendGap` 元数据后 expected comparator 通过，`documentObjectUpdates` placement update 最大误差约 `4.44e-16`，`reference1/reference2` marker placement 最大误差分别约 `2.22e-16` / `1.11e-16`，`solver_adapter.status/mode/joints/unsupported_joints` 一致。`CadCoreP8FeatureTest` 215 tests OK，C API capability 显示 `ondsel_solver_adapter.available=true`、`subshape_marker_placement.remaining_gaps=[]`、`placement_writeback.remaining_gaps=[]`。

`C7M6-ORACLE-302` 和 `C7M6-ORACLE-203` 保持 `oracle_blocked`：302 仍缺 zero Angle fallback `solver_joint_class` / fallback evidence；203 仍缺 dedicated native `preDrag()` / bundled fixed lifecycle evidence。S5 implementation gate closed，只允许 no-code publication closure。S5 允许文件限定为 root README、本包 README、方案、工作步骤总入口 / S5 / S6 文档和本包 `矩阵/*.tsv`；不得修改 `cad-core/src/assembly/*`、adapter、tests、fixtures 或 expected。

### S5 实现或 no-code 发布

若 S4 打开 code gate，S5 实现顺序固定：

1. 在 `cad-core/src/assembly` 正式路径补 Joint placement / constraint / marker / writeback 语义。
2. 写 focused tests，约束 solver DTO、`documentObjectUpdates`、diagnostics、capability 和 expected parity。
3. 删除临时 diagnostic 或保持 known_gap 时同步矩阵。

若 S4 未打开 code gate，S5 只做 no-code publication closure。

已完成。S4 裁决已经关闭 implementation gate，因此 S5 没有做 C++ implementation；只同步 README / 方案 / 总入口 / 工作步骤 / 矩阵发布口径，未修改 `cad-core/src`、adapter、tests、fixtures、expected、collector 或生成输出。`C7M6-ORACLE-202` 保持 `already_closed_expected_backed`；`C7M6-ORACLE-302` 继续等待 zero Angle fallback `solver_joint_class` / fallback evidence；`C7M6-ORACLE-203` 继续等待 dedicated native `preDrag()` / bundled fixed lifecycle probe。`C7M6-BLOCKER-501` 已关闭，`C7M6-GATE-601` 发布为 no-code closure，队列推进到 S6 release gate。

### S6 release gate

已完成。S6 运行本包 queue、TSV、trailing whitespace、`git diff --check`，并同步 root README、本包 README / 总入口 / 方案、矩阵和 P8 细化口径。S5/S6 均未改 C++、adapter、fixtures、expected、tests、collector、capability 或生成输出，因此未跑 focused P8 tests 或 `cmake --build build`。最终 route：`C7M6-ORACLE-202=already_closed_expected_backed`；`C7M6-ORACLE-302=oracle_blocked`，等待 zero Angle fallback `solver_joint_class` / fallback evidence；`C7M6-ORACLE-203=oracle_blocked`，等待 native `preDrag()` / bundled fixed `offsetPlc` lifecycle probe。`C7M6-BLOCKER-601` / `C7M6-GATE-701` 已关闭，C7-M6 队列为空。

## 验收分层

### 本轮短跑

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线 docs/CADCore7.0/README.md
git diff --check
```

### 实现短跑

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

只有 S5 改 C++、expected、tests 或 capability 时，这组才是必须执行项。
