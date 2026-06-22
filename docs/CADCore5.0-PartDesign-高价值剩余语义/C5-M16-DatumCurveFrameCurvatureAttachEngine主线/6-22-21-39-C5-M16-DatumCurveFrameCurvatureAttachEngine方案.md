# C5-M16 DatumCurveFrameCurvature AttachEngine 方案

状态：`S1_done__S2_pending__release_blocked_by_c5m15_s6`

## 当前基线

`part_design.datum_attachment` 已覆盖 selected Datum 基础族、DatumLine line-family、DatumPoint single/proximity family，并由 C5-M15 负责 3D plane family。M16 不重新打开这些已分包内容，只处理同一 FreeCAD curve-frame 调用链下的剩余 exact blockers。

S0 live freeze 已确认 C5-M15 队列仍有 `C5-M15-S6 Oracle 实现与发布闸门` pending；因此 M16 S6 在 C5-M15 S6 关闭前不得发布 capability 或移除 `datum_attach_engine_remaining_modes` 中的任何 mode。当前 blocker 仍同时包含本包目标 modes 和 excluded family，S0 只冻结文档/TSV 边界，不采集 oracle、不改 C++/Python 测试。

## 范围

| 项 | 本包处理 | 说明 |
| --- | --- | --- |
| `FrenetNB` | 是 | 3D plane placement；normal = reversed tangent，X = reversed Frenet normal |
| `FrenetTN` | 是 | 3D plane placement；normal = binormal，X = tangent |
| `FrenetTB` | 是 | 3D plane placement；normal = reversed Frenet normal，X = tangent |
| `Concentric` | 是 | 复用 Frenet TN frame，并把 base point 移到曲率中心 |
| `SectionOfRevolution` | 是 | 复用 Frenet NB frame，并把 base point 移到曲率中心 |
| `AxisOfCurvature` | 是 | DatumLine alias：`AxisOfCurvature -> RevolutionSection`，另有 line 方向 presuper rotation |
| `Normal` / `Binormal` | 是 | DatumLine aliases：`Normal -> FrenetTB`、`Binormal -> FrenetTN` |
| `CenterOfCurvature` | 是 | DatumPoint alias：`CenterOfCurvature -> RevolutionSection` |
| invalid diagnostics | 是 | missing edge, wrong shape, projection failure, D1 zero derivative, D2 undefined, infinite curvature radius |
| `AttachmentOffset` / `MapReversed` / `MapPathParameter` | 是 | 复用 placementFactory tail 和 request-local response 边界 |
| `Folding` | 否 | 四线 fold-angle 状态机，后续独立包 |
| conic landmarks | 否 | `Focus/Directrix/Asymptote` 属 0D/1D conic property family |
| `IntersectionPoint` | 否 | implementation route 仍需单独审计 |
| `TangentU/V` | 否 | 不属于已证明的 shared curve-frame owner，本包只记录排除边界 |

## FreeCAD 调用链

1. `AttachEngine3D::AttachEngine3D()` 为 curve-frame modes 注册 edge/curve/circle support，并允许 optional vertex 与 vertex-first 顺序。
2. `_calculateAttachedPlacement()` 在 `mmNormalToPath`、`mmFrenetNB`、`mmFrenetTN`、`mmFrenetTB`、`mmRevolutionSection`、`mmConcentric` 共用分支读取 path edge。
3. 若给 vertex，则投影到 curve；否则用 `attachParameter` 在 first/last parameter 之间插值。
4. D1 得到 `p` 与 tangent `d`，零导数直接失败；Frenet / curvature modes 继续 D2 得 `dd`。
5. `T = d.Normalized()`，`N = dd - T * (dd dot T)`，`B = T x N`。N 不存在时 `FrenetTN/TB/Concentric/RevolutionSection` 必须报错或保持精确 blocker。
6. `Concentric` / `SectionOfRevolution` 计算曲率半径并把 base point 移到 osculating circle center。
7. DatumLine / DatumPoint aliases 先把 mode 映射回 3D branch，再用自身 executor shape 消费 placement。
8. 非 GUI 路径最后由 `placementFactory()`、`AttachmentOffset`、`MapReversed` 等 request-local 组合完成；cad-core 不保存 session。

## cad-core 实现边界

- `cad-core/src/part_design/datum_attachment.h`：
  - 增加 curve support parser，支持 edge/curve/circle、optional vertex、vertex-first swap。
  - 增加 curve parameter resolver：vertex projection 或 `MapPathParameter` / `attachParameter`。
  - 增加 Frenet frame helper，显式处理 D1/D2 failure、zero derivative、undefined normal。
  - 增加 curvature center helper，保留 infinite radius diagnostic。
  - 增加 alias mapping helper，确保 line/point aliases 不在 executor 里重复实现业务规则。
