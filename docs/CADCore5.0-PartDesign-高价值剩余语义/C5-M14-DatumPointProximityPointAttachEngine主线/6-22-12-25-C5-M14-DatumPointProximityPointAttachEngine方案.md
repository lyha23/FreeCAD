# C5-M14 DatumPoint ProximityPoint AttachEngine 方案

状态：`done_C5-M14_S6_release_gate`

## 当前基线

`part_design.datum_attachment` 已进入 `supported_c51x_selected_attach_engine_with_datum_line_family`。当前 supported 范围已经覆盖 DatumPoint 单输入 `Vertex`、`OnEdge`、`CenterOfMass`，以及 DatumLine `TwoPointLine`、`IntersectionLine`、`ProximityLine`。剩余 `datum_attach_engine_remaining_modes` 中，`ProximityPoint1` / `ProximityPoint2` 是下一批最小可闭环项，因为它与已实现的 DatumLine `ProximityLine` 共享双 support、OCCT distance helper 和 request-local placement 边界，但 FreeCAD 还多了 edge-face intersection 优先路径。

本包不继续抽象复述 AttachEngine 全量状态，而是把 `ProximityPoint1/2` 拆成可执行实现任务：source audit、oracle fixture、cad-core helper、focused tests、capability/docs 发布。

## 范围

| 项 | 本包处理 | 说明 |
| --- | --- | --- |
| `ProximityPoint1` | 是 | 返回第一个 support shape 的 proximity point |
| `ProximityPoint2` | 是 | 返回第二个 support shape 的 proximity point |
| edge-face intersection | 是 | FreeCAD 先查 edge 与 face 交点，存在时直接返回第一个交点 |
| `BRepExtrema_DistShapeShape` fallback | 是 | edge-face 无交点和普通 shape-shape 都走 distance fallback |
| missing / unresolved support diagnostics | 是 | 需要 focused invalid tests |
| `AttachmentOffset` composition | 是 | 沿用现有 DatumPoint selected MapMode 规则 |
| `documentObjectUpdates` | 是 | 只做 request-local evidence，不保存 session |
| `IntersectionPoint` | 否 | 需要先确认 FreeCAD direct branch / enum route |
| Focus / curvature / Frenet / three-point / folding | 否 | 后续独立 family |

## FreeCAD 调用链

1. `AttachEnginePoint::AttachEnginePoint()`：`mm0ProximityPoint1` 与 `mm0ProximityPoint2` 注册 `cat(rtAnything, rtAnything)`。
2. `AttachEnginePoint::_calculateAttachedPlacement()`：读 `AttachmentSupport` 后要求两个 shape，调用 `getProximityPoint(mmode, shape1, shape2)`。
3. `AttachEnginePoint::getProximityPoint()`：
   - 若输入是 edge-face 或 face-edge，先构造 transformed curve。
   - 用 `BRepIntCurveSurface_Inter` 遍历交点；多个交点只 warning，返回第一个。
   - 若没有交点，继续 `BRepExtrema_DistShapeShape`。
   - `mm0ProximityPoint1` 返回 `PointOnShape1(1)`，`mm0ProximityPoint2` 返回 `PointOnShape2(1)`。
4. `_calculateAttachedPlacement()` 用 `placementFactory(gp_Vec(0,0,1), gp_Vec(1,0,0), BasePoint, gp_Pnt())` 生成 DatumPoint placement，再乘 `AttachmentOffset`。
5. `AttachExtension::positionBySupport()` 负责 request-local support subname writeback 语义；cad-core 只能返回更新建议，不保存跨请求状态。

## cad-core 实现边界

- `cad-core/src/part_design/datum_attachment.h`：
  - 增加 `usesPointProximitySupports` 或等价 mode family 判定，确保 `ProximityPoint1/2` 消费两个 support links。
  - 增加 edge-face intersection helper，必要 includes 包括 `BRepIntCurveSurface_Inter.hxx` 与 transformed curve 相关 OCCT API。
  - 增加 distance fallback helper，复用 `BRepExtrema_DistShapeShape`。
  - 在 `selectedDatumPlacement()` 中只对 `DatumAttachmentEngine::Point` 开启两个 mode。
  - invalid diagnostics 保持结构化 code，不抛裸异常。
- `cad-core/src/part_design/datum_point.cpp`：不新增业务逻辑，只继续根据 placement 输出 point shape。
- `cad-core/tools/collect_freecad_expected.py`：只在现有 collector 无法采 DatumPoint `Placement` / point 字段时做最小兼容；不要让 collector 从 cad-core result 生成 expected。
- `cad-core/src/adapters/c_api/c_api.cpp`：实现完成后更新 supported、fixtures、diagnostics、exact blocker list。

## 实施顺序

1. S0：冻结 capability live baseline 和禁止声明。
2. S1：摘录 FreeCAD source authority，填 source candidate 矩阵。
3. S2：把 ProximityPoint scope、backendGap、nonGoal、fixture/oracle 行路由清楚。
4. S3：实现并验证 edge-face intersection 优先路径。
5. S4：实现并验证 distance fallback、ProximityPoint1/2 输出差异和 invalid diagnostics。
6. S5：复核 request-local writeback、AttachmentOffset、MapReversed 非目标边界和 capability 发布口径。
7. S6：已落 C++、fixtures、expected、focused tests、docs/capability，并移除 exact blocker 中 `ProximityPoint1/2`。

## 验收分层

本轮方案短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M14-DatumPointProximityPointAttachEngine主线 docs/CADCore5.0-PartDesign-高价值剩余语义/README.md
for f in docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M14-DatumPointProximityPointAttachEngine主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
```

实现短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
FREECADCMD=/home/user/.local/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c51m5/partdesign-datum-point-proximity-modes.json --check
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51x_datum_point_proximity_modes_match_expected tests.test_p7_features.CadCoreP7FeatureTest.test_c51x_datum_point_proximity_invalid_diagnostics
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

阶段收口：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 下一轮代码落点

| blocker | C++ 落点 | FreeCAD 依据 | tests | 成功标准 |
| --- | --- | --- | --- | --- |
| `C5M14-BLK-201` | `cad-core/src/part_design/datum_attachment.h` | `AttachEnginePoint::getProximityPoint()` edge-face branch | `test_c51x_datum_point_proximity_modes_match_expected` | 已关闭：edge-face intersection expected 与 FreeCADCmd 一致 |
| `C5M14-BLK-202` | `cad-core/src/part_design/datum_attachment.h` | `BRepExtrema_DistShapeShape` fallback | 同上 | 已关闭：`ProximityPoint1` 与 `ProximityPoint2` 返回不同 support 的 point |
| `C5M14-BLK-203` | `datum_attachment.h`、fixtures diagnostics | `_calculateAttachedPlacement()` requires two shapes | `test_c51x_datum_point_proximity_invalid_diagnostics` | 已关闭：少 support / unresolved support 有稳定 diagnostic code，distance helper failure 映射为 `execution_failed` |
| `C5M14-BLK-501` | `c_api.cpp`、docs、tests | capability exact blocker source | `test_c_api_capabilities_exposes_web_contract_facts` | 已关闭：exact blocker 移除 `ProximityPoint1/2`，其它 mode 保持 |

禁止捷径：

- 不按 fixture 名称分支。
- 不用 bbox、长度、面积或 cad-core output 反推 expected。
- 不在 adapter 层实现 AttachEngine 业务逻辑。
- 不把 `ProximityLine` 的输出后处理改造成 DatumPoint 语义。
- 不把 touch/intersect 当成 invalid。FreeCAD proximity point 对相交 shape 可以返回交点；这不同于 `ProximityLine`。
