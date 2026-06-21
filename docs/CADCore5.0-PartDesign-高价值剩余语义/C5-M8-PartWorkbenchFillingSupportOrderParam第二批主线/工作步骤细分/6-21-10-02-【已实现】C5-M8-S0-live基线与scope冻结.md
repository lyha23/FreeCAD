# 【已实现】C5-M8-S0 live 基线与 scope 冻结

状态：`【已实现】`

## live baseline

```text
pwd
/Users/li/Chili3DProject/FreeCAD

git rev-parse --short HEAD
a1c5245f14

git log -1 --oneline
a1c5245f14 文档: 规划 C5-M8 Filling 第二批主线

git -c core.quotepath=false status --short -uall
（无输出，工作区干净）
```

## S0 核查结论

- FreeCAD 调用链已冻结为 `Part.makeFilledFace(...) -> TopoShape::makeElementFilledFace() -> BRepOffsetAPI_MakeFilling`。`AppPartPy.cpp::makeFilledFace()` 暴露 `shapes`、`surface`、`supports`、`orders` 与同组 constructor params；`TopoShapeMapper.h::BRepFillingParams` 同一结构承载 initial surface、support/order map、boundary range 和 params；`TopoShapeExpansion.cpp::makeElementFilledFace()` 在同一 builder 中执行 `LoadInitSurface`、boundary `Add(..., IsBound=true)`、non-boundary wire/edge `IsBound=false`、face constraint、vertex point constraint 和 `makeElementShape(maker, _shapes, op)`。
- `cad-core/src/part/part_filling.cpp` 当前只发布 source-backed helper 的 default boundary 路径：`Boundary=App::PropertyLinkSubList`、closed wire default、connected boundary edges default、default params metadata、boundary source evidence 和 `maker_history:filling`。
- `cad-core/fixtures/c3m4/part-filling-closed-wire-default.json`、`part-filling-boundary-edges-default.json`、`part-filling-invalid-inputs.json` 均有 `expected/*.freecad.json` 与 `tests/test_p8_features.py` 断言保护，属于 expected-backed first batch。
- `cad-core/fixtures/c4m1/part-filling-advanced-deferred.json` 没有对应 FreeCAD expected；`tests/test_p8_features.py` 只断言 `Surface`、`Supports`、`Orders`、`Degree` 输出 locatable `unsupported_property` diagnostics，属于 diagnostic-backed guard，不是 advanced support。
- C5-M8 因此不是单点 support/order case，而是同一 helper 参数结构和同一 Filling builder 下的最小完整语义批次。S0 只冻结 live guard 和 scope，不写 C++、不采集新 expected、不把 deferred 分支改成 supported。

## 冻结边界

| case / property | S0 live 状态 | 后续 owner |
| --- | --- | --- |
| `c3m4/part-filling-closed-wire-default` | expected-backed | guard only |
| `c3m4/part-filling-boundary-edges-default` | expected-backed | guard only |
| `c3m4/part-filling-invalid-inputs` | expected/diagnostic-backed | guard only |
| `c4m1/part-filling-advanced-deferred` | diagnostic-backed only | S1 / S2 |
| `Surface` / `Supports` / `Orders` | deferred diagnostic | S1 |
| non-default params | deferred diagnostic | S2 |
| non-boundary constraints | not published | S3 |
| compound optional / direct wrapper | not published or non-goal boundary | S4 |

## 目标

冻结 C5-M8 Filling 第二批的 live 基线，证明本轮不是单点 support/order，而是 `Part.makeFilledFace(...) -> TopoShape::makeElementFilledFace() -> BRepOffsetAPI_MakeFilling` 同一调用链下的最小完整语义批次。

## 必读

- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线/6-21-10-01-C5-M8-PartWorkbenchFillingSupportOrderParam第二批方案.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线/矩阵/*.tsv`
- `docs/CADCore3.0/capabilities-gap对照表.md`
- `src/Mod/Part/App/AppPartPy.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `cad-core/src/part/part_filling.cpp`
- `cad-core/include/cad_core/part/topo_shape_expansion.h`

## 产物

- 复核 `c3m4` first-batch Filling fixtures 和 `c4m1/part-filling-advanced-deferred` 当前状态。
- 必要时修正文档/矩阵措辞，让 C5-M8 的 `Surface` / `Supports` / `Orders` / params / non-boundary / compound / wrapper 边界一致。
- 更新本 step 文件名为 `【已实现】...`，并在局部 blocker queue 中关闭 C5M8-BLK-000。

## 非目标

- 不写 cad-core 实现。
- 不采集新 expected。
- 不把 unsupported 分支改成 supported。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M8-PartWorkbenchFillingSupportOrderParam第二批主线/工作步骤细分 --format markdown
```
