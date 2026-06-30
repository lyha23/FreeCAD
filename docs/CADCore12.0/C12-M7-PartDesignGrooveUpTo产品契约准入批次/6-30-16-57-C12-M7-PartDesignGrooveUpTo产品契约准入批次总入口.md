# C12-M7 PartDesign Groove UpTo 产品契约准入批次总入口

## 目标

把下一步实现内容收敛到 `PartDesign::Groove` 的 `UpToFirst` / `UpToFace` exact blocker：先复核 native failure，再决定是否发布 CAD Core product diagnostic contract。

## 当前选择理由

当前不应直接实现 SubShapeBinder CopyOnChange：C12-M5 已证明 native copied-object graph、request-local DTO 和 current mismatch 没有同时成立。也不应重开 RuledSurface wire/wire：C12-M6 已关闭为 `wire_wire_admitted_current_supported`。

`part_design.revolution_groove` 的 `partdesign_groove_upto_brepfeat_cut_native_failure` 是 live capability 中最明确的下一项 exact narrowed gap。它已有两条 fixtures、FreeCAD source authority、current CAD Core exact diagnostics 和 future product-contract reopen condition，因此适合进入 C12-M7。

S0 live 冻结进一步确认：C12-M1..M6 队列均为空；`part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache` 仍是 C12-M5 retained diagnostic / `oracle_blocked`，不能从 remaining gap 直接变成实现任务；`part_workbench.ruled_surface` 已是 `supported_wire_wire_expected_backed` 且无 remaining gap；`part_design.revolution_groove` 无 active `remaining_gaps`，但保留 exact narrowed gap `partdesign_groove_upto_brepfeat_cut_native_failure`，适合进入 S1 native/current evidence 复核。

## 执行规则

1. 每步开始前执行 live baseline：`pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git -c core.quotepath=false status --short -uall`。
2. S0-S2 默认只改 C12-M7 文档 / 矩阵，不改 `cad-core`。
3. S3 只有在 S2 批准 product diagnostic contract 后，才允许改 expected/test/capability/docs；仍不得改几何 C++。
4. 只有 FreeCAD native baseline 反转成功且 current mismatch 成立，才把 geometry C++ implementation 作为后续包，不在本包直接实现。
5. 不从 fixture 输出倒推业务逻辑，不用 bbox、输出顺序、fixture 名称或 adapter 补猜替代 FreeCAD / CAD Core source authority。

## 顺序

- S0：live 基线与候选冻结。
- S1：FreeCAD native failure 与 current diagnostic 复核。
- S2：product diagnostic contract 准入裁决。
- S3：expected / test / capability / docs 迁移实现。
- S4：focused validation 与发布边界复核。
- S5：发布闸门与后续分流。

## 当前闭合状态

- 工作步骤总入口已核对为队列入口：只定义 S0-S5 的执行规则、顺序和验收命令，与 README、方案和矩阵一致。
- S0 live 基线与候选冻结已完成：`HEAD=bb69e61a0f`，起点 worktree clean，C12-M1..M6 队列均为空，capability 三段摘录已记录。
- S1 native failure 与 current diagnostic 复核已完成：`HEAD=fdeea2443e` 起点 clean；FreeCAD 调用链已定位到 `Groove::execute -> executeRevolved(CutFromBase) -> Revolved::tryExecuteRevolved -> tryToRevolveToFace -> TopoShape::makeElementRevolution -> BRepFeat_MakeRevol`；C51X native evidence 仍记录 FreeCADCmd 1.2.0 revision 20260519 下两个 Groove UpTo fixtures 报 `Groove: Revolution: Up to face: Could not revolve the sketch!`；current focused test 继续断言 `BRepFeat_MakeRevol could not revolve profile up to face` / `Could not revolve the sketch`。
- S2 product diagnostic contract 准入裁决已完成：`HEAD=cc9e3a1190` 起点 clean；FreeCAD parity success 当前不成立，historical native failure 继续保留为 native evidence，CAD Core product diagnostic contract 已批准。S3 可按 approved 口径迁移两个 Groove UpTo expected、expected-backed focused assertion、capability wording、adapter assertion 和 C12-M7 docs / 矩阵；仍不得改几何 C++。
- S3 expected / test / capability / docs 迁移实现已完成：两个 C51M1 Groove UpTo expected 均记录 `freecad_native_parity=false` 的 product diagnostic contract，focused test 读取 expected 并保留 exact diagnostic/status 断言，capability 与 adapter assertion 发布 `product_diagnostic_contract_non_parity` 且保留 native failure note。下一步是 S4 focused validation 与发布边界复核；S4-S5 尚未标记完成。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M7-PartDesignGrooveUpTo产品契约准入批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M7-PartDesignGrooveUpTo产品契约准入批次/矩阵/*.tsv
git diff --check
```
