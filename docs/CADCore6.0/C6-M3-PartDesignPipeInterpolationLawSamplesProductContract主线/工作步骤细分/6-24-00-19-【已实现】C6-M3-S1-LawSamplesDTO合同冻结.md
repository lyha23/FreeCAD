# 【已实现】C6-M3 S1 LawSamples DTO 合同冻结

## 完成结论

- S1 已冻结 `Transformation=Interpolation` 的 `LawSamples` CAD Core product contract。该合同不是 FreeCAD parity；FreeCAD 只提供 enum-only authority。
- `LawSamples` 是 request-local DTO，不是前端或后端跨请求状态。C6-M3 S1 只承认数组对形态：`[[parameter, scale], ...]`。
- 成功响应 metadata 必须带 `pipe_law.kind=Interpolation`、`pipe_law.source=cad_core_product_contract`、`pipe_law.contract=cad_core_product_contract`、`pipe_law.domain=[0.0,1.0]`、归一化 samples 和 `pipe_law.no_fallback=true`。
- diagnostics 已覆盖 missing / malformed samples、非有限 / 越界 / 非单调 parameter、非正 / 非有限 scale、invalid shape 和 no executable contract。
- `cad-core/fixtures/c6m1/partdesign-pipe-interpolation-law-boundary.json` 本步不修改；它作为 S5 创建 C6-M3 成功 fixture 的迁移种子。
- S1 只更新 C6-M3 文档和矩阵；不改 C++、fixture、capability、OCCT law、frontend/editor。

## 合同定义

### Request

- `Transformation` 必须是 `Interpolation`。
- `LawSamples` 必须存在，且为至少两个 sample 的 JSON array。
- 每个 sample 的唯一合同形态是二元 array：`[parameter, scale]`。对象、标量列表、缺项、额外项、字符串、`null` 或少于两个 samples 均为 malformed。
- `parameter` 必须为有限 number，位于 `[0,1]`，严格递增，且首个 sample 必须是 `0.0`、最后一个 sample 必须是 `1.0`。CAD Core 不排序、不 clamp、不补端点。
- `scale` 必须为有限正数。CAD Core 不做绝对值修复、不丢弃坏 sample、不填默认 scale。
- `LawSamples` 只在单次 recompute 请求内生效；不保存 law、shape、NamedShape、ElementMap 或 mesh 作为跨请求状态。

### Response Metadata

有效 DTO 且后续 S3/S4/S5 实现可执行后，响应必须保留：

- `pipe_law.kind=Interpolation`
- `pipe_law.source=cad_core_product_contract`
- `pipe_law.contract=cad_core_product_contract`
- `pipe_law.domain=[0.0,1.0]`
- `pipe_law.samples=[{parameter,scale},...]`
- `pipe_law.no_fallback=true`

这里的 normalized samples 只表示响应 metadata 用 `{parameter, scale}` 对接受样本做规范表达，不允许修复或重排请求数据。

### Diagnostics

| code | 触发条件 | 必须行为 |
| --- | --- | --- |
| `missing_pipe_law_samples` | `Transformation=Interpolation` 但缺少 `LawSamples` | `object.status=error`，定位 `LawSamples`，无 shape，无 Linear / S-shape fallback。 |
| `invalid_pipe_law_samples` | malformed sample、少于两个 samples、parameter 非有限 / 越界 / 非单调 / 端点缺失、scale 非正 / 非有限 | `object.status=error`，定位 `LawSamples`，`pipe_law.status=invalid`，无 fallback。 |
| `invalid_pipe_law_shape` | samples 合同有效，但 profile / spine / PipeShell 输入 shape 无效 | `object.status=error`，定位 shape 属性，保留 Interpolation product contract metadata，无 fallback shape。 |
| `no_executable_pipe_law_contract` | samples 合同有效，但 law kernel 或 executor 尚无可执行路径 | `object.status=error`，定位 `Transformation`，不得退回 `product_contract_required` 作为成功路径，也不得 fallback。 |

## C6-M1 Fixture 迁移策略

- `cad-core/fixtures/c6m1/partdesign-pipe-interpolation-law-boundary.json` 当前已经包含合法 `LawSamples=[[0.0,1.0],[1.0,1.4]]`，因此它是 C6-M3 成功 fixture 的输入种子。
- S1 不编辑、不移动、不新增 fixture。S5 在 S3/S4 实现落地后，创建 `cad-core/fixtures/c6m3/partdesign-pipe-interpolation-law-product.json`，可从该 legacy input 复制建模 graph，并断言成功响应 metadata、shape 和 no-fallback。
- 旧 C6-M1 boundary 断言当前仍代表历史 `product_contract_required` 边界；后续测试切换必须在 S5 里和 capability 发布一起处理，不能在 S1 单独改 expected。

## 非目标

- 不实现 OCCT law。
- 不改 capability。
- 不把 `LawSamples` 设计成前端长期状态。
- 不允许 implicit Linear / S-shape fallback。
- 不编辑或新增 fixture。
- 不做 frontend/editor 工作。

## 更新产物

- `矩阵/c6m3_pipe_interpolation_law_input_contract_matrix.tsv`
- `矩阵/c6m3_pipe_interpolation_law_oracle_fixture_matrix.tsv`
- `矩阵/c6m3_pipe_interpolation_law_non_goal_registry.tsv`
- `矩阵/c6m3_pipe_interpolation_law_validation_matrix.tsv`
- `矩阵/c6m3_pipe_interpolation_law_blocker_queue.tsv`
- `矩阵/c6m3_pipe_interpolation_law_backend_gap_classification.tsv`
- `矩阵/c6m3_pipe_interpolation_law_scope_review_matrix.tsv`
- C6-M3 主入口与工作步骤总入口的 S1 状态

## 验收结果

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'LawSamples|cad_core_product_contract|invalid_pipe_law_samples|missing_pipe_law_samples|Interpolation' docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/矩阵 docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/工作步骤细分
for f in docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
git diff --check -- docs/CADCore6.0
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/工作步骤细分 --format markdown
```

- `rg`：命中 LawSamples DTO、CAD Core product contract metadata、missing / invalid diagnostics 和 Interpolation 已实现步骤文档。
- TSV field-count check：无输出。
- `git diff --check -- docs/CADCore6.0`：无输出。
- 队列：S1 已跳过，队列头前进到 S2。
