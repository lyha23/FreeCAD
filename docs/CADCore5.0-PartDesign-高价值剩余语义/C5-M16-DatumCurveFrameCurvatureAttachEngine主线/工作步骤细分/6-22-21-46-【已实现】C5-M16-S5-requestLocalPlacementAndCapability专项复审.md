# 【已实现】C5-M16-S5 requestLocalPlacement / Capability 专项复审

## 目标

复核 C5-M16 的 request-local placement response、AttachmentOffset / MapReversed / MapPathParameter、writeback suggestions 和 capability release boundary。

## 工作内容

1. 确认 curve-frame helpers 只消费本次 request graph 中的 support shape 和 subname。
2. 确认 response 只能返回 placement、diagnostics、documentObjectUpdates / elementReferenceUpdates suggestions。
3. 确认 `ReferenceShadow.brep` 例外不扩展成完整 BREP 状态。
4. 确认 release gate 只移除本包 proven modes。
5. 确认 excluded family 仍留在 exact blocker 或 non-goal。

## S5 冻结结论

`C5M16-BLK-501` 的 release gate 合同：

- curve-frame helpers 只能消费本次 request graph 中的 support shape、subname、`MapPathParameter`、`AttachmentOffset` 和 `MapReversed`；不得保存跨请求 shape、curve-frame cache、backend attachment session 或其他 session 状态。
- response 只允许返回 placement、diagnostics、`documentObjectUpdates` / `elementReferenceUpdates` suggestions。`ReferenceShadow.brep` 仍是唯一 BREP 例外，且只能作为被引用单个 subshape 的旧几何快照证据，不能扩展成完整 BREP 状态、建模输入或长期缓存。
- 当前 live 队列仍显示 C5-M15 S6 pending；因此 C5-M16 S6 不得发布 capability，也不得并行修改同一个 `datum_attach_engine_remaining_modes` exact blocker。
- S6 允许删除的仅是 expected-backed proven modes：`FrenetNB`、`FrenetTN`、`FrenetTB`、`Concentric`、`SectionOfRevolution`、`AxisOfCurvature`、`Normal`、`Binormal`、`CenterOfCurvature`。如果某个 mode 只能拆成 precise blocker，不能从 exact blocker 中删除。
- 必须保留 excluded modes：`Folding`、`Focus1/2`、`Directrix1/2`、`Asymptote1/2`、`IntersectionPoint`、`TangentU/V` 和 GUI/session；它们只能由后续专包或 non-goal guard 处理。
- adapter capability test 的未来 S6 断言应对 proven modes 使用 `assertNotIn`，对 excluded modes 使用 `assertIn`。S5 只冻结测试计划，不改 `cad-core/tests/test_adapters.py`。

## capability 发布边界

| 项 | S6 允许移除 | 条件 |
| --- | --- | --- |
| `FrenetNB/TN/TB` | 是 | expected-backed modes + diagnostics 通过 |
| `Concentric` / `SectionOfRevolution` | 是 | curvature center expected-backed 或 precise blocker split |
| `AxisOfCurvature` / `Normal` / `Binormal` / `CenterOfCurvature` | 是 | alias expected-backed |
| `Folding` | 否 | 独立 fold-angle package |
| conic landmarks | 否 | 独立 conic landmark package |
| `IntersectionPoint` | 否 | source route 不清 |
| `TangentU/V` | 否 | 非本包 proven owner |

## 完成条件

- `C5M16-BLK-501` 具备 release gate 合同。
- adapter test expected list 能从本包 proven modes 精确删除，不影响 excluded modes。
- 根 README / 根矩阵 / 包内矩阵状态前进到 S6 pending；S5 不采 oracle、不改 code、不移除 exact blocker。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M16-DatumCurveFrameCurvatureAttachEngine主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
```