- `cad-core/fixtures/c51m5`：
  - 新增 success fixture：`partdesign-datum-curve-frame-modes.json`。
  - 新增 diagnostics fixture：`partdesign-datum-curve-frame-diagnostics.json`。
- `cad-core/tests/test_p7_features.py` / `test_expected_fixtures.py` / `test_adapters.py`：
  - expected parity 覆盖 edge parameter、vertex projection、circle/arc frame、curvature center、DatumLine alias、DatumPoint alias。
  - invalid diagnostics 覆盖 missing support、wrong support、projection failure、zero derivative、D2 / Frenet normal / curvature radius failure。
- `cad-core/src/adapters/c_api/c_api.cpp`：
  - 实现完成后补 fixtures/diagnostics/capability evidence，并只移除本包 proven modes。

## 实施顺序

1. S0：已冻结 M15 依赖、live exact blocker、禁止声明和状态字典。
2. S1：已审计 FreeCAD source candidates，确认哪些 mode 共享 3D curve-frame route，哪些必须排除。
3. S2：下一步把 source candidates 路由到 scope、blocker、backendGap、fixture/oracle、nonGoal。
4. S3：专项复审 projection / D1 / D2 / Frenet frame，写清 `NormalToPath` 与 `FrenetNB/TN/TB` 实现合同。
5. S4：专项复审 curvature center 与 aliases，写清 `Concentric`、`SectionOfRevolution`、`AxisOfCurvature`、`Normal/Binormal`、`CenterOfCurvature` 的成功/失败边界。
6. S5：复审 request-local response、writeback suggestions、capability exact blocker closeout，确保不把 excluded family 顺带 supported。
7. S6：仅在 C5-M15 S6 已关闭后，批量采集 FreeCADCmd expected，落 C++、fixtures、focused tests、capability/docs 和验收记录。

## 验收分层

本轮方案短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M16-DatumCurveFrameCurvatureAttachEngine主线 docs/CADCore5.0-PartDesign-高价值剩余语义/README.md docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
for f in docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M16-DatumCurveFrameCurvatureAttachEngine主线/矩阵/*.tsv docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M16-DatumCurveFrameCurvatureAttachEngine主线/工作步骤细分 --format markdown
```

实现短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
FREECADCMD=/Users/li/.local/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c51m5/partdesign-datum-curve-frame-modes.json --check
FREECADCMD=/Users/li/.local/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c51m5/partdesign-datum-curve-frame-diagnostics.json --check
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51x_datum_curve_frame_modes_match_expected tests.test_p7_features.CadCoreP7FeatureTest.test_c51x_datum_curve_frame_invalid_diagnostics
python3 -m unittest tests.test_expected_fixtures tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

阶段收口：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 下一轮代码落点

| blocker | C++ 落点 | FreeCAD 依据 | tests | 成功标准 |
| --- | --- | --- | --- | --- |
| `C5M16-BLK-101` | `cad-core/src/part_design/datum_attachment.h` | `Attacher.cpp:1674-1755` | `test_c51x_datum_curve_frame_modes_match_expected` | edge parameter / vertex projection / D1 zero derivative 行为一致 |
| `C5M16-BLK-201` | `datum_attachment.h` | `Attacher.cpp:1766-1831` | modes + diagnostics | FrenetNB/TN/TB frame、undefined normal diagnostics 与 expected 一致 |
| `C5M16-BLK-301` | `datum_attachment.h` | `Attacher.cpp:1832-1847` | modes + diagnostics | Concentric / SectionOfRevolution 曲率中心和 infinite radius diagnostic 一致 |
| `C5M16-BLK-401` | `datum_attachment.h`、`datum_line.cpp`、`datum_point.cpp` | `Attacher.cpp:2400-2483,2833-2889` | modes parity | AxisOfCurvature / Normal / Binormal / CenterOfCurvature aliases 走 3D helper，不在 executor 重写业务逻辑 |
| `C5M16-BLK-501` | `c_api.cpp`、C5/C51 docs、matrices | capability exact blocker source | adapter capability test | exact blocker 只移除本包 proven modes；excluded family 保留 |

禁止捷径：

- 不按 fixture 名称、bbox 或输出顺序推断曲线帧。
- 不把 straight-line undefined Frenet normal 伪造成 default frame。
- 不在 adapter 层实现 AttachEngine 业务逻辑。
- 不把 `Folding`、conic landmarks、`IntersectionPoint`、`TangentU/V` 顺带声明 supported。
