# C6-M1 S3 TransformationLawDTO 专项复审

## 目标

定义 Pipe law product DTO，使 `Transformation=Linear/S-shape/Interpolation` 有明确 request contract、诊断边界和实现顺序。S3 只定合同和矩阵，不直接写 C++。

## FreeCAD 依据

| 项 | FreeCAD 依据 | C6 解释 |
| --- | --- | --- |
| `Linear` | 注释块中 `Law_Linear::Set(0, 1, 1, ScalingData[0].x)` | 可作为产品扩展默认合同的 source hint。 |
| `S-shape` | 注释块中 `Law_S::Set(0, 1, ScalingData[0].y, 1, ScalingData[0].x, ScalingData[0].z)` | 可作为产品扩展默认合同的 source hint。 |
| `Interpolation` | enum 暴露但无分支 | 必须由 CAD Core 产品合同定义，不得写成 FreeCAD parity。 |
| `SetLaw` | `Pipe::execute()` 在 `scalinglaw` 存在时对 profile / section 调 `mkPS.SetLaw()` | cad-core 应把 low-level API 放在 `part/topo_shape_expansion` 或 PipeShell helper 中。 |

## DTO 初稿

| 字段 | 用途 | 约束 |
| --- | --- | --- |
| `Transformation` | FreeCAD enum label | `Linear` / `S-shape` / `Interpolation` 进入 C6。 |
| `ScalingData` | 兼容 FreeCAD 注释块的 vector payload | Linear 至少 1 个点；S-shape 至少 1 个点；数值必须有限。 |
| `PipeLaw` | C6 product extension 明确 law kind 和参数 | 不替代 FreeCAD 属性，只做 request-local resolved metadata。 |
| `LawSamples` | Interpolation 可选采样点 | 只有产品决定使用 sample-based interpolation 时才支持。 |
| `law_source_evidence` | 输出 metadata | 记录 `freecad_source_commented` 或 `cad_core_product_contract`。 |

## 必须裁决

| scope | 裁决 |
| --- | --- |
| `C6M1-SCOPE-201` | Linear 是否直接按注释块默认参数实现。 |
| `C6M1-SCOPE-202` | S-shape 是否直接按注释块默认参数实现。 |
| `C6M1-SCOPE-203` | Interpolation 是实现 sample contract，还是保持 `productContractRequired`。 |

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

通过条件：Linear、S-shape、Interpolation 都有明确 DTO 裁决；不能实现的项保留 close condition，不得在 S6 里临时猜。

## 非目标

- 不要求 FreeCADCmd expected。
- 不把 cad-core output 写成 FreeCAD expected。
- 不引入跨请求 law cache。
