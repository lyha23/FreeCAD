# C6-M3 PartDesign Pipe Interpolation LawSamples Product Contract 方案

## 背景

C6-M1 已经把 `Transformation=Linear/S-shape` 从 FreeCAD source-commented blocker 升级为 CAD Core product extension，同时把 `Transformation=Interpolation` 固定成 `product_contract_required` 诊断边界。C6-M2 已恢复 expected fixture regression gate。现在可以安全地重开 Interpolation，但必须先定义产品合同，不能从 fixture 输出倒推几何。

## 核心判断

FreeCAD `FeaturePipe.cpp` 只提供 Interpolation enum，不提供执行分支。因此 C6-M3 的正确口径是：

- 语义来源：FreeCAD enum surface + CAD Core product requirements。
- 实现归属：`cad-core/src/part_design/feature_pipe.cpp` 解析 DTO，`cad-core/src/part/topo_shape_expansion.cpp` 承接 OCCT law / PipeShell，`runtime/capability_contract.cpp` 发布能力。
- 验收方式：CAD Core fixtures + focused tests + capability tests + stage regression。
- 禁止路径：不 fallback 到 Linear / S-shape，不写 FreeCAD parity expected，不使用跨请求缓存。

## LawSamples 最小合同

| 字段 | 合同 |
| --- | --- |
| `Transformation` | 必须为 `Interpolation`。 |
| `LawSamples` | 数组，至少 2 个 sample；每个 sample 可先支持 `[parameter, scale]`，S1 决定是否同时接受 object form。 |
| `parameter` | 有限数，范围 `[0,1]`，严格递增。 |
| `scale` | 有限正数；不能为 0、负数、NaN 或 Inf。 |
| domain coverage | 首个 sample 应覆盖 `0.0`，末个 sample 应覆盖 `1.0`；如 S1 允许自动端点补齐，必须写明 metadata。 |
| response metadata | `pipe_law.kind=Interpolation`、`contract=cad_core_product_contract`、`samples`、`domain`、`source=cad_core_product_contract`。 |
| diagnostics | 缺失、sample 数不足、参数无序、参数越界、scale 非法、schema 非法都必须有 locatable code。 |

## 最小完整语义批次

本包不做单 fixture 小修，必须一次覆盖：

1. 成功 additive pipe Interpolation law。
2. 成功 subtractive pipe Interpolation law 或明确推迟并登记 blocker。
3. 非法 `LawSamples` diagnostics：缺失、少于 2 点、参数非递增、scale 非正。
4. capability 从 remaining gap 迁移到 supported product extension。
5. C6-M1 Interpolation boundary fixture 的迁移或替代：旧 `product_contract_required` 不能和新成功合同互相矛盾。

## 风险

- OCCT 没有直接可用的 sampled law wrapper 时，S3 必须先做低层能力验证，不能在 executor 中伪造几何。
- 如果 sampled law 只影响 metadata 而没有改变 PipeShell geometry，不得发布为几何合同。
- 如果成功几何对 OCCT 版本敏感，应按 C6-M2 的方式写局部 `bbox_delta` / known environment gap，而不是放宽全局断言。

## 下一步

按 `工作步骤细分/` 队列执行 S0-S6。S0-S2 只做证据和合同冻结；S3-S5 才允许代码、fixture 和 capability 改动；S6 发布阶段回归状态。
