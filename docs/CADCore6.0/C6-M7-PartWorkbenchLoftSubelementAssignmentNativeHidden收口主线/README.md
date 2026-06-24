# C6-M7 Part Workbench Loft Subelement Assignment Native Hidden 收口主线

本目录承接 C6-M6 之后的 CAD Core 6.0 下一批工作：围绕 `part_workbench.loft.remaining_gaps=["part_loft_subelement_assignment_native_hidden"]` 建立一个可执行的收口包。目标不是重开完整 Loft 或 full Part surface family，而是把当前唯一 active Loft gap 复核清楚，并按证据收敛为 product contract、diagnostic boundary、narrowed gap 或 non-goal。

## 入口

- 主线总入口：`6-25-00-53-C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线总入口.md`
- 方案：`6-25-00-53-C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- S0 已冻结 live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=5caad308a9`，`git log -1=5caad308a9 发布 C6-M6 GeomPlate release gate`。
- C6-M1 到 C6-M6 队列均返回空表；C6-M7 队列从 S1 继续；C6-M6 已发布 `part_workbench.geomplate.remaining_gaps=[]`。
- 当前 CAD Core capability 中，Part Workbench surface family 的 Sweep / Filling / GeomPlate active remaining gaps 已清空；ProjectOnSurface 剩余项为 GUI / unverified advanced branch 边界，已经同时列入 non-goals。
- `cad-core/src/runtime/capability_contract.cpp` 与 `cad-core/tests/test_adapters.py` 均确认 `part_workbench.loft.remaining_gaps` 只有 `part_loft_subelement_assignment_native_hidden`。
- C5-M12 已关闭 Loft broad `complex_profile_family`，并保留 `c5m12/part-loft-subelement-assignment-diagnostic` 作为 native-hidden diagnostic-only 证据：`TypeError: Type must be App.DocumentObject or None, not tuple`，未采集 `object_fields.sections[].subname` 和 selected Sketch subelement `shape_summary`。
- C6-M7 只处理 `Part::Loft.Sections` 的 subelement assignment 证据闭环，不声明 FreeCAD parity、PartDesign Loft、GUI Loft 或 full Part surface family。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线/矩阵/*.tsv
git diff --check -- docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线 docs/CADCore6.0/README.md
```
