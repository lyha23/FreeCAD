# 【已实现】C6-M3 S0 live 基线与源码 authority 复核

## 完成结论

- S0 已完成 live 基线复核：`pwd=/home/user/Chili3DProject/FreeCAD`，`git rev-parse --short HEAD=92cb3e9d07`，`git log -1 --oneline=92cb3e9d07 docs: 增加 C6-M3 管道插值法则主线`，开始时 `git -c core.quotepath=false status --short -uall` 无输出。
- C6-M1 / C6-M2 已关闭：两条 `工作步骤细分` 队列脚本均只输出 markdown 表头，无 pending row。C6-M1 结论仍是 Linear / S-shape 和 tangent ledger 已作为 CAD Core product extension 发布，Interpolation 保留 precise remaining gap；C6-M2 结论仍是 expected fixture gate、阶段回归和 heavy 收口通过。
- live cad-core 仍把 Pipe `Transformation=Interpolation` 停在 `product_contract_required`：`cad-core/src/part_design/feature_pipe.cpp::resolvePipeLaw()` 对 transformation 4 发出 LawSamples product contract 诊断，`cad-core/src/runtime/capability_contract.cpp` 的 `part_design.pipe` exact blocker / `remaining_gaps` 仍包含 `partdesign_pipe_interpolation_law_product_contract_required`。
- FreeCAD authority 仍是 enum-only：`src/Mod/PartDesign/App/FeaturePipe.cpp::TransformEnums` 暴露 `Interpolation`，`Pipe::execute()` 只有被注释掉的 `Law_Linear` / `Law_S` 提示块，没有 `Transformation.getValue() == 4` 的 Interpolation 执行分支。
- S0 只更新 C6-M3 的 source candidates、scope review、blocker queue 和本步骤文档，不改 C++、fixtures、LawSamples 最终 schema，也不修改 C6-M1/C6-M2 关闭结论。

## 复核输入

- `docs/CADCore6.0/README.md`
- `docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/README.md`
- `docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/6-24-00-16-C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线总入口.md`
- `docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/工作步骤细分/6-24-00-17-【已实现】C6-M3工作步骤总入口.md`
- C6-M1 / C6-M2 已实现主线总入口、工作步骤总入口和 S6 文档
- `cad-core/src/part_design/feature_pipe.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_adapters.py`
- `src/Mod/PartDesign/App/FeaturePipe.cpp`

## live 证据

| 证据项 | live 结论 |
| --- | --- |
| C6-M1 队列 | `python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线/工作步骤细分 --format markdown` 输出空 pending 表。 |
| C6-M2 队列 | `python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/工作步骤细分 --format markdown` 输出空 pending 表。 |
| cad-core executor | `rg -n 'Interpolation|LawSamples|product_contract_required|remaining_gaps' cad-core/src/part_design/feature_pipe.cpp` 命中 `resolvePipeLaw()` 的 `product_contract_required` 诊断、`pipe_law.kind=Interpolation` 和 `LawSamples` allow-list。 |
| capability | `cad-core/src/runtime/capability_contract.cpp` 在 `part_design.pipe` 中发布 `partdesign_pipe_interpolation_law_product_contract_required` exact blocker，并把同一 id 放入 `remaining_gaps`。 |
| focused tests | `cad-core/tests/test_p7_features.py` 仍断言 Interpolation boundary 返回 `product_contract_required`、`pipe_law.status=product_contract_required` 且无 `shape`；`cad-core/tests/test_adapters.py` 仍断言 capability remaining gap 精确等于该 blocker。 |
| FreeCAD authority | `src/Mod/PartDesign/App/FeaturePipe.cpp` 的 `TransformEnums` 包含 `Interpolation`；`Pipe::execute()` 只出现 active multisection branch 和被注释的 Linear / S-shape law 块，没有 active Interpolation branch。 |

## 非目标

- 不定义 LawSamples 最终 schema，留给 S1。
- 不改 C++。
- 不新增 fixture。
- 不修改 C6-M1/C6-M2 关闭结论。

## 验收结果

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/工作步骤细分 --format markdown
rg -n 'Interpolation|LawSamples|product_contract_required|remaining_gaps' cad-core/src/part_design/feature_pipe.cpp cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_p7_features.py
for f in docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
git diff --check -- docs/CADCore6.0
```

- C6-M1 / C6-M2 队列：均为空 pending 表。
- `rg`：命中当前 Interpolation diagnostic boundary、LawSamples allow-list、capability exact blocker / remaining gap 和 focused test assertions。
- TSV field-count check：无输出。
- `git diff --check -- docs/CADCore6.0`：无输出。
