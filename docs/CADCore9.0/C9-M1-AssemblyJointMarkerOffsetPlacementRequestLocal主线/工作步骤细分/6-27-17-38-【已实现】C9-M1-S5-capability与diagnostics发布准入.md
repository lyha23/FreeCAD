# C9-M1 S5 capability 与 diagnostics 发布准入

## 目标

根据 S0-S4 裁决 C9-M1 的最终发布方式：保持 non-goal / known-gap，打开 S6 受限实现，或只更新 capability / tests / docs。

## 裁决规则

- 如果 marker / writeback 已由 source 和 expected 覆盖，S6 只做 capability publication closure 或 no-code release gate。
- 如果 non-identity `offsetPlc` 有 native expected 和 current mismatch，标为 `backend_gap_candidate` 并列出 S6 C++ 落点。
- 如果 non-AssemblyLink primitive frame 或 zero Angle fallback 缺 native oracle，保持 `oracle_candidate` / `known_gap_retained`。
- 如果能力违反无状态边界，必须进入 `diagnostic_non_goal`。
- capability 不能靠 adapter 字符串隐藏 runtime 缺口。

## 必须回写

- `c9m1_assembly_marker_offset_backend_gap_classification.tsv`
- `c9m1_assembly_marker_offset_non_goal_registry.tsv`
- `c9m1_assembly_marker_offset_validation_matrix.tsv`
- `C9M1-BLOCKER-501`
- README 的 S5 结论。

## S5 结论

- S5 执行基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=6d7cb18f0f`（`6d7cb18f0f docs: 完成 C9-M1 S4 writeback 复审`），开始状态干净。
- `./cad-core capabilities` 的 live 输出中，`assembly.ondsel_solver_adapter.status=covered_full`、`available=true`、`mode=request_local_runPreDrag`；`subshape_marker_placement.status=covered_representative_subset`，`request_local_boundaries` 仍是 `identity_offset_assembly_link_subset` 与 `request_graph_no_persistent_solver_state`。
- capability 仍把 `non_identity_bundled_offsetPlc` 与 `non_assembly_link_subshape_primitive_frame_generalization` 列在 `subshape_marker_placement.non_goals`，`placement_writeback.status=covered_full` 且只发布 `documentObjectUpdates.action=assembly_set_placement`；`solver_validation` 仍发布 `unsupported_assembly_solver`、`missing_grounded_part`、`ondsel_solver_failed`、`invalid_assembly_solver_result`。
- `test_adapters.py` 已有断言覆盖上述 runtime capability，说明发布口径不是靠 adapter 字符串隐藏 runtime 缺口；`test_p8_features.py` 继续覆盖 request-local writeback、next-request no-op、partial writeback 和 unsupported diagnostics。
- S5 裁决：marker / writeback 已由 source、expected 和 focused tests 覆盖；non-identity bundled `offsetPlc` 缺 native expected 加 current mismatch，保持 `oracle_candidate` / forbidden guessing；non-AssemblyLink primitive frame 缺 approved DTO 与 native oracle，保持 `diagnostic_non_goal`；zero Angle 缺 native oracle 与 current mismatch，保持 `known_gap_retained`。S5 不打开 runtime C++、fixture 或 native oracle gate。

## S6 route

- route：`capability publication closure` / no-code release gate。
- allowed files：仅 `docs/CADCore9.0/C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/README.md`、主线总入口、`工作步骤细分/` 状态文件、`矩阵/*.tsv`；除非 S6 的 focused capability smoke 或 adapter tests 与 S5 证据冲突，否则不改 `cad-core/src`、`cad-core/tests`、fixtures 或 expected。
- focused tests：必须跑 `cd cad-core && ./cad-core capabilities > /tmp/c9m1-capabilities.json`、`cd cad-core && python3 -m unittest tests.test_adapters`、队列脚本、TSV field count、`rg -n '[ \t]$' docs/CADCore9.0`、`git diff --check`。
- build：不需要。只有 S6 发现必须修改 C++ capability/runtime 时才补 `cmake --build build`。
- native oracle refresh：不需要。non-identity bundled `offsetPlc` 与 zero Angle 只有在后续采到 native expected 且 current mismatch 后，才能另开受限实现或 oracle 包。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
./cad-core capabilities > /tmp/c9m1-capabilities.json
python3 -m unittest tests.test_adapters
```

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/矩阵/*.tsv
git diff --check
```

S5 关闭时，S6 route 必须写清：allowed files、focused tests、是否需要 build、是否需要 native oracle refresh。

## 验收结果

- `cd cad-core && ./cad-core capabilities > /tmp/c9m1-capabilities.json` 通过，输出继续保留 Assembly request-local capability、non-goals 与 diagnostics。
- `cd cad-core && python3 -m unittest tests.test_adapters` 通过，29 个 adapter / capability 测试 OK。
- TSV field count、docs trailing-whitespace、`git diff --check` 均通过；trailing-whitespace 检查退出码 1 表示无匹配。

## 非目标

- 不把 GUI/session/persistent solver state 标为 supported。
- 不对下游仓库做同步实现。
- 不移除必要 diagnostics 来伪装支持。
