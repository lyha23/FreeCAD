# C12-M9 S2 narrowed gaps 与产品契约归类

## 目标

盘点 capability 中的 `narrowed_gaps`、product-contract non-parity、native-hidden 和 current-supported 行，避免把历史证据直接当作可实现 backend gap。

## 必读文件

- `/tmp/c12m9-capabilities.json`
- `docs/CADCore12.0/C12-M6-RuledSurfaceWireWire准入验证批次/README.md`
- `docs/CADCore12.0/C12-M7-PartDesignGrooveUpTo产品契约准入批次/README.md`
- `docs/CADCore12.0/C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次/README.md`
- `../矩阵/c12m9_candidate_scope_review_matrix.tsv`
- `../矩阵/c12m9_candidate_backend_gap_classification.tsv`
- `../矩阵/c12m9_candidate_non_goal_registry.tsv`

## 操作

1. 抽取 `part_design.revolution_groove.narrowed_gaps`。
2. 抽取 `part_workbench.sweep/filling/geomplate/loft/project_on_surface.narrowed_gaps`。
3. 归类每行：current-supported、product-contract non-parity、native-hidden、helper-blocked、oracle-blocked、possible implementation candidate。
4. 保留每行 reopen condition 和 delete condition。
5. 更新 scope / backend classification / non-goal。

## 关闭条件

- Groove、RuledSurface、ProjectOnSurface、Sweep、Filling、GeomPlate、Loft、Assembly 初始行均有分类。
- `C12M9-BLOCKER-201` 关闭。

## 非目标

- 不从 narrowed gap 名称推断 implementation。
- 不运行 FreeCADCmd。
- 不改 `cad-core/src`。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M9-CADCoreImplementationCandidate再盘点批次/矩阵/*.tsv
git diff --check
```
