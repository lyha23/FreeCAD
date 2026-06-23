# 【已实现】C5-M18 DatumFoldingAttachEngine 方案

状态：`done_expected_backed_capability_closed`

## 当前基线

C5-M17 收口 conic landmark 后，Datum AttachEngine exact blocker 的 live list 为：

```text
Folding, TangentU, TangentV, IntersectionPoint
```

这四项不属于同一实现 family。C5-M18 只推进 `Folding`，因为它有完整的 FreeCAD 3D branch、明确的四条 line support 合同和可采集的 DatumPlane / CoordinateSystem expected。

## 范围

| 项 | 本包处理 | 说明 |
| --- | --- | --- |
| `Folding` DatumPlane | 已实现 | 输出 `origin`、`x_axis`、`normal` 并与 FreeCADCmd expected 比对 |
| `Folding` Datum CoordinateSystem | 已实现 | 输出 `origin`、`x_axis`、`y_axis`、`z_axis` 并与 FreeCADCmd expected 比对 |
| reversed support directions | 已实现 | 覆盖 FreeCAD shared vertex sign normalization |
| invalid diagnostics | 已实现 | missing support、non-straight、disconnected、parallel axes、axis/edge parallel、invalid cosine |
| `TangentU/V` | 否 | surface tangent branch，保留 exact blocker |
| `IntersectionPoint` | 否 | face/face intersection route，保留 exact blocker |
| GUI/session/full BREP | 否 | 保持 stateless CAD Core 边界 |

## FreeCAD 调用链

1. `AttachEngine3D::AttachEngine3D()` 为 `mmFolding` 注册四个 `rtLine` refs。
2. `_calculateAttachedPlacement()` 的 `mmFolding` branch 按 `edgeA, axisA, axisB, edgeB` 读取四个 edge line。
3. branch 用前两个 support 找共享顶点，再要求后两个 support 也共享同一点。
4. branch 根据共享顶点在每条 line 的起点还是终点，把 direction sign 统一成从共享点向外。
5. branch 调用 `calculateFoldAngle(dirs[1], dirs[2], dirs[0], dirs[3])`。
6. branch 用 `axisA x axisB` 的 normal 绕 `axisA` 旋转 `-angle`，再反向得到 `SketchNormal`；`SketchXAxis` 使用 `axisA` direction，`SketchBasePoint` 使用共享点。

## cad-core 实现边界

- `cad-core/src/part_design/datum_attachment.h`：
  - 新增 `Folding` resolver，只接受四个 resolved straight edge supports。
  - 新增 shared vertex / sign normalization / fold-angle helper。
  - 对 invalid supports 返回 diagnostics，不落 default placement。
  - dispatch 只开放 DatumPlane 和 Datum CoordinateSystem。
- `cad-core/src/part_design/datum_plane.cpp`：
  - 为 expected parity 输出 `origin`、`x_axis`、`normal`。
- `cad-core/tools/collect_freecad_expected.py`：
  - FreeCADCmd collector 同步采集 DatumPlane placement evidence。
- `cad-core/src/runtime/capability_contract.cpp`：
  - `Folding selected MapMode` 加入 supported。
  - `c51m5/partdesign-datum-folding-modes` 与 diagnostics 加入 fixtures。
  - `datum_attach_engine_remaining_modes` 删除 `Folding`，保留 `TangentU`、`TangentV`、`IntersectionPoint`。

## Oracle batch

| fixture | 场景 | expected 字段 |
| --- | --- | --- |
| `fixtures/c51m5/partdesign-datum-folding-modes.json` | DatumCS Folding、reversed support directions、DatumPlane Folding | origin, axes, normal, map_mode |
| `fixtures/c51m5/partdesign-datum-folding-diagnostics.json` | missing fourth support, non-straight, disconnected, parallel axes, axisA/edgeA parallel, invalid cosine | diagnostic_codes, status=error |

## 验收分层

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
git diff --check -- cad-core docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M18-DatumFoldingAttachEngine主线 docs/CADCore5.0-PartDesign-高价值剩余语义/README.md docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
for f in docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M18-DatumFoldingAttachEngine主线/矩阵/*.tsv docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
```

Focused oracle / tests：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
FREECADCMD=/home/user/.local/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c51m5/partdesign-datum-folding-modes.json --check
FREECADCMD=/home/user/.local/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c51m5/partdesign-datum-folding-diagnostics.json --check
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51x_datum_folding_modes_match_expected tests.test_p7_features.CadCoreP7FeatureTest.test_c51x_datum_folding_invalid_diagnostics
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

阶段收口：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
```

## 禁止捷径

- 不按 fixture 名称、坐标常量或 bbox 倒推 placement。
- 不把 missing/non-straight/disconnected/fold-angle failure 降级成 default placement。
- 不在 adapter 层实现 AttachEngine 业务逻辑。
- 不把 `TangentU/V`、`IntersectionPoint` 顺带声明 supported。
- 不引入 cross-request backend attachment session 或 complete BREP 状态。
