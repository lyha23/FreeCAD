# C13-M5 S4 phase family 扩展

## 目标

在 `c4m6` strict lane 关闭后，按语义家族扩展到其它 phase，避免全量 expected diff 变成不可关闭的大任务。

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
