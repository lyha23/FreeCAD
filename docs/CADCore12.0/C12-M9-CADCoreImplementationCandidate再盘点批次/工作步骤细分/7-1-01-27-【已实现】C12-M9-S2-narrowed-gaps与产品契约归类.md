# C12-M9 S2 narrowed gaps 与产品契约归类【已实现】

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

## 执行结果

- 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=e0a9b08d2a`（`e0a9b08d2a 文档：关闭 C12-M9 S1 live capability 抽取`），起点 `git -c core.quotepath=false status --short -uall` 无输出，即 worktree clean。
- 执行前 C12-M9 队列第一项为本 S2，后续为 S3-S6；本步关闭后队列应从 S3 继续。
- 已重新运行 `cad-core/build/cad-core capabilities > /tmp/c12m9-capabilities.json`，并用 `jq` 抽取 `part_design.revolution_groove`、`part_workbench.sweep/filling/geomplate/loft/project_on_surface` 与 `assembly.ondsel_solver_adapter` / `assembly.representative_solver_adapter`。
- Groove UpTo：live status 为 `supported_c12m7_groove_upto_product_diagnostic_contract`，narrowed gap route 为 `product_diagnostic_contract_non_parity`，归类为 `product_diagnostic_contract_non_parity_retained`。delete/reopen 条件保持 C12-M7 口径：只有同一 FreeCAD/LibPack/OCCT baseline 证明 UpToFirst 与 UpToFace native success，并出现 current mismatch，才可替换 product diagnostic contract。
- RuledSurface wire/wire：live status 为 `supported_wire_wire_expected_backed` 且 `remaining_gaps=[]`，继承 C12-M6 `wire_wire_admitted_current_supported`，归类为 `current_supported_retained`。只有 regression 或 checked-in expected/current mismatch 才重开。
- Part Workbench narrowed gaps：Sweep 归类为 product-contract non-parity，并保留 Location/native-probe blocker 与 current-covered context；Filling 归类为 helper-blocked/product-contract non-parity；GeomPlate projected curve2d initial surface 为 current-covered，其余 curve criteria、G1 curve-on-surface、PlateSurface wrapper lifecycle 与 no-initial-surface oracle blocker 分别按 request-local product contract、native-hidden、non-goal 或 oracle-blocked 保留；Loft 归类为 native-hidden product-contract non-parity；ProjectOnSurface 归类为 native-hidden/request-local ledger product contract。
- Assembly：`ondsel_solver_adapter.status=covered_full`，覆盖 request-local grounded joints、extended distance geometry、subshape marker placement、runPreDrag 和 placement validation；`representative_solver_adapter.status=covered_representative`，full solver、persistent solver state、cross-request assembly session 继续是 non-goals。
- `C12M9-SCOPE-201/202/301/401` 已关闭，`C12M9-CAT-002..005` 已写入 decision，`C12M9-NG-004/006` 继续保留，`C12M9-BLOCKER-201` 已关闭；新增 `C12M9-VAL-201` 记录 S2 validation。
- 本步未运行 FreeCADCmd，未新增 fixture/expected，未修改 `cad-core/src`、`include`、tests、adapters 或 capability source，未做 S3 stable expected/current mismatch gate，未授权 implementation package。

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
