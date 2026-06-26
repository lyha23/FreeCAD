# C7-M6 P8 Assembly Joint 完整 placement 与 constraint 实现主线

本目录承接 C7-M5 release gate 之后的 P8 Assembly / Joint 后续方向。C7-M6 不重开已经发布的 P8 request-local solver baseline：Fixed / Revolute / Cylindrical / Slider / Ball / Distance / Angle 子集、DistanceType basic / extended geometry、marker placement first slices、`documentObjectUpdates.action=assembly_set_placement` 和 `cad-core/fixtures/c3m6/expected` native placement oracle 仍作为当前基线。

C7-M6 的目标是围绕完整 Joint placement / constraint、复杂 placement chain、remaining JointType 和 request-local writeback 语义，先以 FreeCAD source 和 checked-in native expected 证明候选范围，再决定是否打开 `cad-core` implementation gate。S6 release gate 已关闭：没有 `backend_gap_requires_implementation`，本包没有 C++ implementation。

## 入口

- 主线总入口：`6-26-12-05-C7-M6-P8AssemblyJoint完整placement与constraint实现主线总入口.md`
- 方案：`6-26-12-05-C7-M6-P8AssemblyJoint完整placement与constraint实现方案.md`
- 工作步骤总入口索引：`工作步骤细分/6-26-12-05-【已实现】C7-M6工作步骤总入口.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- S0 live 基线已冻结：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=60876b2f6c`（`60876b2f6c docs: 完成 C7-M5 S6 release gate`）。开始时 `git status --short -uall` 只包含 `docs/CADCore7.0/README.md` modified 和本 C7-M6 文档包 untracked 文件，未发现无关 dirty work。
- C7-M1/C7-M2/C7-M3/C7-M4/C7-M5 `工作步骤细分` 队列均为空；C7-M5 final route 为 `expected-backed closed / no backendGap`。
- CAD Core 总览后续队列当前指向 P8 扩展：完整 Joint placement / constraint、Worker / WASM / Web adapter、导入 shape 完整 ElementMap、`ShowElement=true` LinkElement / LinkGroup 持久写回事务生命周期、完整 cross-document 文档哈希 / postfix 生命周期、更复杂多层 LinkSub 链，以及 Part surface full family。
- C7-M6 只取 P8 Assembly / Joint 方向，不把 Part surface、Link lifecycle、Worker / WASM / Web 混入同一实现批次。
- S0 已关闭 `C7M6-BLOCKER-000` / `C7M6-GATE-000`；S1 已关闭 `C7M6-BLOCKER-101`；S2 已关闭 `C7M6-BLOCKER-201`；S3 已关闭 `C7M6-BLOCKER-301`。S3 新增 `assembly-marker-custom-placement-chain-real-solver` native expected；`assembly-angle-zero-and-signed-current-real-solver` 因 collector 缺 zero Angle fallback class/evidence 记录 `native_oracle_blocked`；`offsetPlc` 因 native lifecycle 只存在于 `preDrag()` / bundled fixed path 记录 `native_oracle_blocked`。
- S4 已关闭 `C7M6-BLOCKER-401`：`C7M6-ORACLE-202` 以 current `cad-core` legacy recompute 和 focused P8 unittest 裁为 `already_closed_expected_backed`，S3 expected 里的 marker `backendGap` 只是历史 collector 元数据；`C7M6-ORACLE-302` / `C7M6-ORACLE-203` 继续 `oracle_blocked`。
- S5 已关闭 `C7M6-BLOCKER-501` / `C7M6-GATE-601`：implementation gate closed，没有 `backend_gap_requires_implementation`，本轮只做 README / 方案 / 总入口 / 工作步骤 / 矩阵 no-code publication closure；未改 C++、adapter、tests、fixtures、expected、collector 或生成输出。
- S6 已关闭 `C7M6-BLOCKER-601` / `C7M6-GATE-701`：release route 为 `C7M6-ORACLE-202=already_closed_expected_backed`、`C7M6-ORACLE-302=oracle_blocked`、`C7M6-ORACLE-203=oracle_blocked`；C7-M6 队列为空。S6 只更新发布文档和矩阵，没有新增 C++、adapter、tests、fixtures、expected、collector、capability 或生成输出改动。
- `cad-core/fixtures/c3m6/expected` 当前有 52 个 JSON，其中 51 个 Assembly expected；45 个 Assembly expected 无 `known_gap` / `backendGap`，6 个 Assembly expected 带已知 gap / non-goal 元数据，其中 5 个 DistanceType default / TODO / PointCurve expected 保持 `DTE-NG-003` diagnostic boundary，1 个 `assembly-marker-custom-placement-chain-real-solver` 带 S3 collector marker parity `backendGap` 历史元数据且已在 S4 裁为 current cad-core expected-backed closed。35 个 expected 带 `native_marker_oracle`，覆盖 subshape marker / handleOneSide evidence；这些 expected 均不是从 current `cad-core` 输出倒推。
- `cad-core/tests/test_p8_features.py` 的 c3m6 focused coverage 已覆盖 grounded JointType matrix、DistanceType basic / extended / default diagnostic boundary、marker native oracle expected batch、single / multi-component `assembly_set_placement` writeback、invalid grounded、ungrounded、unsupported diagnostics、RackPinion / Screw sliding precondition 与 marker rewrite；S3 只能采集 S2 已选 source-backed oracle candidates 或记录 blocker / diagnostic non-goal。

## 收口边界

- 先证明 FreeCAD source authority 和 native oracle，再比较 current `cad-core`；不得从当前 `cad-core` 输出倒推 expected。
- 只处理 Assembly Joint placement / constraint、marker placement、JointType mapping、DistanceType / scalar evidence 和 request-local placement writeback。
- 不处理 GUI / ViewProvider / Workbench 交互，不引入跨请求 solver session，不保存后端文档状态，不实现 full Link lifecycle、Worker / WASM / Web 或 full Part surface family。
- 如果 S3/S4 不能取得 source-backed native oracle 或 mismatch，必须发布为 `oracle_blocked`、`diagnostic_non_goal` 或 `already_closed_expected_backed`，不打开 C++ 实现。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线 docs/CADCore7.0/README.md
git diff --check
```
