# C5-M12-S4 Filling / GeomPlate native oracle recovery

状态：`done_C5M12-S4_filling_geomplate_oracle_recovery`

## 目标

恢复或精确收窄 Filling 与 GeomPlate 的 native helper expected blockers：Filling support/order/G2/non-default params/non-boundary edge，GeomPlate G1 curve-on-surface 与 ProjectedCurve2d native expected oracle。

## 必读

- S1 probe 结论。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/`
- `cad-core/src/part/part_filling.cpp`
- `cad-core/src/part/part_geomplate.cpp`
- `cad-core/tools/collect_freecad_expected.py`

## 产物

- 新增 `cad-core/fixtures/c5m12/part-filling-non-boundary-edge-no-support-order.json` 与 FreeCADCmd expected，覆盖 S1 确认可采的 non-boundary edge without support/order representative。
- Filling `surface`、support/order、non-default params、non-boundary support/order 仍保留 FreeCADCmd blocker，并在 expected metadata 中记录 probe case、error 与未采字段。
- GeomPlate G1 curve-on-surface 保留 native-hidden / diagnostic-only blocker；ProjectedCurve2d 保留 FreeCADCmd RuntimeError blocker。
- 已更新 `C5M12-BLK-401`、`C5M12-SCOPE-401`、`C5M12-ORC-401` 与 root C5/C3 相关矩阵。

## 非目标

- 不声明 native `Part::FilledFace` DocumentObject。
- 不声明 GUI GeomPlate / Surface Workbench feature。
- 不实现 persistent helper wrapper lifecycle。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/collect_freecad_expected.py --phase c5m12 --check --skip-unsupported
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures
```
