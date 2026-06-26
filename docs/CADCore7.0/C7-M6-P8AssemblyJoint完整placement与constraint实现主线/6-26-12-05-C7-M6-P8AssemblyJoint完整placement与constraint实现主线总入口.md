# C7-M6 P8 Assembly Joint 完整 placement 与 constraint 实现主线总入口

## 结论

C7-M6 是 C7-M5 之后的 P8 follow-up。总览后续队列已经把 P7 transformed / pattern 复杂 ownership 关闭为 expected-backed / no backendGap，下一类值得推进的实现方向是 P8 Assembly Joint 的完整 placement / constraint 与复杂 placement chain。

当前 gate 已关闭：P8 已有 Fixed / Revolute / Cylindrical / Slider / Ball / Distance / Angle request-local real Ondsel adapter 子集和 c3m6 native expected；S0 已进一步确认 c3m6 checked-in expected / focused tests 还覆盖 Parallel、Perpendicular、Gears、Belt、RackPinion、Screw、marker native oracle、DistanceType extended / default diagnostic boundary 和 multi-component writeback。C7-M6 没有凭“完整 Joint”直接改 C++。S1-S2 已复核 live source、fixtures、expected 和 tests，在已覆盖边界之外形成可采 native oracle 的最小完整语义批次；S4 已裁决没有 backend gap，S5 已按 no-code publication closure 完成，S6 release gate 已清空队列。

## 上游状态

