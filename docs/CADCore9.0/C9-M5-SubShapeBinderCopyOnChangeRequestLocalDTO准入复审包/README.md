# C9-M5 SubShapeBinder CopyOnChange RequestLocal DTO 准入复审包

本目录承接 C9-M4 queue-empty 后的 live capability 状态。C9-M1 到 C9-M4 已把 Assembly request-local solver、marker、placement writeback 和 DistanceType default missing oracle 批次关闭；当前 live capability 里唯一非空 `remaining_gaps` 是 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`。

C9-M5 不把这个 known gap 直接写成 supported，也不搬运 FreeCAD temporary document cache。它只做一次新的准入复审：如果 FreeCAD native evidence 能证明稳定、产品可接受、完全 request-local 的 CopyOnChange DTO 子集，S6 才打开 `cad-core` C++ implementation gate；否则继续保留 `known_gap_diagnostic` / `oracle_blocked`。

## 入口

- 主线总入口：`6-28-11-24-C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包总入口.md`
- 方案：`6-28-11-24-C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包方案.md`
- 工作步骤总入口索引：`工作步骤细分/6-28-11-24-【已实现】C9-M5工作步骤总入口.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- 方案生成基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=ceef6a128b`（`ceef6a128b feat: 关闭 C9-M4 S6 默认距离类型发布闸门`），生成前工作区干净。
- S0 执行基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=ceef6a128b`，`git status --short -uall` 仅包含 `docs/CADCore9.0/README.md` 修改和本 C9-M5 包未跟踪文件，未发现范围外改动。
- C9-M4 队列为空；`assembly.remaining_gaps=[]`、`assembly.unsupported_joint_matrix=[]`、`distance_type_extended_geometry.default_or_todo_boundaries=[]`。
- `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`，对应 `known_gap.status=known_gap_diagnostic`、`route=oracle_blocked`、diagnostic 为 `copy_on_change_full_temporary_document_cache_not_supported`。
- FreeCAD source authority 显示 full path 依赖 `_CopiedObjs`、`"_tmp_binder"` temporary document、`copyObject()`、`recomputeFeature(true)` 和 `_CopiedLink`。
- cad-core 当前只支持 SubShapeBinder request-local shape / ElementMap / NamedShape / BindMode 子集；CopyOnChange Enabled / Mutated / PartialLoad 只发布 diagnostic 和 lifecycle boundary。
- S0 已冻结本包状态词典：当前 gap 仍是 `copy_on_change_full_temporary_document_cache`，状态只能写作 `known_gap_diagnostic` / `oracle_blocked`；S6 code gate 只有在 S3 native evidence 与 S4 产品边界同时成立时打开。

## 收口边界

- C9-M5 不重开 C8-M1 已关闭的 ShapeBinder/SubShapeBinder executor、ElementMap、NamedShape、Body replay 主路径。
- C9-M5 不继承 C8-M2 no-code 裁决为永久结论；它重新复核 native oracle 和 request-local DTO 准入，但必须拿到更强 evidence 才能落代码。
- 禁止引入跨请求 backend session、persistent temporary document、BREP、TopoDS_Shape、NamedShape、ElementMap 或 hidden cache。
- 若 S3-S5 仍只能证明 property-state evidence，S6 必须关闭为 no-code release gate，并保持 known gap。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包 docs/CADCore9.0/README.md
git diff --check
```
