# C5-M17 DatumRemainingAttachEngine 方案

状态：`opened_after_c5m16_remaining_modes`

## 当前基线

C5-M14、C5-M15、C5-M16 已连续收口 Datum AttachEngine 的 proximity point、3D plane、curve-frame / curvature family。当前 capability exact blocker 只剩：

```text
Folding, Directrix1, Directrix2, Asymptote1, Asymptote2,
TangentU, TangentV, Focus1, Focus2, IntersectionPoint
```

这些 mode 并不属于同一 FreeCAD 调用链。C5-M17 先开 remaining owner 包，第一批只推进 conic landmarks；其它项必须保留拆包边界。

## 范围

| 项 | 本包处理 | 说明 |
| --- | --- | --- |
| `Directrix1` / `Directrix2` | 是 | DatumLine conic directrix family；ellipse/hyperbola/parabola 分支不同 |
| `Asymptote1` / `Asymptote2` | 是 | DatumLine hyperbola-only asymptote family |
| `Focus1` / `Focus2` | 是 | DatumPoint conic focus family；parabola 只有 Focus1 |
| invalid diagnostics | 是 | missing support、wrong shape、non-conic edge、parabola second focus/directrix、unsupported conic type |
| `Folding` | 否 | 四 line fold-angle 状态机，后续独立包 |
| `IntersectionPoint` | 否 | 需要单独确认 face/face route、support DTO 和 expected |
| `TangentU/V` | 否 | surface tangent branch，归属 TangentPlane / surface tangent package |
| GUI/session | 否 | 不新增 GUI、TaskPanel、ViewProvider 或 backend session |

## FreeCAD 调用链

1. `AttachEngineLine::AttachEngineLine()` 为 `Asymptote1/2` 注册 hyperbola support，为 `Directrix1/2` 注册 conic / ellipse / hyperbola support。
2. `AttachEngineLine::_calculateAttachedPlacement()` 对 `Asymptote1/2` 要求 edge 且 `BRepAdaptor_Curve::GetType() == GeomAbs_Hyperbola`，然后读取 `gp_Hypr::Asymptote1/2()`。
3. `Directrix1/2` 读取 ellipse/hyperbola 的 `Directrix1/2()`；parabola 只允许 `Directrix1`，`Directrix2` 抛错。
4. `AttachEnginePoint::AttachEnginePoint()` 为 `Focus1` 注册 conic support，为 `Focus2` 注册 ellipse/hyperbola support。
5. `AttachEnginePoint::_calculateAttachedPlacement()` 读取 ellipse/hyperbola 的 `Focus1/2()`；parabola 只允许 `Focus1`，`Focus2` 抛错。
6. line/point executor 只负责最终 datum shape convention，不在 adapter 或 JSON parser 中推断 conic landmarks。

## cad-core 实现边界

- `cad-core/src/part_design/datum_attachment.h`：
  - 增加 conic edge resolver 和 support diagnostics。
  - 增加 conic landmark placement helper，区分 line landmark 和 point landmark。
  - 明确 invalid diagnostics，不把 unsupported conic 类型降级成 default placement。
- `cad-core/fixtures/c51m5`：
  - 新增 `partdesign-datum-conic-landmark-modes.json`。
  - 新增 `partdesign-datum-conic-landmark-diagnostics.json`。
- `cad-core/tools/collect_freecad_expected.py`：
  - 如 expected 字段不足，补充 `map_mode` / conic landmark evidence，expected 必须来自 FreeCADCmd。
- `cad-core/src/adapters/c_api/c_api.cpp`：
  - S6 只删除 `Directrix1/2`、`Asymptote1/2`、`Focus1/2` 中 expected-backed proven modes。
  - `Folding`、`IntersectionPoint`、`TangentU/V` 留在 exact blocker。

## 实施顺序

1. S0：冻结 live remaining blocker，不改 capability。
2. S1：审计 FreeCAD source candidates，确认 conic landmarks 与 excluded families。
3. S2：把 source candidates 路由到 backendGap、later package、source unknown 或 non-goal。
4. S3：专项复审 conic landmark DTO / placement / diagnostics 合同。
5. S4：专项复审 `Folding`、`IntersectionPoint`、`TangentU/V` 拆包证据。
6. S5：复审 request-local placement、writeback suggestions 和 capability release gate。
7. S6：批量采集 FreeCADCmd expected，落 C++、fixtures、focused tests、capability/docs 和验收记录。

## 验收分层

本轮方案短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M17-DatumRemainingAttachEngine主线 docs/CADCore5.0-PartDesign-高价值剩余语义/README.md docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
for f in docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M17-DatumRemainingAttachEngine主线/矩阵/*.tsv docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M17-DatumRemainingAttachEngine主线 --format markdown
```

实现短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
FREECADCMD=/Users/li/.cargo/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c51m5/partdesign-datum-conic-landmark-modes.json --check
FREECADCMD=/Users/li/.cargo/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c51m5/partdesign-datum-conic-landmark-diagnostics.json --check
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51x_datum_conic_landmark_modes_match_expected tests.test_p7_features.CadCoreP7FeatureTest.test_c51x_datum_conic_landmark_invalid_diagnostics
python3 -m unittest tests.test_expected_fixtures tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

阶段收口：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 禁止捷径

- 不按 fixture 名称、bbox、中心点猜 directrix / focus / asymptote。
- 不把 hyperbola-only 或 parabola-only failure 静默降级。
- 不在 adapter 层实现 AttachEngine 业务逻辑。
- 不把 `Folding`、`IntersectionPoint`、`TangentU/V` 顺带声明 supported。
