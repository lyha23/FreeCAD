# C8-M2 SubShapeBinder CopyOnChange 下游同步与 DTO 准入主线

本目录承接 C8-M1 release gate 之后的下一轮 CADCore8.0 方案。C8-M1 已经完成 `PartDesign::ShapeBinder` / `PartDesign::SubShapeBinder` 的 FreeCAD native oracle、`cad-core` executor、ElementMap / NamedShape / Body replay 和 capability 发布；C8-M2 不继续扩展 C8-M1 已关闭主路径。

C8-M2 的目标是拆清两件事：

- 下游同步：把 C8-M1 的 type ids、capability、fixtures、diagnostics、ElementMap / NamedShape 输出合同整理成 `opencascade-rs` / 前端接入可以消费的源头方案。
- CopyOnChange DTO 准入：审计 `SubShapeBinder BindCopyOnChange` 是否存在不依赖跨请求 FreeCAD temporary document / copied-object cache 的 request-local 子集；若没有稳定 native evidence，保持 `known_gap` / diagnostic。

## 入口

- 主线总入口：`6-26-22-20-C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线总入口.md`
- 方案：`6-26-22-20-C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入方案.md`
- 工作步骤总入口索引：`工作步骤细分/6-26-22-20-【已实现】C8-M2工作步骤总入口.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- C8-M1 队列为空，`cad-core` capability 当前发布 `part_design.shape_binder.remaining_gaps=[]`。
- `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`，对应 `known_gap.status=known_gap_diagnostic`、`route=oracle_blocked`。
- S0 live 基线已冻结：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=dc93b0d3af`（`dc93b0d3af chore: 完成 C8-M1 S6 发布闸门`），开始工作区包含既有 C8-M2 文档/矩阵未提交改动与 `docs/CADCore8.0/README.md` 修改。
- C8-M2 S0 已实现 live 基线冻结；S1 已实现 FreeCAD source authority 与 C8-M1 capability/tests/fixtures/current diagnostic 边界复核。S1 live 基线为 `HEAD=e7e07663d9`（`e7e07663d9 docs: 完成 C8-M2 S0 live 基线冻结`），开始工作区干净。
- S1 已确认 `SubShapeBinder::setupCopyOnChange()` / `checkCopyOnChange()` / `onChanged()` / `update()`、`LinkBaseExtension::setupCopyOnChange()`、`cad-core/src/part_design/feature_shape_binder.cpp`、`cad-core/src/app/copy_on_change.cpp`、capability、focused tests、generic diagnostics suite 和 C8-M1 CopyOnChange fixture/expected 的当前边界；C8-M2 仍保持 `copy_on_change_full_temporary_document_cache` 为 known_gap diagnostic。
- C8-M2 当前 S0/S1 已实现；S2-S6 仍待执行。矩阵中 S1 已回写 source candidates、scope review、non-goal registry，并关闭 `C8M2-BLOCKER-101`；oracle、下游同步、capability 和 release gate 结论仍等待后续步骤。

## 收口边界

- C8-M2 不修改 Rust 下游代码；若需要执行下游同步，应另在 `opencascade-rs` 建包。
- C8-M2 不实现跨请求 backend session、persistent BREP、TopoDS_Shape、NamedShape、ElementMap 或 FreeCAD temporary document cache。
- C8-M2 只有在 S3 证明 request-local CopyOnChange DTO 可由 native oracle 支撑时，S6 才能打开 FreeCAD `cad-core` C++ implementation gate。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线 docs/CADCore8.0/README.md
git diff --check
```
