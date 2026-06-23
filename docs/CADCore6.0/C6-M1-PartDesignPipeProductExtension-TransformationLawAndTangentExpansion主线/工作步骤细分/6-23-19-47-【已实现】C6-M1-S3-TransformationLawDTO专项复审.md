# 【已实现】C6-M1 S3 TransformationLawDTO 专项复审

## 完成结论

- S3 已裁决 `C6M1-SCOPE-201/202/203`：`Linear` 与 `S-shape` 按 FreeCAD 注释块作为 `freecad_source_commented` source hint，升级为 CAD Core `cad_core_product_contract`；`Interpolation` 不接受 `LawSamples` 几何合同，保留稳定 `product_contract_required` 诊断边界。
- 已回写 `C6M1-IN-201/202/203`、`C6M1-ORC-201/202/203`、`C6M1-BLK-201/202/203`，并同步 `C6M1-SCOPE-201/202/203` 与 `C6M1-CAT-201/203` 的 routing status。
- `Linear` / `S-shape` 现在是 S6 可实现的 backend gap：实现顺序必须先解析 request-local `PipeLaw` DTO，再在 PipeShell helper / `topo_shape_expansion` 接入 `SetLaw`，最后用 focused fixture 与 capability product-contract 断言发布。
- `Interpolation` 仍不是 backendGap：S6 只允许把当前 source-blocked 诊断收窄为 `product_contract_required` 边界 fixture，不得回退到 Linear / S-shape，也不得引入跨请求 law cache。
- 本步骤没有执行 FreeCADCmd、没有修改 C++、没有新增 fixture、没有更新 capability，也没有把 cad-core 输出写成 FreeCAD expected。

## 目标

定义 Pipe law product DTO，使 `Transformation=Linear/S-shape/Interpolation` 有明确 request contract、诊断边界和实现顺序。S3 只定合同和矩阵，不直接写 C++。

## FreeCAD 依据

| 项 | FreeCAD 依据 | C6 解释 |
| --- | --- | --- |
| `Linear` | 注释块中 `Law_Linear::Set(0, 1, 1, ScalingData[0].x)` | 可作为产品扩展默认合同的 source hint。 |
| `S-shape` | 注释块中 `Law_S::Set(0, 1, ScalingData[0].y, 1, ScalingData[0].x, ScalingData[0].z)` | 可作为产品扩展默认合同的 source hint。 |
| `Interpolation` | enum 暴露但无分支 | 必须由 CAD Core 产品合同定义，不得写成 FreeCAD parity。 |
| `SetLaw` | `Pipe::execute()` 在 `scalinglaw` 存在时对 profile / section 调 `mkPS.SetLaw()` | cad-core 应把 low-level API 放在 `part/topo_shape_expansion` 或 PipeShell helper 中。 |

## DTO 定稿

| 字段 | 用途 | 约束 |
| --- | --- | --- |
| `Transformation` | FreeCAD enum label | `Linear` / `S-shape` 进入 S6 实现；`Interpolation` 只进入诊断边界。 |
| `ScalingData` | 兼容 FreeCAD 注释块的 vector payload | Linear 要求 `ScalingData[0].x` 有限；S-shape 要求 `ScalingData[0].x/y/z` 有限；不得隐式补默认值。 |
| `PipeLaw` | C6 product extension 明确 law kind 和参数 | 只在单次请求内解析，不替代 FreeCAD 属性，不持久化 backend law cache。 |
| `LawSamples` | Interpolation 采样点候选 | C6-M1 不接受该字段为几何合同；后续若要实现必须 reopen 产品 spec。 |
| `law_source_evidence` | 输出 metadata | Linear / S-shape 同时记录 `freecad_source_commented` 与 `cad_core_product_contract`；Interpolation 记录 `product_contract_required`。 |

## DTO 裁决

| scope | 裁决 | S6 顺序 |
| --- | --- | --- |
| `C6M1-SCOPE-201` | `Linear` 成为 CAD Core 产品合同：`Law_Linear::Set(0, 1, 1, ScalingData[0].x)` 只作为 source hint，合同要求 finite `x`、domain `[0,1]`、start scale `1`、end scale `x`。 | 先落 request-local `PipeLaw(kind=Linear)` 和 invalid `ScalingData` 诊断，再接 PipeShell `SetLaw`，最后更新 fixture / capability。 |
| `C6M1-SCOPE-202` | `S-shape` 成为 CAD Core 产品合同：`Law_S::Set(0, 1, ScalingData[0].y, 1, ScalingData[0].x, ScalingData[0].z)` 只作为 source hint，合同要求 finite `x/y/z`，并禁止 bbox 或隐式默认推导。 | 先落 request-local `PipeLaw(kind=S-shape)` 和缺失 / 非有限数诊断，再接 PipeShell `SetLaw`，最后更新 fixture / capability。 |
| `C6M1-SCOPE-203` | `Interpolation` 不实现 `LawSamples`。因 FreeCAD 只有 enum 无执行分支，C6-M1 固定为 `product_contract_required` 诊断边界。 | S6 只做边界 fixture / diagnostic assertion，不接 geometry law；若产品要 sample interpolation，必须新开 spec。 |

## 必须回写的矩阵行

- `C6M1-IN-201`、`C6M1-IN-202`、`C6M1-IN-203`。
- `C6M1-ORC-201`、`C6M1-ORC-202`、`C6M1-ORC-203`。
- `C6M1-BLK-201`、`C6M1-BLK-202`、`C6M1-BLK-203`。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'C6M1-IN-20[1-3]|PipeLaw|ScalingData|LawSamples|freecad_source_commented|cad_core_product_contract' docs/CADCore6.0/C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线
rg -n 'Law_Linear|Law_S|SetLaw|ScalingData' src/Mod/PartDesign/App/FeaturePipe.cpp
```

通过条件：Linear、S-shape、Interpolation 都有明确 DTO 裁决；不能实现的项保留 close condition，不得在 S6 里临时猜。S3 文件名和标题标记为 `【已实现】` 后，队列显示 S0-S3 已完成、S4-S6 待执行。

## 非目标

- 不要求 FreeCADCmd expected。
- 不把 cad-core output 写成 FreeCAD expected。
- 不引入跨请求 law cache。
