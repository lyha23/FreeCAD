# C12-M9 S1 live capability 与 remaining gap 抽取【已实现】

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

## 执行结果

- 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=a7e7eb040f`（`a7e7eb040f 文档：冻结 C12-M9 S0 live 基线`），起点 worktree clean。
- C12-M9 执行前队列第一项为本 S1，后续为 S2-S6；本步关闭后队列应从 S2 继续。
- live capability snapshot 保存到 `/tmp/c12m9-capabilities.json`。唯一非空 `remaining_gaps` 是 `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- live `known_gaps` 只有 `part_design.sub_shape_binder.known_gaps.copy_on_change_full_temporary_document_cache`，current 为 `status=known_gap_diagnostic`、`route=oracle_blocked`、`diagnostic=copy_on_change_full_temporary_document_cache_not_supported`。
- CopyOnChange delete condition 仍是 FreeCADCmd 暴露不依赖 persistent backend session 的 stable request-local copied-object evidence；reopen condition 继承为更强 native copied graph artifact + request-local DTO approval + current mismatch，三项未齐不得进入 implementation。
- `narrowed_gaps` presence 保持为 15 条：Groove UpTo 1 条、Filling 6 条、GeomPlate 4 条、Loft 1 条、ProjectOnSurface 1 条、Sweep 2 条。S1 仅记录 presence，不做 S2 产品契约归类或 S3 current mismatch 判断。
- covered/current subset 摘要：`part_design.sub_shape_binder.status=supported_c8m1_expected_backed_request_local_with_copy_on_change_known_gap`；`part_workbench.ruled_surface.status=supported_wire_wire_expected_backed`；Sweep/Filling/GeomPlate/Loft/ProjectOnSurface 均为 expected-backed 或 product-contract supported 状态；Assembly real ondsel adapter 与 placement writeback 为 `covered_full`，extended DistanceType 与 subshape marker 为 covered subset，representative adapter 仍只是 fallback metadata。
- publication authority 位于 `cad-core/src/runtime/capability_contract.cpp::capabilityContractJson()`、`diagnosticCodeList()`、`ondselSolverCapabilityJson()` 与 `part_design.sub_shape_binder` capability block；adapter assertions 位于 `cad-core/tests/test_adapters.py::assert_capability_publication_smoke()`、`test_c_api_capabilities_exposes_web_contract_facts()` 和 narrowed gap assertions；CopyOnChange retained diagnostic 由 `cad-core/tests/test_c8_shapebinder.py::test_capability_contract_publishes_c8m1_binder_scope()` 断言。
- 本步未新增 fixture/expected，未修改 capability source、production code、tests 或 adapters，未运行 FreeCADCmd oracle，未把唯一 remaining gap 升级为 implementation。

## 关闭条件

- `C12M9-SRC-001..003` 已写入 current evidence。
- `C12M9-SCOPE-101` 已裁决为 `retained_blocker_needs_further_gate`。
- `C12M9-CAT-001` 已记录为 retained known gap，不授权 implementation。
- `C12M9-BLOCKER-101` 已关闭。

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
