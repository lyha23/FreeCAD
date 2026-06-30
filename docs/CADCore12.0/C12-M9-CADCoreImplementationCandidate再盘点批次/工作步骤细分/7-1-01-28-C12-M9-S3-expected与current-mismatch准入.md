# C12-M9 S3 expected 与 current mismatch 准入

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

## 关闭条件

- `C12M9-CAT-001..006` 均有 S3 gate 状态。
- `C12M9-BLOCKER-301` 关闭或明确保留到 S4/S5。

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