- C7-M5 release gate 已完成，最终 route=`expected-backed closed / no backendGap`。
- `docs/CADCore方案/细化方案/00-CAD-Core完整抽取执行总览.md` 的后续队列当前指向 P8：完整 Joint placement / constraint、Worker / WASM / Web adapter、导入 shape 完整 ElementMap、完整 Link lifecycle、Part surface full family。
- `docs/CADCore方案/细化方案/11-P8-Part导入导出与Assembly后续.md` 已同步 C7-M6 release 口径：Assembly solver native FreeCAD placement oracle 和非 identity marker chain 已 expected-backed，zero Angle fallback 与 bundled `offsetPlc` lifecycle 继续 `oracle_blocked`。
- `cad-core/fixtures/c3m6/expected` 已保存多个 Assembly solver placement expected；这些文件是后续 parity 的标准答案，不能用 current `cad-core` 输出倒推。
- S0 live 基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=60876b2f6c`（`60876b2f6c docs: 完成 C7-M5 S6 release gate`）。开始状态只包含 root `docs/CADCore7.0/README.md` modified 和本 C7-M6 文档包 untracked 文件。C7-M1 到 C7-M5 队列均为空。
- 当前 c3m6 expected/test 边界：52 个 checked-in expected JSON，其中 51 个 Assembly expected；45 个 Assembly expected 无 `known_gap` / `backendGap`，6 个 Assembly expected 带已知 gap / non-goal 元数据，其中 5 个 DistanceType diagnostic expected 保持 `DTE-NG-003`，1 个新 marker custom placement expected 带 collector marker parity `backendGap` 元数据；35 个 expected 带 `native_marker_oracle`。`test_p8_features.py` 已把既有 expected 约束到 real Ondsel solver DTO、marker placement、DistanceType、writeback 和 diagnostics；新 S3 expected 的 current cad-core parity 由 S4 裁决。
- S1 已完成 source / current coverage 复核：`AssemblyObject::solve()`、`handleOneSideOfJoint()`、`makeMbdJointOfType()`、`AssemblyUtils::getDistanceType()` / `getJointCurrentValue()`、`JointObject.py`、current `cad-core/src/assembly/*`、c3m6 expected 和 focused tests 均已矩阵化；`C7M6-BLOCKER-101` 已关闭。未采 oracle，未新增或修改 fixtures/expected/tests，未改 C++。
- S4 已完成 parity 与 implementation gate 裁决：`C7M6-ORACLE-202` 裁为 `already_closed_expected_backed`，S3 expected 里的 marker `backendGap` 是历史 collector 元数据；`C7M6-ORACLE-302` / `C7M6-ORACLE-203` 继续 `oracle_blocked`。
- S5 已完成 no-code publication closure：没有 C++ implementation，没有修改 adapter、tests、fixtures、expected、collector 或生成输出；`C7M6-BLOCKER-501` 关闭，`C7M6-GATE-601` 发布为 no-code closure。
- S6 已完成 release gate：`C7M6-BLOCKER-601` 和 `C7M6-GATE-701` 已关闭；C7-M6 队列为空。本轮最终 route 为 `C7M6-ORACLE-202=already_closed_expected_backed`、`C7M6-ORACLE-302=oracle_blocked`、`C7M6-ORACLE-203=oracle_blocked`，没有 `backend_gap_requires_implementation`。

## 初始范围

- `AssemblyObject::solve()` 的 request-local solve 顺序、GroundedJoint 同步、part placement writeback 和 `documentObjectUpdates`。
- `AssemblyObject::handleOneSideOfJoint()` 的 object-global / part-local marker placement、subshape marker、`Placement1/2` 与 `offsetPlc`。
- `AssemblyObject::makeMbdJointOfType()` 和 current `cad-core` `makeOndselJointOfType()` 的 JointType mapping：Fixed、Revolute、Cylindrical、Slider、Ball、Distance、Parallel、Perpendicular、Angle、RackPinion、Screw、Gears、Belt。
- `AssemblyUtils::getDistanceType()` / `getJointCurrentValue()` 的 scalar、radius、signed distance / angle 和 solver DTO evidence。
- multi-component placement writeback、unsupported / diagnostic JointType 和 capability publication。

## 排除项

- GUI、ViewProvider、TaskPanel、drag session、persistent solver session、cross-request cache。
- 完整 Link lifecycle、ShowElement 持久写回事务、cross-document hash / postfix 生命周期。
- Worker / WASM / Web adapter 产品化。
- Part surface full family。
- 用 current `cad-core` 输出刷新 FreeCAD expected。

## 步骤队列

1. S0：冻结 live baseline、C7-M1..M5 队列、P8 当前 supported / expected-backed / diagnostic 边界。已完成，关闭 `C7M6-BLOCKER-000` / `C7M6-GATE-000`。
2. S1：复核 FreeCAD Assembly source、current `cad-core` solver / marker / writeback 能力和 c3m6 fixture/test 覆盖。已完成，关闭 `C7M6-BLOCKER-101`。
3. S2：形成 Joint placement / constraint native oracle 候选矩阵和最小完整语义批次。已完成，关闭 `C7M6-BLOCKER-201`，只保留 `C7M6-ORACLE-202` / `C7M6-ORACLE-302` 为 S3 oracle candidates，`C7M6-ORACLE-203` 为 offsetPlc oracle blocker。
4. S3：采集 native oracle 或记录 oracle blocker / diagnostic non-goal。已完成，关闭 `C7M6-BLOCKER-301`；`C7M6-ORACLE-202` 已采集 native expected，`C7M6-ORACLE-302` / `C7M6-ORACLE-203` 记录 `native_oracle_blocked`。
5. S4：用 current `cad-core` 做 parity 和 implementation gate 裁决。已完成，`C7M6-ORACLE-202=already_closed_expected_backed`，`C7M6-ORACLE-302/203=oracle_blocked`。
6. S5：实现正式 Assembly Joint placement / constraint gap，或 no-code 发布收口。已完成 no-code publication closure，未改 C++ / fixtures / expected / tests。
7. S6：release gate，更新 README / 矩阵 / P8 口径并清空队列。已完成，关闭 `C7M6-BLOCKER-601` / `C7M6-GATE-701`。

## 验收入口

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线 docs/CADCore7.0/README.md
git diff --check
```
