# 【已实现】C5-M16-S3 CurveProjection / FrenetFrame 专项复审

## 目标

复审并固化 edge/curve parameter、optional vertex projection、D1/D2、Frenet T/N/B 和 `NormalToPath` / `FrenetNB/TN/TB` 的 placement 合同。

## FreeCAD 依据

- `src/Mod/Part/App/Attacher.cpp:1242-1282`：edge-driven mode table 接受 edge/curve/circle，并允许 edge+vertex 与 vertex+edge。
- `src/Mod/Part/App/Attacher.cpp:1674-1755`：`mmNormalToPath`、`mmFrenetNB/TN/TB`、`mmRevolutionSection`、`mmConcentric` 共享 curve projection / parameter / D1 分支。
- `src/Mod/Part/App/Attacher.cpp:1766-1831`：Frenet modes 共享 D2、T/N/B 和 mode orientation 分支。
- `src/Mod/Part/App/AttachExtension.cpp:753-762`：`MapPathParameter` 只在 point-on-curve mode 且单 edge ref 时可见。
- `cad-core/src/part_design/datum_attachment.h` 当前只存在 `MapPathParameter` / `NormalToEdge` 等相邻能力；本轮只冻结合同，不改代码。

## S3 冻结结论

`C5M16-BLK-101` 的可实现合同：

- support order：支持 edge/curve only、edge+vertex、vertex+edge；vertex-first 必须先 swap 成 path edge + optional vertex；missing support 和 wrong support 必须给 locatable diagnostic，不能落回 default placement。
- parameter source：无 vertex 时使用 `attachParameter` / `MapPathParameter` 在 `FirstParameter` 到 `LastParameter` 之间映射，infinite first/last 按 FreeCAD 归一为 `0..1`；有 vertex 时读取 vertex 点并用 `GeomAPI_ProjectPointOnCurve`，取 `LowerDistanceParameter()`。
- projection failure：`NbPoints() < 1` 是明确 diagnostic，不允许用 edge midpoint、bbox center 或原 vertex placement 代替。
- D1：`adapt.D1(u, p, d)` 后若 tangent magnitude 小于 `Precision::Confusion()`，必须 diagnostic；不能生成 default placement fallback。
- `NormalToPath` 只作为 shared projection/helper 合同，服务 S6 helper 设计，不是 C5-M16 release mode。

`C5M16-BLK-201` 的可实现合同：

- D2：FreeCAD `adapt.D2(u, p, d, dd)` 失败时只 warning，并把 `dd = gp_Vec(0,0,0)`；cad-core 可以用 diagnostic 或 precise blocker 表达这个边界，但不得静默生成 straight-line default frame。
- Frenet math：`T = d.Normalized()`；`N = dd - T * (dd dot T)`；`N` 大于 `Precision::SquareConfusion()` 时 normalize；`B = T.Crossed(N)`。
- orientation：`FrenetNB` normal = `T.Reversed()`、X = `N.Reversed()`；`FrenetTN` normal = `B`、X = `T`；`FrenetTB` normal = `N.Reversed()`、X = `T`。
- undefined normal：`FrenetTN` / `FrenetTB` 在 `N == 0` 时必须失败或保留 precise blocker；S6 不允许用 world axes、previous frame 或 straight-line default frame 伪造通过。

## S6 fixture plan

Success fixture `c51m5/partdesign-datum-curve-frame-modes.json` 至少覆盖：

- edge-only parameter：不同 `MapPathParameter` / `attachParameter` 产生不同 base。
- vertex projection：edge+vertex 与 vertex+edge 都命中 projection，并证明 vertex-first swap。
- support order：edge/curve/circle family 只接受 path support，不把 vertex 当 path。
- Frenet orientation：`FrenetNB`、`FrenetTN`、`FrenetTB` 的 base、normal 和 X axis 与 FreeCAD expected 一致。

Diagnostics fixture `c51m5/partdesign-datum-curve-frame-diagnostics.json` 至少覆盖：

- missing support / wrong support。
- projection failure。
- zero derivative / D1 failure。
- D2 failure 或 undefined Frenet normal；若 FreeCAD warning 后仍无法形成稳定 placement，保留 precise blocker，不转成 supported。

## cad-core 合同

- `resolveCurveFrameSupport()`
- `curveFrameParameter()`
- `frenetFrameAtParameter()`
- `normalToPathPlacement()`
- `frenetPlanePlacement()`

函数名只是建议，最终可以按现有 `datum_attachment.h` 风格调整；业务逻辑不得放到 adapter。

## 完成条件

- `C5M16-BLK-101` 和 `C5M16-BLK-201` 具备可实现合同。
- diagnostics fixture 覆盖 missing/wrong support、projection failure、zero derivative、undefined Frenet normal。
- 根 README / 根矩阵 / 包内矩阵状态前进到 S4 pending；capability exact blocker 仍 blocked by C5-M15 S6。
- 不采 oracle、不改 code、不声明 supported、不移除 exact blocker。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'attachParameter|GeomAPI_ProjectPointOnCurve|adapt\.D1|adapt\.D2|Frenet-Serret normal|path curve derivative|modeIsPointOnCurve' src/Mod/Part/App/Attacher.cpp src/Mod/Part/App/AttachExtension.cpp
for f in docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M16-DatumCurveFrameCurvatureAttachEngine主线/矩阵/*.tsv docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M16-DatumCurveFrameCurvatureAttachEngine主线 docs/CADCore5.0-PartDesign-高价值剩余语义/README.md docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M16-DatumCurveFrameCurvatureAttachEngine主线/工作步骤细分 --format markdown
```
