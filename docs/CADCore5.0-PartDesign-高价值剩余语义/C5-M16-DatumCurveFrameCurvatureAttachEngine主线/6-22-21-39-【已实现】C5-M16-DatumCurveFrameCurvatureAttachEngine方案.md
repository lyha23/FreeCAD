# 【已实现】C5-M16 DatumCurveFrameCurvature AttachEngine 方案

状态：`done_c5m16_expected_backed_capability_closed`

## 当前基线

`part_design.datum_attachment` 已覆盖 selected Datum 基础族、DatumLine line-family、DatumPoint single/proximity family，并由 C5-M15 关闭 3D plane family。M16 不重新打开这些已分包内容，只处理同一 FreeCAD curve-frame 调用链下的剩余 exact blockers。

C5-M15 S6 已关闭；C5-M16 S6 已完成 FreeCADCmd expected、cad-core helper、fixtures、focused tests、adapter capability 和 docs/root matrix closeout。当前 exact blocker 只删除本包 expected-backed proven modes；excluded family 仍保留。

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
| invalid diagnostics | 是 | missing edge, wrong shape, D1 zero derivative, undefined Frenet normal, infinite curvature radius；`projection_failed` 为 source/code-path-backed diagnostic |
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
6. `SectionOfRevolution` 先设置 normal = `T.Reversed()`、X = `N.Reversed()`；`Concentric` 先设置 normal = `B`、X = `T`；随后二者都用 `curvature = dd.Dot(N) / pow(d.Magnitude(), 2)` 把 base point 移到 `p + N * (1 / curvature)`。`N == 0` 必须报 infinite radius / undefined curvature diagnostic，不能用 bbox、circle center 或 world axes 猜。
7. DatumLine / DatumPoint aliases 先把 mode 映射回 3D branch，再用自身 executor shape 消费 placement；`AxisOfCurvature` 还要应用 Z-to-Y presuper rotation。
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
  - invalid diagnostics 覆盖 missing support、wrong support、zero derivative、undefined Frenet normal、curvature radius failure；`projection_failed` 保留 source/code-path-backed 边界。
- `cad-core/src/adapters/c_api/c_api.cpp`：
  - S6 已补 fixtures/diagnostics/capability evidence，并只移除本包 proven modes。

## 实施顺序

1. S0：已冻结 M15 依赖、live exact blocker、禁止声明和状态字典。
2. S1：已审计 FreeCAD source candidates，确认哪些 mode 共享 3D curve-frame route，哪些必须排除。
3. S2：已把 source candidates 路由到 scope、blocker、backendGap、fixture/oracle、nonGoal；`NormalToPath` 是 shared helper 合同，不是本包 release mode。
4. S3：已专项复审 projection / D1 / D2 / Frenet frame，冻结 `NormalToPath` shared helper 与 `FrenetNB/TN/TB` 实现合同。
5. S4：已专项复审 curvature center 与 aliases，冻结 `Concentric`、`SectionOfRevolution`、`AxisOfCurvature`、`Normal/Binormal`、`CenterOfCurvature` 的成功/失败边界。
6. S5：已复审 request-local response、writeback suggestions、capability exact blocker closeout，冻结不扩大 `ReferenceShadow.brep`、不引入 backend session、C5-M15 S6 未关闭前不改 exact blocker、excluded family 必须保留的发布闸门。
7. S6：已在 C5-M15 S6 关闭后完成 FreeCADCmd expected、C++、fixtures、focused tests、capability/docs 和验收记录。

## 验收分层

本轮方案短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M16-DatumCurveFrameCurvatureAttachEngine主线 docs/CADCore5.0-PartDesign-高价值剩余语义/README.md docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 cad-core
for f in docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M16-DatumCurveFrameCurvatureAttachEngine主线/矩阵/*.tsv docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M16-DatumCurveFrameCurvatureAttachEngine主线 --format markdown
```

实现短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
FREECADCMD=/Users/li/.cargo/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c51m5/partdesign-datum-curve-frame-modes.json --check
FREECADCMD=/Users/li/.cargo/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c51m5/partdesign-datum-curve-frame-diagnostics.json --check
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51x_datum_curve_frame_modes_match_expected tests.test_p7_features.CadCoreP7FeatureTest.test_c51x_datum_curve_frame_invalid_diagnostics
python3 -m unittest tests.test_expected_fixtures tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

阶段收口：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 已完成代码落点

| blocker | C++ 落点 | FreeCAD 依据 | tests | 成功标准 |
| --- | --- | --- | --- | --- |
| `C5M16-BLK-101` | `cad-core/src/part_design/datum_attachment.h` | `Attacher.cpp:1242-1282,1674-1755` | `test_c51x_datum_curve_frame_modes_match_expected`;`test_c51x_datum_curve_frame_invalid_diagnostics` | edge/curve only、edge+vertex、vertex+edge、vertex-first swap、`attachParameter` / vertex projection 和 D1 zero derivative 行为一致；`projection_failed` 保持 source/code-path-backed；`NormalToPath` 只作为 helper 合同 |
| `C5M16-BLK-201` | `datum_attachment.h` | `Attacher.cpp:1766-1831` | modes + diagnostics | D2 warning/`dd=0` 边界、T/N/B math、FrenetNB/TN/TB orientation、TN/TB undefined normal diagnostics 与 expected 一致；不生成 straight-line default frame |
| `C5M16-BLK-301` | `datum_attachment.h` | `Attacher.cpp:1832-1847` | modes + diagnostics | Concentric / SectionOfRevolution 先复用 S3 Frenet frame，再按 `curvature = dd.Dot(N) / pow(d.Magnitude(), 2)` 把 base 移到 `p + N * (1 / curvature)`；`N == 0` 是 infinite radius / undefined curvature diagnostic，不从 bbox 或 circle center 猜 |
| `C5M16-BLK-401` | `datum_attachment.h`、`datum_line.cpp`、`datum_point.cpp` | `Attacher.cpp:2400-2483,2833-2889` | modes parity | AxisOfCurvature / Normal / Binormal / CenterOfCurvature aliases 走 3D helper；AxisOfCurvature 额外做 Z-to-Y presuper rotation；line/point executor 只处理 shape convention，不重写 T/N/B/curvature 逻辑 |
| `C5M16-BLK-501` | `c_api.cpp`、C5/C51 docs、matrices | capability exact blocker source | adapter capability test | C5-M15 S6 已关闭；native expected/focused tests 通过后，exact blocker 只移除 expected-backed proven modes；excluded family 保留 |

禁止捷径：

- 不按 fixture 名称、bbox 或输出顺序推断曲线帧。
- 不把 straight-line undefined Frenet normal 伪造成 default frame。
- 不在 adapter 层实现 AttachEngine 业务逻辑。
- 不把 `Folding`、conic landmarks、`IntersectionPoint`、`TangentU/V` 顺带声明 supported。
