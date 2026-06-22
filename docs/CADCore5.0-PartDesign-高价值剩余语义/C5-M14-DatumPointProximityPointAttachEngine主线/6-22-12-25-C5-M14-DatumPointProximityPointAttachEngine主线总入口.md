# C5-M14 DatumPoint ProximityPoint AttachEngine 主线

状态：`pending_C5-M14_S1_source_audit_done`

本包承接 C5.1 Datum AttachEngine exact blocker 的下一批可实现子线，但按 C5.0 高价值剩余语义的主线形态落档。当前已支持 DatumPoint `Vertex` / `OnEdge` / `CenterOfMass` 和 DatumLine `TwoPointLine` / `IntersectionLine` / `ProximityLine`；本包只处理 DatumPoint `ProximityPoint1` / `ProximityPoint2`，不重开其它 AttachEngine mode。

## 目标

- 把 `datum_attach_engine_remaining_modes` 中的 `ProximityPoint1` / `ProximityPoint2` 从大 exact blocker 拆成可执行实现项。
- 先冻结 FreeCAD 源码语义：两输入 shape，edge-face 时先用 `BRepIntCurveSurface_Inter` 找交点，找不到再走 `BRepExtrema_DistShapeShape`，按 mode 返回 `PointOnShape1(1)` 或 `PointOnShape2(1)`。
- 建立 native expected fixture 与 focused tests，覆盖 vertex/edge/face 代表场景、edge-face intersection 特殊路径、distance fallback 和关键 invalid diagnostics。
- 保持 CAD Core 无状态边界：`AttachmentSupport`、`documentObjectUpdates`、subname 恢复都只来自单次 request graph，不引入跨请求 attachment session。
- 发布时只从 capability blocker 列表移除 `ProximityPoint1` / `ProximityPoint2`，不得顺带声明 `IntersectionPoint`、Focus、Frenet、三点/折叠或曲率相关 mode。

## 当前基线

- capability：`part_design.datum_attachment.status=supported_c51x_selected_attach_engine_with_datum_line_family`。
- 已 supported：`FlatFace`、`ObjectXY/ObjectXZ/ObjectYZ`、`ObjectOrigin`、`ObjectX/ObjectY/ObjectZ`、`NormalToEdge`、`Vertex`、`OnEdge`、`CenterOfMass`、DatumLine line-family。
- 待收口 exact blocker：`ProximityPoint1` / `ProximityPoint2` 仍在 `datum_attach_engine_remaining_modes`。
- cad-core 当前落点：`cad-core/src/part_design/datum_attachment.h` 对 selected Datum mode 已有单 support 与 line-family 多 support 路径，但 DatumPoint proximity 尚未消费双 support。
- 现有 fixture 口径：继续使用 `cad-core/fixtures/c51m5` 作为 Datum AttachEngine 后续 expected-backed fixture 家族。

## 证明链条

