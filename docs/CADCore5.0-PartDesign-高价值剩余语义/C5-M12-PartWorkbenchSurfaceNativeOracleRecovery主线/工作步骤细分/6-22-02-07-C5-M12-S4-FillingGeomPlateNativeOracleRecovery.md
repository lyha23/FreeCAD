# C5-M12-S4 Filling / GeomPlate native oracle recovery

状态：`pending_C5M12-S4_filling_geomplate_oracle`

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

- collectable Filling / GeomPlate blockers 替换为 FreeCADCmd expected。
- uncollectable helper paths 保留精确 blocker，记录 FreeCADCmd / wrapper evidence。
- 更新 `C5M12-BLK-401`、`C5M12-SCOPE-401`、`C5M12-ORC-401`。

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
