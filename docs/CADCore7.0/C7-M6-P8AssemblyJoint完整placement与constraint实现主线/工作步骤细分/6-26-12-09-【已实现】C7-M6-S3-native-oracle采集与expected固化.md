# 【已实现】C7-M6 S3 native oracle 采集与 expected 固化

## 目标

按 S2 候选批次采集 FreeCAD native oracle，或记录 native oracle blocker / diagnostic non-goal。S3 可以新增 oracle fixture / expected / known_gap；不改 runtime C++ 主路径。

## 完成结论

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=7c98548fd1`（`7c98548fd1 文档：完成 C7-M6 S2 oracle 候选矩阵`），开始状态干净。
- `C7M6-ORACLE-202` 已采集为 `native_oracle_collected`：新增 `cad-core/fixtures/c3m6/assembly-marker-custom-placement-chain-real-solver.json`，并用 FreeCAD native collector 生成 `cad-core/fixtures/c3m6/expected/assembly-marker-custom-placement-chain-real-solver.freecad.json`。expected 记录 `freecad_version=1.2.0 revision 20260519`、`solver_adapter.status=solved`、`mode=real_ondsel_solver`、1 条 `assembly_set_placement` writeback、非 identity `Placement1/2` connector、object-global / part-local / marker placement evidence。collector 自动写入的 `backendGap` 只是 marker parity 元数据，S4 仍是唯一 implementation gate。
- `C7M6-ORACLE-302` 已记录为 `native_oracle_blocked`：新增 `cad-core/fixtures/c3m6/assembly-angle-zero-and-signed-current-real-solver.json`，native probe 成功运行到 `solver_adapter.status=solved`，并能观察 `SignedDistanceJoint` 的负 signed current value `-4.190763653560053`；但 collector 输出缺少 S2 要求的 zero Angle fallback `solver_joint_class` / fallback evidence，未提交不完整 expected。删除 / reopen 条件：collector 或 source-backed native probe 能暴露 `AssemblyObject::makeMbdJointOfType()` 对 `Angle=0` 的 `ASMTParallelAxesJoint` fallback 类证据后，重跑 S3 collector 并固化 expected。
- `C7M6-ORACLE-203` 已记录为 `native_oracle_blocked`：源码证明 `offsetPlc` 只在 `AssemblyObject::preDrag()` 设置 `bundleFixed=true` 后由 `getMbDData()` 为 fixed bundle 生成；常规 request-local `solve(False)` collector 无法进入该 drag / bundled lifecycle。删除 / reopen 条件：新增 dedicated native preDrag / bundled fixed probe，能观察非 identity `MbDPartData.offsetPlc` 同时影响 `handleOneSideOfJoint()` marker 和 `setNewPlacements()` writeback。
- S3 没有新增 focused tests，因为 302 / 203 没有完整 native expected，且 202 的 current cad-core parity 属于 S4 裁决；本轮没有改 `cad-core/src/assembly`、adapter、runtime 或 current cad-core output。
- `C7M6-BLOCKER-301` 已关闭，队列推进到 S4；S4 只能对 202 做 current cad-core parity，对 302 / 203 继承 `oracle_blocked`，不得把 S3 blocker 直接改成 backend implementation gap。

## 必读文件

- S2 完成后的 C7-M6 README、方案和矩阵。
- `cad-core/tools/collect_freecad_expected.py`
- S2 指定的 fixture / expected / focused test 文件。
- S1 记录的 FreeCAD source authority。

## 执行要点

1. 记录 live baseline 和 C7-M6 queue。已完成。
2. 按 S2 的 oracle plan 执行 collector 或 probe。202 target collector 成功；302 temp probe 成功但字段不完整；203 source probe blocked。
3. 如果采到 native oracle，expected 必须记录 FreeCAD version、placement / constraint evidence、solver DTO evidence、writeback summary 和 source authority。202 已满足。
4. 如果无法证明 native lifecycle，写 known_gap 和删除条件。302 / 203 已在本文件、方案和矩阵记录 `native_oracle_blocked` 与 reopen 条件。
5. 如果明确超出无状态 CAD Core 边界，写 diagnostic non-goal。本轮没有新增 diagnostic non-goal。
6. 更新 S3 相关矩阵和方案。已完成。
7. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S4。已完成。

## 合法产物

- 可以新增或更新 `cad-core/fixtures/c3m6/*assembly*` / `*joint*` 相关 fixture。
- 可以新增或更新 `cad-core/fixtures/c3m6/expected/*.freecad.json`。
- 可以新增 focused oracle tests。
- 不允许改 `cad-core/src/assembly`、adapter 或 runtime 主路径。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线 docs/CADCore7.0/README.md
git diff --check
```

S3 具体 FreeCADCmd / unittest 命令以 S2/S3 矩阵记录为准。

## 完成标准

- 每个 S2 oracle candidate 都有 native oracle、native blocker 或 diagnostic non-goal 结论。
- S3 不改 C++ runtime 主路径。
- 队列推进到 S4。
