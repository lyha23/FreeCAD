# 【已实现】C12-M18 S3 产品扩展与 frontend sync 分流裁决

把 product extension、backend gap 和前端消费缺口分开，避免把非后端问题写成 C++ 实现任务。

## 必读

- `../README.md`
- `../矩阵/c12m18_live_backlog_product_extension_split.tsv`
- `../矩阵/c12m18_live_backlog_non_goal_registry.tsv`
- `../../../capability/7-5-00-14-cad-web-background非FreeCAD原生语义边界.md`
- `../../C12-M11-SketchInternalEdgeSubshapeMeshContract批次/README.md`
- `../../C12-M15-SketchStableGeometryIdMappedNameLedger设计批次/README.md`
- `../../C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/README.md`

## 操作

1. 明确 PartDesign 几何共线 BSpline / 非 Line axis 继续作为 product extension 保留。
2. 复核 C12-M11 open wire raw EdgeN、C12-M15 stable geometry id、C12-M16 split fragment ledger 的后端当前状态。
3. 若剩余工作是 my-chili3d consumer sync，只记录为 frontend package candidate，不创建 FreeCAD/cad-core C++ work。
4. 检查 `docs/capability` wording 是否还把已整改项列为 current non-native parity。
5. 验证后把本文件重命名为带 `【已实现】` 的同名文件。

## 完成记录

- live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=bcd6569a6c`（`bcd6569a6c 文档：关闭 C12-M18 S2 三闸门复审`），起点 worktree clean。
- PartDesign 几何共线 BSpline / 非 Line axis 继续作为 CAD Core product extension 保留；不打开 strict FreeCAD parity work，除非用户未来明确反转产品决策或 capability / expected 误称 native parity。
- `docs/capability/7-5-00-14-cad-web-background非FreeCAD原生语义边界.md` 已复核：当前仍只把 PartDesign axis extension 列为 current non-native product extension；`SubtractivePipe product PipeLaw` 主 `Shape` lifecycle 位于“已整改”段落，不再作为 current non-native parity。
- C12-M11 open wire raw `EdgeN` 当前为 backend current-supported；C12-M15 stable geometry id ledger final status 为 `design_published_no_code_current_sufficient`；C12-M16 split fragment ledger final status 为 `implemented_current_supported`。
- 若 sketch token、stable id 或 split fragment 仍有可见产品问题，本轮只记录为 `my-chili3d` frontend consumer sync package candidate；不创建 FreeCAD/cad-core C++ work，不做 backend prefix guessing。
- S3 只更新 C12-M18 文档 / 矩阵与 root README；未修改 `docs/capability` wording，未改 C++、fixtures、expected、tests 或 adapters。

## 非目标

- 不改 my-chili3d。
- 不删除已批准 product extension。
- 不把 frontend token consumer 缺口写成 backend geometry gap。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n "PartDesign 轴引用|SubtractivePipe product PipeLaw|product extension|frontend|my-chili3d" docs/CADCore12.0/C12-M18-CADCoreLiveBacklogReAudit批次 docs/capability/7-5-00-14-cad-web-background非FreeCAD原生语义边界.md
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M18-CADCoreLiveBacklogReAudit批次/矩阵/*.tsv
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M18-CADCoreLiveBacklogReAudit批次/工作步骤细分 --format markdown
git diff --check
```
