# CADCore11.0

CADCore11.0 承接 CADCore10.0 队列关闭后的下一轮 evidence-first 收口工作。当前不再继续 C10-M4 CopyOnChange：`copy_on_change_full_temporary_document_cache` 仍是 retained known gap / oracle blocked，不是默认代码实现入口。

C11-M1 转向 Part Workbench Sweep `Location` overload native parity 复开。C6-M4 已把 `SectionOptions[].ProfilePlacement=AnchorLocationToSpineStart`、`WithContact`、`WithCorrection`、AuxiliarySpine + Tolerance + Transition + located section 发布为 CAD Core product contract non-parity；当前 live capability 的 `part_workbench.sweep.remaining_gaps=[]`，但仍保留两个 narrowed historical wrapper evidence：`part_sweep_located_profile_freecadcmd_wrapper_build_blocker` 与 `part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker`。

本批次目标不是把 C6-M4 重新实现一遍，而是复开 FreeCAD native oracle：如果 `FreeCADCmd` / native helper 能稳定采到 `add(Profile, Location, WithContact, WithCorrection)` 与 combined auxiliary / tolerance / located section 的 `shape_summary`，再比较 current cad-core product contract 是否能升级为 FreeCAD parity；如果仍然不可采，则继续保留 non-parity product contract 和 narrowed evidence，不新增 C++。

当前 C11-M1 S3 已复跑本机 FreeCADCmd：FreeCAD `1.2.0 revision 20260519` / OCCT `7.8.1` 仍在 Location overload `builder.build()` 阶段返回 `OCCError: NCollection_Array1::Value`，advanced combined 保留为 located dependency-retained；因此暂不新增 C11-M1 native expected 或 C++ gate。

C11-M2 转向 Part Workbench Filling `Part.makeFilledFace` native helper parity 复开。C6-M5 已把 Surface、Supports/Orders、ExplicitParams 和 non-boundary support/order 发布为 CAD Core request-local product contract non-parity；当前 live capability 的 `part_workbench.filling.remaining_gaps=[]`，但 six native helper evidence 仍保留在 `narrowed_gaps` / `historical_native_helper_evidence`。

C11-M2 的目标不是重做 C6-M5，而是重新采集当前 FreeCAD / OCCT 下的 `Part.makeFilledFace(...)` helper oracle：如果 Surface、Supports/Orders G1/G2、ExplicitParams all params 或 non-boundary support/order 能稳定返回 native `shape_summary`，再比较 C6-M5 current product contract 是否可升级为 parity；如果仍然 crash / timeout / `notCollected`，则发布 no-code retained non-parity gate。

当前 C11-M2 S3 已复跑本机 FreeCADCmd：FreeCAD `1.2.0 revision 20260519` / OCCT `7.8.1` 下 Surface helper 仍返回 `TypeError: argument 2 must be , not Part.Face`，Supports/Orders G1/G2 同样无法稳定采集，PtsOnCurve / TolG1+TolG2 / MaxSegments / all params 与 non-boundary support/order 仍有 SIGSEGV / timeout / no-payload evidence；direct wrapper controls 只作为 diagnostic dependency，不进入 request-local expected。

## 入口

- C11-M1 总入口：`C11-M1-PartSweepLocationOverloadNativeParity复开批次/6-29-10-10-C11-M1-PartSweepLocationOverloadNativeParity复开批次总入口.md`
- C11-M1 方案：`C11-M1-PartSweepLocationOverloadNativeParity复开批次/6-29-10-10-C11-M1-PartSweepLocationOverloadNativeParity复开批次方案.md`
- C11-M1 工作步骤：`C11-M1-PartSweepLocationOverloadNativeParity复开批次/工作步骤细分/`
- C11-M1 矩阵：`C11-M1-PartSweepLocationOverloadNativeParity复开批次/矩阵/`
- C11-M2 总入口：`C11-M2-PartWorkbenchFillingNativeHelperParity复开批次/6-29-12-22-C11-M2-PartWorkbenchFillingNativeHelperParity复开批次总入口.md`
- C11-M2 方案：`C11-M2-PartWorkbenchFillingNativeHelperParity复开批次/6-29-12-22-C11-M2-PartWorkbenchFillingNativeHelperParity复开批次方案.md`
- C11-M2 工作步骤：`C11-M2-PartWorkbenchFillingNativeHelperParity复开批次/工作步骤细分/`
- C11-M2 矩阵：`C11-M2-PartWorkbenchFillingNativeHelperParity复开批次/矩阵/`

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore11.0/C11-M1-PartSweepLocationOverloadNativeParity复开批次/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore11.0/C11-M2-PartWorkbenchFillingNativeHelperParity复开批次/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore11.0/C11-M1-PartSweepLocationOverloadNativeParity复开批次/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore11.0/C11-M2-PartWorkbenchFillingNativeHelperParity复开批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore11.0
git diff --check
```
