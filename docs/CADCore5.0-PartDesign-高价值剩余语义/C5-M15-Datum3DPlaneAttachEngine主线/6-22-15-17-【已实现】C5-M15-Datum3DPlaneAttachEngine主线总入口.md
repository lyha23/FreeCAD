# 【已实现】C5-M15 Datum3DPlane AttachEngine 主线

状态：`done_c5m15_expected_backed_capability_closed`

本包承接 C5-M14 DatumPoint `ProximityPoint1/2` 已收口后的下一批 AttachEngine exact blocker。它不再挑单个 thin case，而是按同一 FreeCAD 调用链、同一 cad-core DTO/API 边界、同一 c51m5 expected 家族，批量处理并已发布 `Translate`、`TangentPlane`、`ThreePointsPlane`、`ThreePointsNormal`。

## 目标

- 把 `datum_attach_engine_remaining_modes` 中的 3D plane / local placement family 从大 blocker 拆成已验证批次。
- 按 FreeCAD `AttachEngine3D::_calculateAttachedPlacement()` 中四个 mode 的源码语义实现：`Translate` 的 vertex-local placement，`TangentPlane` 的 face+vertex surface projection/normal/tangent，`ThreePointsPlane` 和 `ThreePointsNormal` 的三点成面和法向推导。
- 完成 FreeCADCmd oracle、`cad-core/src/part_design/datum_attachment.h`、`c51m5` fixtures/expected、focused tests、capability/docs 和验收记录闭环。
- 保持 CAD Core 无状态边界：placement、subname 更新建议和 diagnostics 都来自单次 request graph；不引入跨请求 attachment session、shape cache 或完整 BREP 状态。
- 发布时已只从 exact blocker 移除本包四个 mode；`Folding`、Frenet/curve frame、curvature/conic landmarks、`IntersectionPoint` 保持后续分包。

## 当前基线

- C5-M14 已在 cad-core capability 中移除 `ProximityPoint1/2`，并已有 `c51m5/partdesign-datum-point-proximity-*` expected-backed fixtures。
- C5-M15 本批四个 mode 已进入 selected placement 主路径，并从 `datum_attach_engine_remaining_modes` exact blocker 移除。
- `cad-core/fixtures/c51m5/expected` 已包含 FreeCADCmd-backed `partdesign-datum-3d-plane-modes` expected；diagnostics fixture 保持 source-backed diagnostic 口径。
- `cad-core/fixtures/c51m5` 继续作为 Datum AttachEngine expected 家族，本包未新建跨主题 fixture 家族。

## 证明链条

