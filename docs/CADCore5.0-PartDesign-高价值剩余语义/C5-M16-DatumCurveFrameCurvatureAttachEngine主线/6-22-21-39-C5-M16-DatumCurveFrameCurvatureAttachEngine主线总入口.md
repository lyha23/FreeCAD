# C5-M16 DatumCurveFrameCurvature AttachEngine 主线

状态：`S2_done__S3_pending__release_blocked_by_c5m15_s6`

本包承接 C5-M15 Datum3DPlane AttachEngine 收口后的下一批 `datum_attach_engine_remaining_modes`。范围按同一 FreeCAD 曲线帧调用链、同一 `cad-core/src/part_design/datum_attachment.h` placement helper 边界、同一 `c51m5` Datum expected 家族拆分，不再按单个 UI mode 名称薄切。

## 目标

- 冻结 `AttachEngine3D::_calculateAttachedPlacement()` 中 curve frame / curvature family 的源码语义：edge/curve support、optional vertex projection、`attachParameter`、D1/D2、Frenet T/N/B、curvature center、failure diagnostics。
- 批量规划 `FrenetNB`、`FrenetTN`、`FrenetTB`、`Concentric`、`SectionOfRevolution`，以及复用这些 3D branch 的 `AxisOfCurvature`、`CenterOfCurvature`、line `Normal/Binormal` aliases。
- 在方案阶段写清 native oracle、request-local DTO/API、fixtures、focused tests、capability/docs 和根矩阵闭环。
- 保持 CAD Core 无状态边界：所有 placement、diagnostics、writeback suggestions 都来自单次 request graph，不保存跨请求曲线帧、shape cache 或完整 BREP。
- 发布时只移除本包 proven 的 curve frame / curvature modes；`Folding`、conic landmarks、`IntersectionPoint`、`TangentU/V` 和 GUI/session 继续后续分包或 non-goal。

## 前置条件

- C5-M15 S6 当前 live 队列仍 pending；`Translate`、`TangentPlane`、`ThreePointsPlane`、`ThreePointsNormal` 不再出现在 exact blocker 后，C5-M16 才能进入 S6 release gate。
- 本包可以先完成 S0-S5 docs/source/scope 复审，但 S6 不得和 C5-M15 S6 并行改同一个 capability exact blocker。

## 当前基线

- C5-M14 已支持 DatumPoint `ProximityPoint1/2`。
- C51X 已支持 DatumLine `TwoPointLine` / `IntersectionLine` / `ProximityLine` 和 DatumPoint `Vertex` / `OnEdge` / `CenterOfMass`。
- M15 负责关闭 3D plane family，不负责 curve frame / curvature。
- 当前 capability exact blocker 仍保留 `FrenetNB`、`FrenetTN`、`FrenetTB`、`Concentric`、`SectionOfRevolution`、`AxisOfCurvature`、`Normal`、`Binormal`、`CenterOfCurvature` 等 M16 scope modes，同时也保留 `Folding`、conic landmarks、`IntersectionPoint`、`TangentU/V` 等 excluded family。

## 证明链条

