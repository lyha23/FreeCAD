# C6-M3 PartDesign Pipe Interpolation LawSamples Product Contract 主线

本目录是 CADCore6.0 的第三条主线。它从 C6-M1 已关闭的 `partdesign_pipe_interpolation_law_product_contract_required` precise remaining gap 出发，为 PartDesign Pipe `Transformation=Interpolation` 定义并实现 CAD Core 自有 `LawSamples` 产品合同。

## 为什么现在做 C6-M3

- C6-M1 已把 `Transformation=Linear/S-shape` 和 selected tangent expansion 发布为 CAD Core product extension。
- C6-M2 已恢复 expected fixture 阶段回归可信度，后续新能力不再混入旧 expected 漂移。
- `Interpolation` 在 FreeCAD 源码中只有 enum，没有可执行 law 分支；因此 C6-M3 只能写成 CAD Core product contract，不能声明 FreeCAD parity。

## 主线边界

- 本包只处理 `PartDesign::AdditivePipe` / `SubtractivePipe` 的 `Transformation=Interpolation` + request-local `LawSamples`。
- `LawSamples` 最小合同：至少 2 个 sample；参数在 `[0,1]` 且严格递增；scale 为有限正数；首尾 sample 明确覆盖 pipe law domain。
- 成功输出必须保留 `pipe_law.kind=Interpolation`、`pipe_law.contract=cad_core_product_contract`、sample 归一化 metadata、PipeShell history 和 Body replay。
- 非法 `LawSamples` 必须返回稳定、locatable diagnostics，不能 fallback 到 Linear / S-shape。
- 本包不做 FreeCAD parity，不新增 GUI/TaskPanel 行为，不引入跨请求 law cache。

## 发布状态

- S6 已关闭 C6-M3 release gate：`cmake --build build` 通过；阶段回归 `tests.test_p7_features tests.test_expected_fixtures tests.test_adapters` 为 `Ran 182 tests in 65.996s`，`OK (skipped=29)`；heavy `tests.test_p6_topology tests.test_p7_features tests.test_expected_fixtures tests.test_adapters` 为 `Ran 218 tests in 74.459s`，`OK (skipped=29)`。
- heavy 必跑原因：S5 触达 PipeShell law/history、Body replay fixtures、expected gate 和 capability schema。验证无残留失败，因此 `C6M3-BLK-501/CAT-501/SCOPE-501/ORC-501` 关闭。

## 队列

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/工作步骤细分 --format markdown
```
