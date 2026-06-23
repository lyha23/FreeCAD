# 【已实现】C6-M1 S5 PipeShellHistoryCapability 专项复审

## 完成结论

- S5 已锁定 PipeShellHistoryCapability 发布闸门：C6 law / tangent 实现只能在 request-local PipeShell 构建链路中接入，不能重写或绕过 C51 已发布的 PipeShell maker history、front/back cap sewing、MapperSewing ElementMap 传播和 Body additive/subtractive replay。
- 当前 capability 仍必须保持 `supported_c51s4_pipe_advanced_with_exact_source_blockers`，并保留 `partdesign_pipe_transformation_laws_source_commented` 与 `partdesign_pipe_spine_tangent_source_commented`，直到 S6 的 C51 regression 与 C6 product fixtures / focused tests 同时通过。
- S6 若发布 C6 product extension，只能把已实现且有 fixture / test 证明的 law / tangent blocker 从 exact blocker 移到 product extension boundary；未实现项必须继续保留 precise blockers 或 `remaining_gaps`，不得清空。
- capability 文案只允许写 `CAD Core product extension`，不得写 full PartDesign Pipe coverage、不得写 FreeCAD parity，也不得让 adapter 承载 law / tangent 业务语义。
- Interpolation、branch junction、invalid law data 的 diagnostics 必须 locatable；S6 不能用 Linear / S-shape fallback、compound all-edge collection、输出几何猜测或 fixture-name path patch 关闭这些诊断边界。
- 已回写 `C6M1-SCOPE-401/402/501`、`C6M1-BLK-401/501`、`C6M1-ORC-401/501`，并同步 release gate 分类和 validation notes。

## 目标

锁定 C6 product extension 对 PipeShell maker history、front/back sewing、Body replay 和 capability 的发布规则，防止 C6 law / tangent 改动破坏 C51 已支持的 Pipe advanced 能力。

## FreeCAD 依据

| 语义 | 源码 | 关键行为 |
| --- | --- | --- |
| PipeShell 构建 | `FeaturePipe.cpp::Pipe::execute()` | 对每组 wires 创建 `BRepOffsetAPI_MakePipeShell`，调用 `setupAlgorithm()`，再 Add / SetLaw。 |
| maker history | `TopoShape::makeElementShape(mkPS, wires, OpCodes::PipeShell)` | PipeShell maker history 是已发布边界。 |
| front/back sewing | `BRepBuilderAPI_Sewing` + `MapperSewing` | 非闭合 shell 生成 front/back face 并通过 MapperSewing 传播 ElementMap。 |
| Body replay | `FeatureAddSub` fuse / cut | AdditivePipe / SubtractivePipe 必须保持 Body additive fuse / subtractive cut 语义。 |

## capability 规则

| 项 | 发布要求 |
| --- | --- |
| status | 从 `supported_c51s4_pipe_advanced_with_exact_source_blockers` 改为 C6 product extension 状态，只在 S6 代码和 tests 通过后。 |
| exact blockers | law / tangent blocker 只有在对应 C6 fixture 和 focused test 落地后才删除或改成 product extension boundary。 |
| supported list | 写 `CAD Core product extension: Transformation=Linear` 等，不写 FreeCAD parity。 |
| diagnostics | Interpolation / branch junction / invalid law data 必须 locatable。 |
| remaining_gaps | 不因部分实现清空其他 product-contract-required 项。 |

## 发布闸门

| gate | S6 必须证明 | 不允许 |
| --- | --- | --- |
| C51 regression | `c51m4/partdesign-pipe-fixed-round-body`、`partdesign-pipe-auxiliary-binormal-modes`、`partdesign-pipe-selected-spine-multisection`、`partdesign-pipe-source-backed-blockers` 仍通过；`part_sweep:pipeshell_history`、`part_design_pipe:sewing`、`history_consumed:generated_modified`、`Body additive fuse replay` 和 subtractive replay 仍存在。 | 为了 law / tangent 改动重写 C51 PipeShell history 主路径，或删除 MapperSewing / Body replay 断言。 |
| C6 product fixtures | Linear / S-shape SetLaw、SpineTangent / AuxiliarySpineTangent ledger、Interpolation boundary 的 fixture 和 focused tests 同时通过。 | 只靠 capability 文案、docs 或单个 fixture 发布。 |
| capability publication | `part_design.pipe` status、supported list、exact blockers、remaining_gaps 与 S6 实现状态逐项一致。 | 写 full PartDesign Pipe coverage、FreeCAD parity，或把未实现项从 blockers / remaining_gaps 中消失。 |
| adapter boundary | adapter 只暴露 capability contract 和 diagnostics，不解析或修补 law / tangent 业务语义。 | 在 adapter 层承载 law DTO、continuous-edge ledger、fallback 或输出修剪。 |

## 必须回写的矩阵行

- `C6M1-SCOPE-401`、`C6M1-SCOPE-402`、`C6M1-SCOPE-501`。
- `C6M1-BLK-401`、`C6M1-BLK-501`。
- `C6M1-ORC-401`、`C6M1-ORC-501`。

## 验收标准

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'supported_c51s4_pipe_advanced_with_exact_source_blockers|partdesign_pipe_transformation_laws_source_commented|partdesign_pipe_spine_tangent_source_commented' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py
rg -n 'MapperSewing|PipeShell|front/back|Body additive fuse replay|CAD Core product extension|C6M1-SCOPE-401|C6M1-BLK-501' docs/CADCore6.0/C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线
for f in docs/CADCore6.0/C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
git diff --check -- docs/CADCore6.0
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线/工作步骤细分 --format markdown
```

通过条件：capability 改动前置条件明确；S6 发布必须同时验证 C51 Pipe regression 和 C6 product extension；S5 文件名和标题标记为 `【已实现】` 后，队列显示 S0-S5 已完成、S6 待执行。

## 非目标

- 不重写 C51 PipeShell history 主路径。
- 不把 capability 改成 full PartDesign Pipe coverage。
- 不在 adapter 层承载 law / tangent 业务语义。
