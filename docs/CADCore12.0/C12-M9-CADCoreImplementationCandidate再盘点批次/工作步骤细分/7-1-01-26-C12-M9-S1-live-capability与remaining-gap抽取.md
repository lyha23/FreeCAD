# C12-M9 S1 live capability 与 remaining gap 抽取

## 目标

从 live capability 中结构化抽取 `remaining_gaps`、`known_gaps`、diagnostics、covered subset 和当前 publication authority，明确哪些行只是 retained blocker，哪些行可进入后续 narrowed gap 审计。

## 必读文件

- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_adapters.py`
- `cad-core/tests/test_c8_shapebinder.py`
- `../矩阵/c12m9_candidate_source_candidates.tsv`
- `../矩阵/c12m9_candidate_scope_review_matrix.tsv`
- `../矩阵/c12m9_candidate_backend_gap_classification.tsv`

## 操作

1. 运行 `cad-core/build/cad-core capabilities` 并保存临时 snapshot 到 `/tmp/c12m9-capabilities.json`。
2. 用 `jq` 抽取所有非空 `remaining_gaps` 和 `known_gaps`。
3. 记录 CopyOnChange 的 current status、diagnostic、delete condition 和 reopen condition。
4. 记录 capability source / adapter assertion 的当前落点。
5. 更新 source / scope / backend classification。

## 关闭条件

- `C12M9-SRC-001..003` 有 current evidence。
- `C12M9-SCOPE-101` 已裁决为 retained blocker、active candidate 或 needs further gate。
- `C12M9-BLOCKER-101` 关闭。

## 非目标

- 不把唯一 live remaining gap 自动升级为 implementation。
- 不新增 fixture 或 expected。
- 不改 capability source。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M9-CADCoreImplementationCandidate再盘点批次/矩阵/*.tsv
git diff --check
```
