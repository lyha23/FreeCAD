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
- S0 live 基线与候选冻结已完成：执行起点为 `HEAD=bb69e61a0f`（`bb69e61a0f docs: 关闭 C12-M7 工作步骤总入口`），worktree clean，C12-M1..M6 队列均为空。
- `cad-core/build/cad-core capabilities` 摘录显示：`part_design.revolution_groove.remaining_gaps=[]`，`narrowed_gaps.partdesign_groove_upto_brepfeat_cut_native_failure.route=historical_native_failure`；`part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]` 且仍为 `known_gap_diagnostic` / `oracle_blocked`；`part_workbench.ruled_surface.status=supported_wire_wire_expected_backed` 且 `remaining_gaps=[]`。
- S0 冻结结论：下一步仍选择 Groove UpTo exact narrowed gap；不重开 CopyOnChange，因为 C12-M5 已发布 `no_code_retained_diagnostic`；不重开 RuledSurface wire/wire，因为 C12-M6 已发布 `wire_wire_admitted_current_supported`。
- S1 native failure 与 current diagnostic 复核已完成：本轮起点 `HEAD=fdeea2443e`，worktree clean；FreeCAD chain 为 `Groove::execute -> executeRevolved(CutFromBase) -> Revolved::tryExecuteRevolved -> tryToRevolveToFace -> TopoShape::makeElementRevolution -> BRepFeat_MakeRevol`；C51X 旧证据仍采用 `FreeCADCmd 1.2.0 revision 20260519` 下两个 Groove UpTo fixtures 报 `Groove: Revolution: Up to face: Could not revolve the sketch!`，本机 `freecadcmd --version` 也为 `FreeCAD 1.2.0 Revision: 20260519 (Git shallow)`；focused test 确认 current exact diagnostics 仍为 `BRepFeat_MakeRevol could not revolve profile up to face` / `Could not revolve the sketch`。
- S2 product diagnostic contract 准入裁决已完成：本轮起点 `HEAD=cc9e3a1190`，worktree clean；current diagnostic 批准为 CAD Core product diagnostic contract，但 FreeCAD native failure 仍保留为历史证据，不得写成 FreeCAD parity success。批准依据是两个 fixtures 同时覆盖 UpToFirst / UpToFace，primary diagnostic 稳定、locatable、request-local 且产品可见：UpToFirst 定位到 `Groove.Type / subname=UpToFirst`，UpToFace 定位到 `Groove.UpToFace / target=Pad / subname=Face4`。
- S3 expected / test / capability / docs 迁移实现已完成：新增两个 C51M1 Groove UpTo product diagnostic expected，`test_p7_features.py` 改为 expected-backed assertion，`capability_contract.cpp` / `test_adapters.py` 将 route 发布为 `product_diagnostic_contract_non_parity`，并保留 FreeCAD native failure note、fixture pair、delete/reopen condition。focused Groove test 与 C API capability smoke 已通过。
- 当前 live 队列应从 S4 `工作步骤细分/6-30-17-03-C12-M7-S4-focused-validation与发布边界复核.md` 开始；S4-S5 仍保持 pending。

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
