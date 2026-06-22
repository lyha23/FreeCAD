# 【已实现】C5-M17-S3 ConicLandmark 合同复审

状态：`done_s3_contract_frozen`

## 目标

冻结 conic landmark 的 DTO、placement、diagnostics 和 expected 字段。S3 只做合同复审和矩阵状态更新；FreeCAD expected 采集、C++ helper、fixtures 实体文件和 focused tests 留到 S6。

## FreeCAD 依据

- `src/Mod/Part/App/Attacher.cpp:2413-2419`：`Asymptote1/2` 只注册 `rtHyperbola`，`Directrix1` 注册 `rtConic`，`Directrix2` 注册 `rtEllipse` / `rtHyperbola`。
- `src/Mod/Part/App/Attacher.cpp:2613-2702`：`AttachEngineLine` 通过 `BRepAdaptor_Curve` 读取 hyperbola asymptote 或 conic directrix。
- `src/Mod/Part/App/Attacher.cpp:2842-2845`：`Focus1` 注册 `rtConic`，`Focus2` 注册 `rtEllipse` / `rtHyperbola`。
- `src/Mod/Part/App/Attacher.cpp:2937-2990`：`AttachEnginePoint` 读取 ellipse/hyperbola/parabola focus，parabola second focus 抛错。
- `src/Mod/Part/App/Attacher.h:60-96`：MapMode 枚举把 line `Directrix/Asymptote` 与 point `Focus` 分属 DatumLine / DatumPoint。

## 合同结论

| MapMode | support 合同 | placement / point 合同 | diagnostic 边界 |
| --- | --- | --- | --- |
| `Asymptote1` / `Asymptote2` | 只接受 hyperbola edge | `base = gp_Hypr::Location()`；`direction = gp_Hypr::Asymptote1/2().Direction()` | ellipse / parabola / non-conic edge / wrong support / null edge 都是 diagnostic，不能 default placement |
| `Directrix1` | 接受 ellipse / hyperbola / parabola edge | ellipse/hyperbola 取 `Directrix1()`；parabola 取 `Directrix()`；expected 写 line base + direction | wrong support、non-conic edge、null edge 都是 diagnostic |
| `Directrix2` | 只接受 ellipse / hyperbola edge | ellipse/hyperbola 取 `Directrix2()`；expected 写 line base + direction | parabola `Directrix2` 是 diagnostic，不能按 `Directrix1` 或 default placement 兜底 |
| `Focus1` | 接受 ellipse / hyperbola / parabola edge | ellipse/hyperbola 取 `Focus1()`；parabola 取 `Focus()`；expected 写 point/base | wrong support、non-conic edge、null edge 都是 diagnostic |
| `Focus2` | 只接受 ellipse / hyperbola edge | ellipse/hyperbola 取 `Focus2()`；expected 写 point/base | parabola `Focus2` 是 diagnostic，不能按 `Focus1` 或 default placement 兜底 |

DatumLine / DatumPoint executor 只消费 conic helper 产出的 placement 或 point，不复制 conic edge 类型判断、asymptote/directrix/focus 业务规则。conic 业务落点仍是 `datum_attachment.h` 的 AttachEngine helper；adapter 只暴露 capability/diagnostics。

## Fixture 合同

| fixture | 覆盖场景 | expected 字段 |
| --- | --- | --- |
| `fixtures/c51m5/partdesign-datum-conic-landmark-modes.json` | ellipse：`Directrix1/2`、`Focus1/2`；hyperbola：`Directrix1/2`、`Asymptote1/2`、`Focus1/2`；parabola：`Directrix1`、`Focus1` | DatumLine 写 `base`、`direction`、`map_mode`、line shape convention；DatumPoint 写 `point` / `base`、`map_mode`、point shape convention；包含 request-local `documentObjectUpdates`（若 support recovery 发生） |
| `fixtures/c51m5/partdesign-datum-conic-landmark-diagnostics.json` | missing / unresolved support、wrong support、non-conic edge、null edge、parabola `Directrix2`、parabola `Focus2`、ellipse/parabola `Asymptote1/2` unsupported support | `diagnostic_codes` 至少稳定到既有 code family：`missing_link_target` / `subname_resolve_failed` / `attachment_support_invalid_shape`；同时记录 object/property/subname/message family；`documentObjectUpdates` 只能是 request-local suggestion |

S6 不得把 `Folding`、`IntersectionPoint`、`TangentU/V` 放进这两个 conic fixture；也不得把 hyperbola-only 或 parabola-only failure 变成 supported。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'mm1Asymptote|mm1Directrix|mm0Focus|Directrix1|Focus1|Asymptote1' src/Mod/Part/App/Attacher.cpp
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M17-DatumRemainingAttachEngine主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
for f in docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M17-DatumRemainingAttachEngine主线/矩阵/*.tsv docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M17-DatumRemainingAttachEngine主线/工作步骤细分 --format markdown
```

## 非目标

- 不实现代码。
- 不采集 expected。
- 不创建 fixture JSON 或 expected JSON。
- 不把 hyperbola-only / parabola-only failure 变成 supported。
- 不把 `Folding`、`IntersectionPoint`、`TangentU/V` 纳入 conic fixture。