```text
S0 live blocker freeze (已冻结：capability exact blocker 仍列出 ProximityPoint1/2)
  -> S1 FreeCAD proximity source audit (已完成：source authority 已回写，未实现代码)
  -> S2 scope / backendGap / nonGoal routing
  -> S3 edge-face intersection special path
  -> S4 distance fallback and diagnostics
  -> S5 request-local writeback and capability contract
  -> S6 oracle, code landing and release gate
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| DatumPoint proximity mode 注册 | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp::AttachEnginePoint::AttachEnginePoint()` | `mm0ProximityPoint1/2` 接收 `rtAnything, rtAnything` |
| DatumPoint proximity placement | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp::AttachEnginePoint::_calculateAttachedPlacement()` | 要求两个 shape，调用 `getProximityPoint()`，再走 `placementFactory(... BasePoint ...)` |
| edge-face 交点优先 | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp::AttachEnginePoint::getProximityPoint()` | edge-face 时先构造 transformed `GeomAdaptor_Curve` 并跑 `BRepIntCurveSurface_Inter` |
| distance fallback | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp::AttachEnginePoint::getProximityPoint()` | `BRepExtrema_DistShapeShape` 返回 p1/p2，按 mode 选择 |
| request-local writeback | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/AttachExtension.cpp::AttachExtension::positionBySupport()` | `calculateAttachedPlacement(..., &subChanged)` 后写回 sub values |
| subname 数据模型 | `~/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp` | `PropertyLinkSub/List::getSubValues()` 提供 request 输入证据 |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| PartDesign Datum AttachEngine | `cad-core/src/part_design/datum_attachment.h` | 新增 DatumPoint proximity helpers、双 support 解析、edge-face intersection、distance fallback、diagnostics |
| DatumPoint executor | `cad-core/src/part_design/datum_point.cpp` | 保持 Placement 到点 shape 的输出路径，不承载 mode 业务逻辑 |
| fixture / expected | `cad-core/fixtures/c51m5` | 新增 `partdesign-datum-point-proximity-modes` 和 diagnostics fixture |
| tests | `cad-core/tests/test_p7_features.py`、`cad-core/tests/test_expected_fixtures.py`、`cad-core/tests/test_adapters.py` | focused expected parity、invalid diagnostics、capability blocker 移除 |
| capability | `cad-core/src/adapters/c_api/c_api.cpp` | supported/fixtures/diagnostics/exact_blockers 同步，adapter 不承载几何逻辑 |
| 文档 | `docs/CADCore5.0-PartDesign-高价值剩余语义`、`docs/CADCore5.1-PartDesign-剩余deferred语义实现` | C5-M14 和 C51X 状态一致 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 方案 | `6-22-12-25-C5-M14-DatumPointProximityPointAttachEngine方案.md` | 主线范围、实现顺序、验收分层 |
| 工作步骤总入口 | `工作步骤细分/6-22-12-26-【已实现】C5-M14工作步骤总入口.md` | S0-S6 队列索引 |
| S0 | `工作步骤细分/6-22-12-27-【已实现】C5-M14-S0-liveAttachEngineBlocker冻结.md` | 已冻结 live blocker、禁止声明和状态字典 |
| S1 | `工作步骤细分/6-22-12-28-【已实现】C5-M14-S1-FreeCADProximityPoint源码候选矩阵.md` | FreeCAD source audit 与 source candidates |
| S2 | `工作步骤细分/6-22-12-29-C5-M14-S2-scope准入与待实现矩阵.md` | scope / blocker / nonGoal / backendGap routing |
| S3 | `工作步骤细分/6-22-12-30-C5-M14-S3-edgeFaceIntersection专项复审.md` | edge-face intersection 优先路径 |
| S4 | `工作步骤细分/6-22-12-31-C5-M14-S4-distanceFallbackAndDiagnostics专项复审.md` | distance fallback 与 diagnostics |
| S5 | `工作步骤细分/6-22-12-32-C5-M14-S5-requestLocalWriteback专项复审.md` | request-local support/writeback/capability 边界 |
| S6 | `工作步骤细分/6-22-12-33-C5-M14-S6-Oracle实现与发布闸门.md` | 代码落点、oracle、focused tests、发布闸门 |
| source candidates | `矩阵/c5m14_datum_point_proximity_source_candidates.tsv` | FreeCAD 源码候选 |
| scope review | `矩阵/c5m14_datum_point_proximity_scope_review_matrix.tsv` | 范围准入与状态 |
| blocker queue | `矩阵/c5m14_datum_point_proximity_blocker_queue.tsv` | 待实现 blocker 队列 |
| backend gap classification | `矩阵/c5m14_datum_point_proximity_backend_gap_classification.tsv` | backendGap / unsupported / nonGoal 分类 |
| fixture oracle | `矩阵/c5m14_datum_point_proximity_fixture_oracle_matrix.tsv` | oracle / fixture 计划 |
| non-goal registry | `矩阵/c5m14_datum_point_proximity_non_goal_registry.tsv` | 非目标与 reopen 条件 |
| validation | `矩阵/c5m14_datum_point_proximity_validation_matrix.tsv` | 短跑、focused、收口验证 |

## 最小完整语义批次

| 批次 | 代表场景 | 产物 |
| --- | --- | --- |
| live blocker guard | current capability blocker 列表含 `ProximityPoint1/2` | S0 冻结 exact blocker 和 non-goal |
| source audit | `AttachEnginePoint` proximity constructor / placement / helper | S1 source candidates 与 FreeCAD 短句证据 |
| input contract | 两个 `AttachmentSupport` shape / subshape | S2 scope / backendGap / fixture target |
| edge-face intersection | edge 与 face 相交时优先返回第一个交点 | S3 oracle representative 与 cad-core helper 目标 |
| distance fallback | vertex-vertex、edge-edge、edge-face no-hit 最近点 | S4 expected-backed success + diagnostics |
| request-local publication | `documentObjectUpdates` 不跨请求保存 | S5/S6 capability、tests、docs 收口 |

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M14-DatumPointProximityPointAttachEngine主线/工作步骤细分 --format markdown
```

## 非目标

- 不实现 `IntersectionPoint`。当前 source audit 未确认同名直接分支，必须单独查证后再建包。
- 不实现 `Focus1/2`、`CenterOfCurvature`、`FrenetNB/TN/TB`、`Concentric`、`SectionOfRevolution`、三点/折叠、Directrix/Asymptote、curve tangent/normal/binormal。
- 不修改 C5-M13 Part Workbench surface narrowed blocker recovery。
- 不新增 GUI Attachment editor、TaskPanel、ViewProvider 或 visual resize。
- 不引入跨请求 attachment session、长期 shape/BREP 缓存或 cad-core-output-derived expected。
