# C5-M17 DatumRemainingAttachEngine 主线

状态：`opened_after_c5m16_remaining_modes`

本包承接 C5-M16 之后 `part_design.datum_attachment.exact_blockers.datum_attach_engine_remaining_modes` 的剩余项。它不是把所有 mode 硬塞进同一轮实现，而是先冻结 live blocker 和 FreeCAD source route，再把同一调用链、同一 DTO/API 边界和同一 expected family 的内容纳入最小完整语义批次。

## 目标

- 冻结当前 remaining modes：`Folding`、`Directrix1/2`、`Asymptote1/2`、`TangentU/V`、`Focus1/2`、`IntersectionPoint`。
- 把 conic landmark family 作为第一批可执行实现候选：DatumLine `Directrix1/2`、`Asymptote1/2` 和 DatumPoint `Focus1/2`。
- 把 `Folding`、`IntersectionPoint`、`TangentU/V` 明确拆成后续独立 source / oracle route，不在 conic S6 中顺带发布。
- 保持 CAD Core 无状态边界：每次只消费 request graph 的 support shape/subname/MapMode/AttachmentOffset/MapReversed，不保存 backend attachment session 或完整 BREP。
- 发布时只从 exact blocker 删除 FreeCADCmd expected-backed proven modes；不能因为同属 remaining list 就批量删除未证明项。

## 当前基线

- C5-M14 已关闭 DatumPoint `ProximityPoint1/2`。
- C5-M15 已关闭 3D plane family：`Translate`、`TangentPlane`、`ThreePointsPlane`、`ThreePointsNormal`。
- C5-M16 已关闭 curve-frame / curvature family：`FrenetNB/TN/TB`、`Concentric`、`SectionOfRevolution`、`AxisOfCurvature`、`Normal`、`Binormal`、`CenterOfCurvature`。
- 当前 exact blocker 仍保留：`Folding`、`Directrix1`、`Directrix2`、`Asymptote1`、`Asymptote2`、`TangentU`、`TangentV`、`Focus1`、`Focus2`、`IntersectionPoint`。

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| remaining exact blocker | `cad-core/src/adapters/c_api/c_api.cpp` `datum_attach_engine_remaining_modes` | 发布真源：只删除 expected-backed proven modes |
| conic line ref types | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:2413-2419` | `Asymptote1/2` 只接受 hyperbola；`Directrix1` 接受 conic；`Directrix2` 接受 ellipse/hyperbola |
| conic line placement | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:2613-2702` | 从 `BRepAdaptor_Curve` 读取 hyperbola asymptote、ellipse/hyperbola/parabola directrix，输出 line base + direction |
| conic point ref types | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:2842-2845` | `Focus1` 接受 conic；`Focus2` 接受 ellipse/hyperbola |
| conic point placement | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:2937-2990` | 从 ellipse/hyperbola/parabola 读取 focus，parabola 无第二 focus |
| Folding excluded | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1947-2056` | 四条 line 的 fold-angle 状态机，不属于 conic landmark |
| TangentU/V excluded | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:1652-1661` | surface tangent-plane 局部 tangent 分支，不属于 conic landmark |
| IntersectionPoint excluded | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp:2432,2703+` | 有 line intersection route，需要单独确认 face/face DTO 和 expected |

## cad-core 落点

| 层 | 目标落点 | 职责 |
| --- | --- | --- |
| Datum AttachEngine | `cad-core/src/part_design/datum_attachment.h` | conic edge support 解析、mode dispatch、request-local diagnostics |
| DatumLine | `cad-core/src/part_design/datum_line.cpp` | 消费 Directrix/Asymptote placement，保持 line convention |
| DatumPoint | `cad-core/src/part_design/datum_point.cpp` | 消费 Focus placement，保持 point convention |
| fixtures / expected | `cad-core/fixtures/c51m5` | 新增 conic landmark modes 和 diagnostics fixture |
| tests | `cad-core/tests/test_p7_features.py`、`test_expected_fixtures.py`、`test_adapters.py` | expected parity、invalid diagnostics、capability exact blocker 移除 |
| capability | `cad-core/src/adapters/c_api/c_api.cpp` | supported / fixtures / diagnostics / exact blocker 同步；adapter 不承载 AttachEngine 业务 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 方案 | `6-23-03-26-C5-M17-DatumRemainingAttachEngine方案.md` | 主线范围、拆包策略、验收分层 |
| 工作步骤总入口 | `工作步骤细分/6-23-03-27-【已实现】C5-M17工作步骤总入口.md` | S0-S6 队列索引 |
| S0 | `工作步骤细分/6-23-03-28-C5-M17-S0-liveRemainingAttachEngineBlocker冻结.md` | live blocker 与禁止声明 |
| S1 | `工作步骤细分/6-23-03-29-【已实现】C5-M17-S1-FreeCADRemainingAttachEngine源码候选矩阵.md` | FreeCAD source audit |
| S2 | `工作步骤细分/6-23-03-30-C5-M17-S2-scope准入与待实现矩阵.md` | scope / backendGap / nonGoal routing |
| S3 | `工作步骤细分/6-23-03-31-C5-M17-S3-ConicLandmark合同复审.md` | conic landmark DTO / placement 合同 |
| S4 | `工作步骤细分/6-23-03-32-C5-M17-S4-ExcludedFamilies拆包复审.md` | Folding / IntersectionPoint / TangentU/V 拆包证据 |
| S5 | `工作步骤细分/6-23-03-33-C5-M17-S5-requestLocalPlacementAndCapability专项复审.md` | request-local response 与 capability release gate |
| S6 | `工作步骤细分/6-23-03-34-C5-M17-S6-Oracle实现与发布闸门.md` | conic oracle、C++、fixtures、tests、docs closeout |

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M17-DatumRemainingAttachEngine主线 --format markdown
```

## 非目标

- 不在 conic S6 中实现 `Folding`。
- 不在 conic S6 中实现 `IntersectionPoint`。
- 不在 conic S6 中发布 `TangentU/V`。
- 不新增 GUI Attachment editor、TaskPanel、ViewProvider、visual resize 或跨请求 backend attachment session。
