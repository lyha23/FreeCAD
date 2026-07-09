# C13-M5 S4 phase family 扩展

## 目标

在 `c4m6` strict lane 关闭后，按语义家族扩展到其它 phase，避免全量 expected diff 变成不可关闭的大任务。

## 实现结果

- `compare_freecad_expected.py` 已增加 family-aware classification；非 `c4m6` diff 不再落入 `unclassified_phase_gap`，每个 diff 都带 `owner`、`owner_step`、`decision`、`source`、`freecad_authority`、`next_action`、`close_condition`。
- 首批 representative tranche 已选定并重生成同名 `cad-core-res`：`c3m1`、`c10m1`、`c12m12`、`c3m5`、`c3m6`。
- 五个代表 phase 的 strict report 均为 `red_classified`，这代表 S4 known-gap surface，不代表 expected 错误，也不把 phase 标记 green。
- `tests.test_freecad_expected_public_parity` 已断言代表 phase report 可生成、没有 anonymous/unclassified diff，且 `c4m6` 仍只允许 `intentional_protocol_divergence`。
- README、方案和矩阵已记录每个家族的 representative phase、strict status、known-gap id、原因、删除条件和下一步。

## 推进顺序

1. TopoNamingState / ElementMap / App::Link：`c4m6`、`p8`、`c3m1`。
2. Sketch / InternalShape / split fragment：`c10m1`、`c12m16`、`p2`、`p6`。
3. Part primitives / boolean / sweep / loft / pipe：`p8`、`c3m4`、`c12m12`、`c12m13`。
4. PartDesign Body / dress-up / pattern / hole：`c3m5`、`p7`、`c5*`、`c51*`。
5. Assembly / placement / App::Link：`c3m6`、`p8`。

## 每个家族关闭条件

- comparator report 已归类，不留 anonymous diff。
- 每个实现缺口有 FreeCAD source authority。
- cad-core-res 只按 expected discovery 重生成。
- focused tests 覆盖该家族的语义，不靠全量 JSON diff 唯一守门。
- known gap 有 id、原因、删除条件和下一步。

## 非目标

- 不把所有 phase 一次性设为必须 green。
- 不因为某个 phase 红灯就改 expected。
- 不在 feature executor 中做 fixture 名称分支。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/compare_freecad_expected.py --phase <phase> --write-current
python3 tools/compare_freecad_expected.py --phase <phase> --strict
python3 -m unittest <focused-test-module>
```
