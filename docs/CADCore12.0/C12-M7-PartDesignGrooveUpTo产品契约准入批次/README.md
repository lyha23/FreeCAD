# C12-M7 PartDesign Groove UpTo 产品契约准入批次

C12-M7 用于裁决 `PartDesign::Groove` 的 `Type=UpToFirst` / `Type=UpToFace` 是否应从 historical native failure 升级为 CAD Core product diagnostic contract，或继续保留为 historical native failure。

## 为什么是下一步

- C12-M1..M6 队列均已关闭。
- live capability 中唯一 active `remaining_gaps` 仍是 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`，但 C12-M5 已关闭为 `no_code_retained_diagnostic`，不能直接实现。
- `part_design.revolution_groove` 当前没有 active `remaining_gaps`，但有精确 `narrowed_gaps.partdesign_groove_upto_brepfeat_cut_native_failure`，且 capability 的 delete condition 明确允许未来包批准 CAD Core non-parity product contract。
- 该 gap 已有 exact fixtures、focused diagnostic test、FreeCAD source authority 和 current CAD Core diagnostic，比继续重开 broad PartDesign / Part Workbench 更适合作为下一轮最小完整语义批次。

## 本包目标

1. 复核 FreeCAD native baseline 是否仍失败：`Groove: Revolution: Up to face: Could not revolve the sketch!`。
2. 若 native 仍失败，裁决是否把 current CAD Core exact diagnostic 发布为 product diagnostic contract。
3. 若 product contract 被批准，补齐 expected/test/capability/docs 的公开口径，而不是几何 C++。
4. 只有 FreeCAD native baseline 反转成功，且 current CAD Core 与 stable expected mismatch，才另开 geometry implementation candidate。

## 入口

- 总入口：`6-30-16-57-C12-M7-PartDesignGrooveUpTo产品契约准入批次总入口.md`
- 方案：`6-30-16-57-C12-M7-PartDesignGrooveUpTo产品契约准入批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前执行状态

- 工作步骤总入口已核对闭合：`工作步骤细分/6-30-16-58-【已实现】C12-M7工作步骤总入口.md` 仅定义 S0-S5 顺序、执行规则和验收命令。
- 当前 live 队列应从 S0 `工作步骤细分/6-30-16-59-C12-M7-S0-live基线与候选冻结.md` 开始；S0-S5 仍保持 pending，未提前关闭。

## 预期出口

| 出口 | 含义 |
| --- | --- |
| `product_diagnostic_contract_published` | FreeCAD native 仍失败；CAD Core exact diagnostic 被批准为 product diagnostic contract，并完成 expected/test/capability/docs 公开口径。 |
| `retained_historical_native_failure` | native 仍失败，但 product contract 证据或发布面不足；继续保留 narrowed gap。 |
| `implementation_candidate_required` | native 成功且 current CAD Core mismatch，可另开 geometry implementation package。 |
| `no_code_retained` | 证据不足或边界不批准，不改代码和 expected。 |

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M7-PartDesignGrooveUpTo产品契约准入批次/工作步骤细分 --format markdown
```
