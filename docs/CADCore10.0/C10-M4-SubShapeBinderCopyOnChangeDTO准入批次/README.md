# C10-M4 SubShapeBinder CopyOnChange DTO 准入批次

## 当前定位

C10-M4 承接 C10-M3 队列关闭后的 live capability 剩余项：`part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`。这个剩余项目前仍是 `known_gap_diagnostic` / `oracle_blocked`，不是直接实现 full temporary-document cache 的许可。

本包目标是做 SubShapeBinder `BindCopyOnChange` 的 request-local DTO 准入：先证明 FreeCAD native copied-object lifecycle 能稳定转成无状态请求证据，再决定是否把 cad-core 现有 `documentObjectUpdates` 机制扩到 SubShapeBinder。若证据或产品边界不足，S6 只发布 no-code retained diagnostic / release gate。

## 入口

- 总入口：`6-29-03-29-C10-M4-SubShapeBinderCopyOnChangeDTO准入批次总入口.md`
- 方案：`6-29-03-29-C10-M4-SubShapeBinderCopyOnChangeDTO准入批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 工作步骤

- S0：live 基线与声明口径冻结。
- S1：FreeCAD 源码与 current coverage 候选矩阵复核。
- S2：范围准入与 blocker 矩阵。
- S3：CopyOnChange native probe 与 DTO evidence 专项复审。
- S4：cad-core 请求 DTO 与 `documentObjectUpdates` 专项复审。
- S5：临时文档禁用与 non-goal 边界专项复审。
- S6：Oracle 实现与发布闸门。

## 当前禁止声明

- 不实现 backend session、persistent temporary document 或 cross-request cache。
- 不把 BREP、TopoDS_Shape、NamedShape、ElementMap 或完整 copied-object graph 保存为后端状态。
- 不把 FreeCAD GUI / task panel / user prompt 生命周期写入 cad-core。
- 不把 App::Link 的完整 CopyOnChange group lifecycle 当成 SubShapeBinder supported，除非 S3-S5 证明 request-local DTO 子集成立。
- 不用 fixture 名称、几何猜测、输出修剪或 adapter repair 伪造 CopyOnChange 语义。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次 docs/CADCore10.0/README.md
git diff --check
```
