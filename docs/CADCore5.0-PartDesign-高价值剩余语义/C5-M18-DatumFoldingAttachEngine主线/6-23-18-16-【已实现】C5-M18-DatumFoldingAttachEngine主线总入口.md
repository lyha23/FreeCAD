# 【已实现】C5-M18 DatumFoldingAttachEngine 主线

状态：`done_expected_backed_capability_closed`

本包承接 C5-M17 后仍留在 `part_design.datum_attachment.exact_blockers.datum_attach_engine_remaining_modes` 的 `Folding`。它只处理 FreeCAD `AttachEngine3D::_calculateAttachedPlacement()` 的 `mmFolding` 分支，把四条有序 straight line support 的 fold-angle placement 迁移到 stateless `cad-core`。

## 目标

- 支持 DatumPlane / Datum CoordinateSystem 的 `Folding` selected MapMode。
- 按 FreeCAD 顺序解析四条 `rtLine` support：`edgeA`、`axisA`、`axisB`、`edgeB`。
- 迁移 shared vertex、support direction sign、`calculateFoldAngle()`、`SketchBasePoint` / `SketchXAxis` / `SketchNormal` placement。
- 用 FreeCADCmd expected 固化成功 fixture，并用 FreeCADCmd message evidence 固化 invalid diagnostics。
- 从 capability exact blocker 中删除 `Folding`；发布后只剩 `TangentU`、`TangentV`、`IntersectionPoint`。
- 保持 CAD Core 无状态边界：后端只消费本次 request graph，不保存 backend attachment session，不传完整 BREP；`ReferenceShadow.brep` 仍只允许作为单 referenced subshape snapshot 例外。

## 当前基线

- C5-M14 至 C5-M17 已关闭 proximity point、3D plane、curve-frame / curvature、conic landmark families。
- C5-M18 发布后 exact blocker 只保留 `TangentU`、`TangentV`、`IntersectionPoint`。
- GUI Attachment editor、TaskPanel、ViewProvider、cross-request backend state 和 complete BREP 仍为 non-goal。

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| mode registration | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1313` | `mmFolding` 注册四个 `rtLine` refs |
| folding placement | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1947-2056` | 按 `edgeA, axisA, axisB, edgeB` 取直线，找共享顶点，按共享端点修正 direction sign |
| fold angle | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:2309-2346` | `calculateFoldAngle(axisA, axisB, edgeA, edgeB)` 拒绝 parallel axes、axis/edge parallel 和 invalid cosine |
| final axes | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:2047-2055` | `SketchBasePoint=p`，`SketchXAxis=axisA direction`，`SketchNormal=rotated norm reversed` |

## cad-core 落点

| 层 | 目标落点 | 职责 |
| --- | --- | --- |
| Datum AttachEngine | `cad-core/src/part_design/datum_attachment.h` | `Folding` support resolver、shared vertex/sign logic、fold-angle helper、diagnostics、DatumPlane/CoordinateSystem dispatch |
| DatumPlane output | `cad-core/src/part_design/datum_plane.cpp` | 输出 `origin`、`x_axis`、`normal` 供 FreeCAD expected 比对 |
| expected collector | `cad-core/tools/collect_freecad_expected.py` | 为 `PartDesign::Plane` 采集 `origin`、`x_axis`、`normal` |
| fixtures / expected | `cad-core/fixtures/c51m5` | `partdesign-datum-folding-modes.json` 与 diagnostics fixture |
| tests | `cad-core/tests/test_p7_features.py`、`cad-core/tests/test_adapters.py` | success parity、invalid diagnostics、capability exact blocker 移除 |
| capability | `cad-core/src/runtime/capability_contract.cpp` | supported / fixtures / diagnostics / exact blocker 同步；adapter 不承载 AttachEngine 业务 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 方案 | `6-23-18-16-【已实现】C5-M18-DatumFoldingAttachEngine方案.md` | Folding 主线范围、实现边界、验收分层 |
| 工作步骤总入口 | `工作步骤细分/6-23-18-17-【已实现】C5-M18工作步骤总入口.md` | S0-S6 队列索引 |
| S0 | `工作步骤细分/6-23-18-18-【已实现】C5-M18-S0-liveFoldingBlocker冻结.md` | live blocker 与发布边界 |
| S1 | `工作步骤细分/6-23-18-19-【已实现】C5-M18-S1-FreeCADFolding源码候选矩阵.md` | FreeCAD source audit |
| S2 | `工作步骤细分/6-23-18-20-【已实现】C5-M18-S2-scope准入与待实现矩阵.md` | scope / backendGap / nonGoal routing |
| S3 | `工作步骤细分/6-23-18-21-【已实现】C5-M18-S3-Folding合同复审.md` | four-line DTO、diagnostics、placement contract |
| S4 | `工作步骤细分/6-23-18-22-【已实现】C5-M18-S4-requestLocalBoundary复审.md` | request-local support/writeback 和 no-session 边界 |
| S5 | `工作步骤细分/6-23-18-23-【已实现】C5-M18-S5-OracleFixtures与Expected.md` | FreeCADCmd success / diagnostics expected |
| S6 | `工作步骤细分/6-23-18-24-【已实现】C5-M18-S6-实现与发布闸门.md` | C++、tests、capability、docs closeout |

## S6 closeout

- `cad-core` 已支持 `Folding` selected MapMode for DatumPlane / CoordinateSystem。
- success fixture 覆盖 normal support order 和 reversed support direction sign；expected 来自 FreeCADCmd。
- diagnostics fixture 覆盖 missing fourth support、non-straight support、disconnected supports、parallel axes、axisA/edgeA parallel、invalid folding cosine。
- capability exact blocker 已移除 `Folding`，只保留 `TangentU`、`TangentV`、`IntersectionPoint`。
- response 仍只返回 placement、diagnostics、`documentObjectUpdates` / `elementReferenceUpdates` suggestions；这些 suggestions 不是 backend 持久状态。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
FREECADCMD=/home/user/.local/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c51m5/partdesign-datum-folding-modes.json --check
FREECADCMD=/home/user/.local/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c51m5/partdesign-datum-folding-diagnostics.json --check
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51x_datum_folding_modes_match_expected tests.test_p7_features.CadCoreP7FeatureTest.test_c51x_datum_folding_invalid_diagnostics
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
cd /home/user/Chili3DProject/FreeCAD
git diff --check
```

## 非目标

- 不实现 `TangentU` / `TangentV`。
- 不实现 `IntersectionPoint`。
- 不新增 GUI Attachment editor、TaskPanel、ViewProvider、visual resize 或 cross-request backend attachment session。
- 不传递或保存 complete BREP；`ReferenceShadow.brep` 只保留单 referenced subshape snapshot 例外。