```text
S0 live blocker freeze (已冻结)
  -> S1 FreeCAD AttachEngine3D source candidates (已实现)
  -> S2 scope / blocker / backendGap / nonGoal routing (已实现)
  -> S3 Translate + ThreePoints placement math review (已实现)
  -> S4 TangentPlane surface normal/tangent review (已实现)
  -> S5 request-local placement + capability contract review (已实现)
  -> S6 oracle collection, cad-core implementation, focused tests, docs/capability closeout (已实现)
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| selected 3D mode 调度 | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1330` `AttachEngine3D::_calculateAttachedPlacement()` | `readLinks()` 后按 `mapMode` 进入 `Translate` / `TangentPlane` / `ThreePoints*` 分支 |
| `Translate` | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1363` | 要求一个 vertex；position = vertex + `attachmentOffset.getPosition()`；rotation 保留 `origPlacement.getRotation()`；立即返回 |
| `TangentPlane` | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1590` | face+vertex；vertex-first 时 swap 并让 base point through vertex；投影到 surface 后用 `Tools::getNormal()` 和 tangent U/V 定向 |
| `ThreePointsPlane` | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1857` | 从 vertex 或 edge endpoints 收集前三点；normal = `vec01 x vec02`；base point = 三点 centroid |
| `ThreePointsNormal` | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1857` | normal = `vec02` 对 `vec01` 的垂直分量并 reverse；base point = p2 投影到 p0+normal plane |
| 后续 placement 组合 | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp` same function tail | 非 `Translate` 分支继续由 `placementFactory()` 和 AttachmentOffset/MapReversed 组合 |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| PartDesign Datum AttachEngine | `cad-core/src/part_design/datum_attachment.h` | 已实现四个 3D selected MapMode 的 request-local placement helper、support 解析、diagnostics |
| Datum executor | `cad-core/src/part_design/datum_plane.cpp`、`cad-core/src/part_design/datum_coordinate_system.cpp`、`cad-core/src/part_design/datum_point.cpp` | 保持 shape 输出和 Placement 消费，不承载 mode 业务特判 |
| geometry / OCCT helper | `cad-core/src/part_design/datum_attachment.h` 内部或后续可提取 helper | `BRep_Tool::Pnt`、`BRepAdaptor_Curve`、`GeomAPI_ProjectPointOnSurf`、surface normal/tangent 计算 |
| fixture / expected | `cad-core/fixtures/c51m5` | 已新增 `partdesign-datum-3d-plane-modes` 和 diagnostics fixture |
| tests | `cad-core/tests/test_p7_features.py`、`cad-core/tests/test_expected_fixtures.py`、`cad-core/tests/test_adapters.py` | expected parity、invalid diagnostics、capability exact blocker 移除 |
| capability | `cad-core/src/adapters/c_api/c_api.cpp` | supported/fixtures/diagnostics/exact blocker 同步；adapter 不承载 AttachEngine 业务逻辑 |
| docs | `docs/CADCore5.0-PartDesign-高价值剩余语义`、`docs/CADCore5.1-PartDesign-剩余deferred语义实现` | C5 / C51X 状态一致 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 方案 | `6-22-15-17-【已实现】C5-M15-Datum3DPlaneAttachEngine方案.md` | 主线范围、实现顺序、验收分层 |
| 工作步骤总入口 | `工作步骤细分/6-22-15-18-【已实现】C5-M15工作步骤总入口.md` | S0-S6 队列索引 |
| S0 | `工作步骤细分/6-22-15-19-【已实现】C5-M15-S0-liveAttachEnginePlaneBlocker冻结.md` | 已冻结 live blocker、禁止声明和状态字典 |
| S1 | `工作步骤细分/6-22-15-20-【已实现】C5-M15-S1-FreeCAD3DPlane源码候选矩阵.md` | FreeCAD source audit 与 source candidates |
| S2 | `工作步骤细分/6-22-15-21-【已实现】C5-M15-S2-scope准入与待实现矩阵.md` | scope / blocker / nonGoal / backendGap routing |
| S3 | `工作步骤细分/6-22-15-22-【已实现】C5-M15-S3-TranslateAndThreePoints专项复审.md` | vertex-local 与三点几何专项复审 |
| S4 | `工作步骤细分/6-22-15-23-【已实现】C5-M15-S4-TangentPlaneSurfaceNormal专项复审.md` | face+vertex surface normal/tangent 专项复审 |
| S5 | `工作步骤细分/6-22-15-24-【已实现】C5-M15-S5-requestLocalPlacementAndCapability专项复审.md` | request-local placement、writeback、capability 边界 |
| S6 | `工作步骤细分/6-22-15-25-【已实现】C5-M15-S6-Oracle实现与发布闸门.md` | oracle、代码落点、focused tests、发布闸门 |
| source candidates | `矩阵/c5m15_datum_3d_plane_source_candidates.tsv` | FreeCAD 源码候选 |
| scope review | `矩阵/c5m15_datum_3d_plane_scope_review_matrix.tsv` | 范围准入与状态 |
| blocker queue | `矩阵/c5m15_datum_3d_plane_blocker_queue.tsv` | 待实现 blocker 队列 |
| backend gap classification | `矩阵/c5m15_datum_3d_plane_backend_gap_classification.tsv` | backendGap / releaseGate / nonGoal 分类 |
| fixture oracle | `矩阵/c5m15_datum_3d_plane_fixture_oracle_matrix.tsv` | oracle / fixture 计划 |
| non-goal registry | `矩阵/c5m15_datum_3d_plane_non_goal_registry.tsv` | 非目标与 reopen 条件 |
| validation | `矩阵/c5m15_datum_3d_plane_validation_matrix.tsv` | 短跑、focused、收口验证 |

当前 S0 已完成 docs-only live guard，S1 已补齐 FreeCAD source evidence，S2 已把 source candidates 路由到 scope、blocker、backendGap、nonGoal 和 oracle fixture 行，S3 已完成 `Translate` 与 `ThreePoints*` placement 数学、support 解析、diagnostics 和 fixture/test route 复审，S4 已完成 `TangentPlane` projection / `Tools::getNormal` / tangent U/V / diagnostics 复审，S5 已完成 request-local placement / writeback / capability 发布边界复审，S6 已完成 FreeCADCmd expected、cad-core helper、focused tests、adapter capability 和 docs/matrix 发布闭环。

## 最小完整语义批次

| 批次 | 代表场景 | 产物 |
| --- | --- | --- |
| live blocker guard | S6 前 exact blocker 列出 `Translate`、`TangentPlane`、`ThreePointsPlane`、`ThreePointsNormal` | S0 已冻结 blocker 和 excluded family，S6 已移除本包四个 proven modes |
| source audit | `AttachEngine3D::_calculateAttachedPlacement()` 四个分支 | S1 source candidates 与 FreeCAD 短句证据 |
| DTO/API contract | `AttachmentSupport` 解析、MapMode、AttachmentOffset、MapReversed、Placement response | S2 scope / backendGap / fixture route |
| oracle batch | vertex translate、three vertices、edge endpoints + vertex、face+vertex tangent plane、invalid diagnostics | S6 FreeCADCmd expected 与 focused tests 已通过 |
| implementation batch | `datum_attachment.h` selected 3D placement helpers | C++ code 已落地，不放 adapter 业务逻辑 |
| release closeout | fixtures、tests、capability exact blocker、C5/C51 docs | 已只移除本包四个 mode |

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M15-Datum3DPlaneAttachEngine主线/工作步骤细分 --format markdown
```

## 非目标

- 不实现 `Folding`；它需要四条线、共享顶点、fold angle 状态机，单独建包。
- 不实现 `FrenetNB/TN/TB`、`Concentric`、`SectionOfRevolution`、`AxisOfCurvature`、`CenterOfCurvature`、`Normal`、`Binormal`、`TangentU/V`；这些依赖曲线 D1/D2、Frenet frame 和曲率半径。
- 不实现 `Focus1/2`、`Directrix1/2`、`Asymptote1/2`；这些属于 conic landmark family。
- 不实现 `IntersectionPoint`；需先查清 FreeCAD direct branch / enum route 后单独分包。
- 不新增 GUI Attachment editor、TaskPanel、ViewProvider、可视化 resize 或跨请求 backend attachment session。
