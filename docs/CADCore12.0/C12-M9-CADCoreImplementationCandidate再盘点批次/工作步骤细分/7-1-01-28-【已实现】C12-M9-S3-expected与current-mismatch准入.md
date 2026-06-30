# C12-M9 S3 expected 与 current mismatch 准入【已实现】

## 目标

用三闸门筛选候选：stable expected / product contract、request-local boundary、current mismatch。只有三项同时成立的行，才能进入 S4 source / landing 复核。

## 必读文件

- `../矩阵/c12m9_candidate_scope_review_matrix.tsv`
- `../矩阵/c12m9_candidate_backend_gap_classification.tsv`
- `../矩阵/c12m9_candidate_blocker_queue.tsv`
- 由 S1/S2 写入的 capability snapshot / candidate notes。

## 操作

1. 对每个候选记录 expected source：native expected、checked-in expected、product contract、product diagnostic contract、native-hidden 或 none。
2. 对每个候选记录 request-local boundary：approved、rejected、needs product decision 或 non-goal。
3. 对每个候选记录 current comparison status：current-covered、mismatch-confirmed、not comparable、blocked。
4. 只有 mismatch-confirmed 才标为 S4 candidate；其余写清阻断原因。

## 执行结果

- 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=514409a568`（`514409a568 docs: 关闭 C12-M9 S2 narrowed gaps 归类`），起点 `git -c core.quotepath=false status --short -uall` 无输出，即 worktree clean。
- 执行前 C12-M9 队列第一项为本 S3，后续为 S4-S6；本步关闭后队列应从 S4 继续。
- 已刷新 live capability 到 `/tmp/c12m9-capabilities-s3.json`，并复核 C12-M2/M3/M4/M6/M7/M8 README 中对应发布口径。本步未运行 FreeCADCmd，未新增或修改 fixture expected，未修改 `cad-core/src`、`include`、tests、adapters 或 capability source。
- `C12M9-CAT-001` CopyOnChange：expected source=`none`；request-local boundary=`needs product decision`；current comparison=`blocked/not comparable`。C12-M8 native copied graph gate 未过，完整 CopyOnChange DTO 未批准，current retained diagnostic 不能被写成 implementation mismatch。
- `C12M9-CAT-002` Groove UpTo：expected source=`product diagnostic contract`；request-local boundary=`approved`；current comparison=`current-covered`。继续是 product diagnostic contract non-parity，不是 native parity success。
- `C12M9-CAT-003` RuledSurface wire/wire：expected source=`checked-in expected`；request-local boundary=`approved`；current comparison=`current-covered`。未发现 checked-in expected/current mismatch。
- `C12M9-CAT-004` Part Workbench narrowed rows：expected source 与 boundary 为 mixed；current comparison=`current-covered or not comparable`。Sweep、Filling、GeomPlate、Loft、ProjectOnSurface 没有 mismatch-confirmed row。
- `C12M9-CAT-005` Assembly：request-local subset current-covered；full solver、persistent solver state 与 cross-request assembly session 为 non-goals。未发现 request-local subset mismatch。
- `C12M9-CAT-006` authorization placeholder：expected source=`none`；request-local boundary=`non-goal until a real row is admitted`；current comparison=`not comparable`。
- `C12M9-BLOCKER-301` 已关闭为 `closed_s3_no_admitted_candidate`，`C12M9-VAL-301` 已记录实际复核结果。S3 没有 S4 implementation candidate；后续 S4 只能记录 no-candidate source review，除非发现新的 stable expected/product contract + request-local boundary + current mismatch 证据。

## 关闭条件

- `C12M9-CAT-001..006` 均有 S3 gate 状态。
- `C12M9-BLOCKER-301` 关闭为 `closed_s3_no_admitted_candidate`。

## 非目标

- 不为了制造 mismatch 而改 expected 或放宽断言。
- 不把 product diagnostic contract 当作 native parity success。
- 不跑全量 build。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M9-CADCoreImplementationCandidate再盘点批次/矩阵/*.tsv
git diff --check
```
