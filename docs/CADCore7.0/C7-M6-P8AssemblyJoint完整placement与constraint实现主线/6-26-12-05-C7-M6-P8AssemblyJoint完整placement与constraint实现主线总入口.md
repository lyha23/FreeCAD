# C7-M6 P8 Assembly Joint 完整 placement 与 constraint 实现主线总入口

## 结论

C7-M6 是 C7-M5 之后的 P8 follow-up。总览后续队列已经把 P7 transformed / pattern 复杂 ownership 关闭为 expected-backed / no backendGap，下一类值得推进的实现方向是 P8 Assembly Joint 的完整 placement / constraint 与复杂 placement chain。

当前默认 gate closed：P8 已有 Fixed / Revolute / Cylindrical / Slider / Ball / Distance / Angle request-local real Ondsel adapter 子集和 c3m6 native expected；S0 已进一步确认 c3m6 checked-in expected / focused tests 还覆盖 Parallel、Perpendicular、Gears、Belt、RackPinion、Screw、marker native oracle、DistanceType extended / default diagnostic boundary 和 multi-component writeback。C7-M6 不能凭“完整 Joint”直接改 C++。S1-S2 必须继续复核 live source、fixtures、expected 和 tests，在已覆盖边界之外形成可采 native oracle 的最小完整语义批次。S4 只有在 source-backed native oracle 证明 current `cad-core` mismatch 时，才允许把 S5 转成 implementation。

## 上游状态

- C7-M5 release gate 已完成，最终 route=`expected-backed closed / no backendGap`。
- `docs/CADCore方案/细化方案/00-CAD-Core完整抽取执行总览.md` 的后续队列当前指向 P8：完整 Joint placement / constraint、Worker / WASM / Web adapter、导入 shape 完整 ElementMap、完整 Link lifecycle、Part surface full family。
- `docs/CADCore方案/细化方案/11-P8-Part导入导出与Assembly后续.md` 已声明 Assembly solver native FreeCAD placement oracle 入库并验收，但完整 Joint placement / constraint 和复杂 placement chain 仍未迁移。
- `cad-core/fixtures/c3m6/expected` 已保存多个 Assembly solver placement expected；这些文件是后续 parity 的标准答案，不能用 current `cad-core` 输出倒推。
- S0 live 基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=60876b2f6c`（`60876b2f6c docs: 完成 C7-M5 S6 release gate`）。开始状态只包含 root `docs/CADCore7.0/README.md` modified 和本 C7-M6 文档包 untracked 文件。C7-M1 到 C7-M5 队列均为空。
- 当前 c3m6 expected/test 边界：51 个 checked-in expected JSON，其中 50 个 Assembly expected；45 个 Assembly expected 无 `known_gap` / `backendGap`，5 个 DistanceType diagnostic expected 保持 `DTE-NG-003`；34 个 expected 带 `native_marker_oracle`。`test_p8_features.py` 已把这些 expected 约束到 real Ondsel solver DTO、marker placement、DistanceType、writeback 和 diagnostics。
- S1 已完成 source / current coverage 复核：`AssemblyObject::solve()`、`handleOneSideOfJoint()`、`makeMbdJointOfType()`、`AssemblyUtils::getDistanceType()` / `getJointCurrentValue()`、`JointObject.py`、current `cad-core/src/assembly/*`、c3m6 expected 和 focused tests 均已矩阵化；`C7M6-BLOCKER-101` 已关闭。未采 oracle，未新增或修改 fixtures/expected/tests，未改 C++。

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
4. S3：采集 native oracle 或记录 oracle blocker / diagnostic non-goal。当前下一步。
5. S4：用 current `cad-core` 做 parity 和 implementation gate 裁决。
6. S5：实现正式 Assembly Joint placement / constraint gap，或 no-code 发布收口。
7. S6：release gate，更新 README / 矩阵 / P8 口径并清空队列。

## 验收入口

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线 docs/CADCore7.0/README.md
git diff --check
```
