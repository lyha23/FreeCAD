# C6-M1 S5 PipeShellHistoryCapability 专项复审

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

## 必须回写的矩阵行

- `C6M1-SCOPE-401`、`C6M1-SCOPE-402`、`C6M1-SCOPE-501`。
- `C6M1-BLK-401`、`C6M1-BLK-501`。
- `C6M1-ORC-401`、`C6M1-ORC-501`。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'supported_c51s4_pipe_advanced_with_exact_source_blockers|partdesign_pipe_transformation_laws_source_commented|partdesign_pipe_spine_tangent_source_commented' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py
rg -n 'MapperSewing|PipeShell|front/back|Body additive fuse replay|CAD Core product extension' docs/CADCore6.0/C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线
```

通过条件：capability 改动前置条件明确；S6 发布必须同时验证 C51 Pipe regression 和 C6 product extension。

## 非目标

- 不重写 C51 PipeShell history 主路径。
- 不把 capability 改成 full PartDesign Pipe coverage。
- 不在 adapter 层承载 law / tangent 业务语义。
