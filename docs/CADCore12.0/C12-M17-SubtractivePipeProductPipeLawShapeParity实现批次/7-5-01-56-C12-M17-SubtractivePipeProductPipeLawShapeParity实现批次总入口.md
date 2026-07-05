# C12-M17 SubtractivePipe product PipeLaw Shape parity 实现批次总入口

## 目标

把 `SubtractivePipe` product PipeLaw 从“主响应发布 removed tool”修正为“主响应发布 FreeCAD post-cut feature `Shape`”，同时保留 CAD Core product PipeLaw 和几何共线 BSpline 轴引用作为产品扩展。

## 为什么现在做

当前 C12-M1..M16 队列已关闭，open wire raw edge mesh 已在 2026-07-05 live audit 中确认为 current-supported。用户明确选择处理 capability 文档中的第一类非原生偏差，并明确要求继续支持第二类 PartDesign axis product extension，因此 C12-M17 是新的 implementation 批次。

## 范围

本包只处理：

- Body 内 `PartDesign::SubtractivePipe`。
- `PipeLaw` 来源为 CAD Core product extension，例如 `Interpolation LawSamples`。
- 主响应 `Shape` / mesh / subshapes / namedShape / bbox / volume。
- `AddSubShape` 仍保存 pre-boolean removed tool cache。
- capability / expected / adapter wording 的边界同步。

本包不处理：

- strict FreeCAD axis parity。
- Part Sweep helper lifecycle。
- persistent document/session/cache。
- 前端 consumer。

## 队列

1. `C12-M17 工作步骤总入口`
2. `C12-M17 S0 live 基线与偏差边界冻结`
3. `C12-M17 S1 FreeCAD source 与 current publish 路径复核`
4. `C12-M17 S2 red fixture 与 expected 迁移设计`
5. `C12-M17 S3 product PipeLaw 主 Shape 实现`
6. `C12-M17 S4 capability / expected / adapter 口径同步`
7. `C12-M17 S5 发布闸门`

队列状态：closed；S5 标记 `【已实现】` 后，`step_goal_queue.py .../工作步骤细分 --format markdown` 预期只输出 markdown 表头。

## 退出口径

- 当前退出口径：`implemented_freecad_main_shape_parity_product_law_retained`。
- `implemented_freecad_main_shape_parity_product_law_retained`：实现完成，主 `Shape` parity 与 product PipeLaw extension 共存。
- `blocked_missing_expected_or_test_surface`：无法建立 focused expected / current mismatch。
- `blocked_contract_conflict`：实现发现主响应必须保留 tool shape 的已批准产品契约证据。
