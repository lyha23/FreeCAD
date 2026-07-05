# 【已实现】C12-M18 S2 历史 narrowed gap 三闸门复审

对 S1 抽取的 `narrowed_gaps` family 做统一准入复审。

## 必读

- `../README.md`
- `../矩阵/c12m18_live_backlog_narrowed_gap_gate.tsv`
- `../矩阵/c12m18_live_backlog_backend_gap_classification.tsv`
- `../../C12-M7-PartDesignGrooveUpTo产品契约准入批次/README.md`
- `../../C12-M12-FreeCADSweepPipeParity迁移批次/README.md`
- `../../C12-M14-PartSweepHelperMutableLifecycle证据解锁批次/README.md`
- `../../C12-M17-SubtractivePipeProductPipeLawShapeParity实现批次/README.md`
- `../../README.md`

## 操作

1. 对 Groove、Sweep、Filling、GeomPlate、Loft、ProjectOnSurface、Assembly、Sketch / topo / SubShapeBinder families 应用三闸门。
2. 对每行写明 expected/contract、request-local boundary、current comparison status。
3. 只有 current comparison 为 `mismatch_confirmed` 的行才能进入 S4 implementation authorization。
4. helper-blocked、native-hidden、oracle-blocked、product-contract non-parity、current-supported 都要保留为分类，不得混写。
5. 验证后把本文件重命名为带 `【已实现】` 的同名文件。

## 完成记录

- live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=60da049164`（`60da049164 文档：关闭 C12-M18 S1 capability 抽取`），起点 worktree clean。
- 已复审 Groove、Sweep、Filling、GeomPlate、Loft、ProjectOnSurface、Assembly、Sketch/topo 与 SubShapeBinder；每行均写入 expected/contract、request-local boundary 与 current comparison status。
- S2 结论：没有 `current_comparison=mismatch_confirmed` 行。Groove 保留 product diagnostic non-parity；Sweep/Filling/GeomPlate/Loft/ProjectOnSurface 保留 helper-blocked、native-hidden、oracle-blocked、product-contract non-parity、non-goal 或 current-covered 分类；Assembly、Sketch/topo、SubShapeBinder 保留 request-local current-supported 或外部/非目标边界。
- S2 未采集 native oracle，未刷新 expected，未写 C++，未运行 FreeCADCmd，未修改 capability source、tests 或 adapters。
- S2 只更新 C12-M18 README、总入口、矩阵、本步骤状态与 root `docs/CADCore12.0/README.md`。

## 非目标

- 不采集 native oracle。
- 不刷新 expected。
- 不写 C++。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M18-CADCoreLiveBacklogReAudit批次/矩阵/*.tsv
git diff --check
```