```text
S0 live blocker / M15 dependency freeze
  -> S1 FreeCAD curve-frame source candidates
  -> S2 scope / blocker / backendGap / nonGoal routing frozen
  -> S3 edge projection + Frenet T/N/B review
  -> S4 curvature center + alias placement review
  -> S5 request-local placement / capability contract review
  -> S6 oracle collection, cad-core implementation, focused tests, docs/capability closeout
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| ref type 注册 | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1242-1282` `AttachEngine3D::AttachEngine3D()` | curve family 接受 `rtEdge` / `rtCurve` / `rtCircle`，可带 optional vertex，vertex-first 时在计算分支中 swap |
| 曲线投影与参数 | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1674-1745` | 无 vertex 用 `attachParameter` 插值；有 vertex 用 `GeomAPI_ProjectPointOnCurve` 得到 `LowerDistanceParameter()` |
| D1 / D2 与 Frenet frame | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1746-1831` | D1 得点和切向；D2 得二阶导；`T=d.Normalized()`、`N=dd - T*(dd dot T)`、`B=T x N`；不同 mode 组合 SketchNormal / SketchXAxis |
| curvature center | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1832-1847` | `Concentric` / `SectionOfRevolution` 把 origin 移到 osculating circle center；无 N 或无限曲率半径时报错 |
| common placement tail | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:2294-2306` | 走 `placementFactory()` 后乘 `attachmentOffset` |
| DatumLine aliases | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:2400-2483` | `AxisOfCurvature -> RevolutionSection` 且加 presuper rotation；`Binormal -> FrenetTN`；`Normal -> FrenetTB` |
| DatumPoint aliases | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:2833-2889` | `CenterOfCurvature -> RevolutionSection`；`OnEdge -> NormalToPath` 已由 C51X 单输入批次覆盖，不在本包发布项 |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| Datum AttachEngine | `cad-core/src/part_design/datum_attachment.h` | 新增 curve projection、Frenet frame、curvature center 和 alias placement helpers |
| Datum executors | `cad-core/src/part_design/datum_plane.cpp`、`datum_line.cpp`、`datum_point.cpp` | 消费 placement，不承载 mode 业务特判 |
| geometry helper | `datum_attachment.h` 内部或后续提取 helper | `BRepAdaptor_Curve`、`GeomAPI_ProjectPointOnCurve`、D1/D2、curve continuity diagnostics |
| fixtures / expected | `cad-core/fixtures/c51m5` | 新增 `partdesign-datum-curve-frame-modes` 和 diagnostics fixture |
| tests | `cad-core/tests/test_p7_features.py`、`test_expected_fixtures.py`、`test_adapters.py` | expected parity、invalid diagnostics、capability exact blocker 移除 |
| capability | `cad-core/src/adapters/c_api/c_api.cpp` | supported/fixtures/diagnostics/exact blocker 同步；adapter 不承载 AttachEngine 业务逻辑 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 方案 | `6-22-21-39-C5-M16-DatumCurveFrameCurvatureAttachEngine方案.md` | 主线范围、实现顺序、验收分层 |
| 工作步骤总入口 | `工作步骤细分/6-22-21-40-【已实现】C5-M16工作步骤总入口.md` | S0-S6 队列索引 |
| S0 | `工作步骤细分/6-22-21-41-【已实现】C5-M16-S0-liveCurveFrameBlocker冻结.md` | live blocker、M15 dependency、禁止声明 |
| S1 | `工作步骤细分/6-22-21-42-【已实现】C5-M16-S1-FreeCADCurveFrame源码候选矩阵.md` | FreeCAD source audit |
| S2 | `工作步骤细分/6-22-21-43-【已实现】C5-M16-S2-scope准入与待实现矩阵.md` | scope / backendGap / nonGoal routing |
| S3 | `工作步骤细分/6-22-21-44-C5-M16-S3-CurveProjectionFrenetFrame专项复审.md` | projection、D1/D2、Frenet T/N/B |
| S4 | `工作步骤细分/6-22-21-45-C5-M16-S4-CurvatureCenterAlias专项复审.md` | curvature center 与 line/point alias |
| S5 | `工作步骤细分/6-22-21-46-C5-M16-S5-requestLocalPlacementAndCapability专项复审.md` | writeback、capability、release boundary |
| S6 | `工作步骤细分/6-22-21-47-C5-M16-S6-Oracle实现与发布闸门.md` | oracle、C++、fixtures、tests、docs closeout |

## 最小完整语义批次

| 批次 | 代表场景 | 产物 |
| --- | --- | --- |
| live guard | M15 S6 closed 后的 `datum_attach_engine_remaining_modes` | S0 exact blocker freeze |
| source audit | `NormalToPath` / `FrenetNB/TN/TB` / `Concentric` / `SectionOfRevolution` / aliases | S1 source candidates |
| DTO/API contract | curve support、optional vertex、`MapPathParameter` / `AttachmentOffset` / `MapReversed` | S2 scope + backendGap |
| oracle batch | edge parameter, vertex projection, circle/arc Frenet, curvature center, line/point aliases, invalid diagnostics | S6 FreeCADCmd expected |
| implementation batch | `datum_attachment.h` curve-frame helpers | C++ landing |
| release closeout | fixtures、tests、capability exact blocker、C5/C51 docs | S6 release gate |

## S0 冻结结论

- S0 已完成 live blocker 冻结，不改 code、不采集 oracle、不移除 exact blocker。
- M16 scope 仅限 `FrenetNB/TN/TB`、`Concentric`、`SectionOfRevolution`、`AxisOfCurvature`、`Normal`、`Binormal`、`CenterOfCurvature`。
- `Folding`、conic landmarks、`IntersectionPoint`、`TangentU/V`、GUI/session 继续由 non-goal / later-package guard 保护。
- S1 已完成 FreeCAD source candidates 冻结。
- S2 已完成 scope 准入与待实现矩阵冻结：`NormalToPath`/projection 与 `FrenetNB/TN/TB` 进入 S3，curvature center 与 aliases 进入 S4，invalid diagnostics 进入 S6，excluded family 留在 non-goal / later package。C5-M15 S6 未关闭前，M16 S6 不得发布 capability。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M16-DatumCurveFrameCurvatureAttachEngine主线/工作步骤细分 --format markdown
```

## 非目标

- 不实现 `Folding`；它是四 line fold-angle 状态机。
- 不实现 `Focus1/2`、`Directrix1/2`、`Asymptote1/2`；它们属于 conic landmark family。
- 不实现 `IntersectionPoint`；需先确认 implementation route。
- 不把 `TangentU/V` 顺带发布；当前审计只证明 name-table 和 tangent-plane 局部短句，不证明 C5-M16 的 shared curve-frame route。
- 不新增 GUI Attachment editor、TaskPanel、ViewProvider、visual resize 或跨请求 backend attachment session。
