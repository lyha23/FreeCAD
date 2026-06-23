# 【已实现】C5-M18-S5 Oracle fixtures 与 expected

状态：`done_s5_closed`

## Fixtures

| fixture | 用途 | expected |
| --- | --- | --- |
| `cad-core/fixtures/c51m5/partdesign-datum-folding-modes.json` | DatumCS normal order、DatumCS reversed support directions、DatumPlane Folding | `expected/partdesign-datum-folding-modes.freecad.json` |
| `cad-core/fixtures/c51m5/partdesign-datum-folding-diagnostics.json` | 六类 invalid Folding diagnostics | `expected/partdesign-datum-folding-diagnostics.freecad.json` |

## Expected 字段

- Datum CoordinateSystem：`origin`、`x_axis`、`y_axis`、`z_axis`、`map_mode`。
- DatumPlane：`origin`、`x_axis`、`normal`、`map_mode`。
- Diagnostics：`diagnostic_codes` 和 FreeCADCmd message evidence。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
FREECADCMD=/home/user/.local/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c51m5/partdesign-datum-folding-modes.json --check
FREECADCMD=/home/user/.local/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c51m5/partdesign-datum-folding-diagnostics.json --check
```
