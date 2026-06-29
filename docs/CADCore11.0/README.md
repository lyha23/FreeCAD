# CADCore11.0

CADCore11.0 承接 CADCore10.0 队列关闭后的下一轮 evidence-first 收口工作。当前不再继续 C10-M4 CopyOnChange：`copy_on_change_full_temporary_document_cache` 仍是 retained known gap / oracle blocked，不是默认代码实现入口。

C11-M1 转向 Part Workbench Sweep `Location` overload native parity 复开。C6-M4 已把 `SectionOptions[].ProfilePlacement=AnchorLocationToSpineStart`、`WithContact`、`WithCorrection`、AuxiliarySpine + Tolerance + Transition + located section 发布为 CAD Core product contract non-parity；当前 live capability 的 `part_workbench.sweep.remaining_gaps=[]`，但仍保留两个 narrowed historical wrapper evidence：`part_sweep_located_profile_freecadcmd_wrapper_build_blocker` 与 `part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker`。

本批次目标不是把 C6-M4 重新实现一遍，而是复开 FreeCAD native oracle：如果 `FreeCADCmd` / native helper 能稳定采到 `add(Profile, Location, WithContact, WithCorrection)` 与 combined auxiliary / tolerance / located section 的 `shape_summary`，再比较 current cad-core product contract 是否能升级为 FreeCAD parity；如果仍然不可采，则继续保留 non-parity product contract 和 narrowed evidence，不新增 C++。

## 入口

- C11-M1 总入口：`C11-M1-PartSweepLocationOverloadNativeParity复开批次/6-29-10-10-C11-M1-PartSweepLocationOverloadNativeParity复开批次总入口.md`
- C11-M1 方案：`C11-M1-PartSweepLocationOverloadNativeParity复开批次/6-29-10-10-C11-M1-PartSweepLocationOverloadNativeParity复开批次方案.md`
- C11-M1 工作步骤：`C11-M1-PartSweepLocationOverloadNativeParity复开批次/工作步骤细分/`
- C11-M1 矩阵：`C11-M1-PartSweepLocationOverloadNativeParity复开批次/矩阵/`

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore11.0/C11-M1-PartSweepLocationOverloadNativeParity复开批次/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore11.0/C11-M1-PartSweepLocationOverloadNativeParity复开批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore11.0
git diff --check
```
