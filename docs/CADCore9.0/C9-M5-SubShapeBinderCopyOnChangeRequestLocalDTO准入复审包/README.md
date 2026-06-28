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
- S1 已关闭 source/current 审计：FreeCAD full path 仍依赖 `setupCopyOnChange()` / `update()`、`_tmp_binder`、`_CopiedObjs`、`copyObject()`、`recomputeFeature(true)` 和 `_CopiedLink`；current `cad-core` 仍只发布 SubShapeBinder diagnostic boundary，`app/copy_on_change.cpp` 仅作为 App Link DTO 词汇对照。
- S2 已关闭 scope admission：`C9M5-SCOPE-001..301` 只保留 `native_oracle_required`、`product_decision_required`、`known_gap_retained`、`backend_gap_candidate`、`diagnostic_non_goal` 和 `release_gate` 路由；没有生成 `backend_gap_requires_implementation`，full temporary-document copied-object cache 保持 retained known gap。
- S3 已关闭 native lifecycle probe 复审：新增 `cad-core/tools/probe_c9m5_subshapebinder_copyonchange.py`、`cad-core/fixtures/c9m5/subshape-binder-copyonchange-lifecycle-probe.json` 和 `cad-core/fixtures/c9m5/expected/subshape-binder-copyonchange-lifecycle-probe.freecad.json`；FreeCADCmd 为 `/home/user/.local/bin/freecadcmd`，`freecad_version=1.2.0 revision 20260519`。证据能观察 Disabled / Enabled / Mutated / PartialLoad、`_tmp_binder` 和 `_CopiedLink`，但 `_CopiedObjs`、`copyObject()` dependency order 与 `recomputeFeature(true)` internal ElementMap lifecycle 仍不可导出为稳定 request-local DTO；`C9M5-SCOPE-103` 保持 `needs_more_native_evidence`，不打开 implementation gate。
- S4 已关闭 request-local DTO 产品边界复审：裁决为 `dto_rejected_known_gap_retained`。可接受字段仅限 request graph 可持久化字段、request-local `documentObjectUpdates` / diagnostics、source id/name、support subname、mutated property delta、copy intent 与 deletion/update/reselect 建议；forbidden fields 包括 hidden temporary document、TopoDS_Shape / BREP / full shape cache、request 后继续有效的 NamedShape / ElementMap / copied-object cache、`_CopiedObjs` private vector 和 native object pointer identity。
- S5 已关闭 cad-core 实现闸门与 diagnostic 发布复审：current `cad-core` 仍发布 `copy_on_change_full_temporary_document_cache_not_supported`，capability 仍保留 `known_gap_diagnostic` / `oracle_blocked` 和 `remaining_gaps=["copy_on_change_full_temporary_document_cache"]`，focused tests 仍证明 known gap。S6 是 no-code retained known gap release gate，不落 C++ support。
- S6 已关闭 Oracle 实现与发布闸门：消费 S3 native evidence、S4 `dto_rejected_known_gap_retained` 和 S5 `no_code_retained_known_gap_release_gate`，最终裁决为 `no_code_retained_known_gap_release_gate`。`C9M5-BLOCKER-601` 已关闭为 no-code retained known gap；队列为空；capability smoke 和 focused tests 继续证明 diagnostic、delete / reopen condition 与 `remaining_gaps=["copy_on_change_full_temporary_document_cache"]` 保留。

## 收口边界

- C9-M5 不重开 C8-M1 已关闭的 ShapeBinder/SubShapeBinder executor、ElementMap、NamedShape、Body replay 主路径。
- C9-M5 不继承 C8-M2 no-code 裁决为永久结论；它重新复核 native oracle 和 request-local DTO 准入，但必须拿到更强 evidence 才能落代码。
- 禁止引入跨请求 backend session、persistent temporary document、BREP、TopoDS_Shape、NamedShape、ElementMap 或 hidden cache。
- S4 已拒绝 DTO；S5/S6 已关闭为 no-code release gate，并保持 known gap。

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
