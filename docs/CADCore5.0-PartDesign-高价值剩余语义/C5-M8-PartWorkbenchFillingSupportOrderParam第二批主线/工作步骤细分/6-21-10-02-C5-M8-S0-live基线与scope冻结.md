# C5-M8-S0 live 基线与 scope 冻结

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
