# P8 Assembly Reference / JCS MarkerPlacement S6 Capability 与发布闸门

## 目标

在 S3-S5 通过后，发布 subshape marker placement supported subset，并确认 radius-bearing DistanceType、curve/default、GUI/session 和 persistent solver state 没有被误发布。

## 发布要求

- C ABI capability 增加 `subshape_marker_placement` 或等价字段。
- supported subset 明确列出 object / Vertex / Edge / Face / mixed representative coverage。
- DistanceType docs 回写：basic DistanceType 已有 solver DTO/class/scalar，且本包补齐 representative placement parity。
- P8 AssemblySolver docs 回写：Reference/JCS marker placement supported subset 与 remaining boundaries。
- blocker queue `MP-BLOCK-001..010` 全部 closed。
- `connector_only_marker_shortcut` 必须保留为排除项，除非实现已经证明它只适用于 object-level baseline 且不会作为 subshape support 发布。

## 验收

```bash
python3 -m unittest cad-core.tests.test_adapters.CadCoreAdapterTest -k capabilities
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵/*.tsv
rg -n 'subshape_marker_placement|radius-bearing|curve/default|persistent_solver_state' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线 cad-core/src/adapters/c_api/c_api.cpp cad-core/tests/test_adapters.py
rg -n 'MP-BLOCK-00[1-9]|MP-BLOCK-010|connector_only_marker_shortcut' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线 cad-core/src/adapters/c_api/c_api.cpp cad-core/tests/test_adapters.py
```

## 非目标

- 不发布 radius-bearing DistanceType。
- 不发布 curve/default DistanceType。
- 不发布 GUI/session 或 persistent solver state。
- 不发布 connector-only subshape marker placement。
