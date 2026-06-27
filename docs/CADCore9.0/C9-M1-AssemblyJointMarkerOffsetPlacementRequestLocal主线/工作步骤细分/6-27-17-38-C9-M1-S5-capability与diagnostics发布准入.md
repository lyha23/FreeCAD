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

## 非目标

- 不把 GUI/session/persistent solver state 标为 supported。
- 不对下游仓库做同步实现。
- 不移除必要 diagnostics 来伪装支持。
