# 【已实现】C12-M7 S1 native failure 与 current diagnostic 复核

## 目标

复核 FreeCAD native Groove UpTo failure 是否仍成立，并确认 current CAD Core diagnostic 来自 PartDesign / TopoShape source path。

## 必读来源

- `src/Mod/PartDesign/App/FeatureGroove.cpp`
- `src/Mod/PartDesign/App/FeatureRevolved.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `cad-core/src/part_design/feature_revolved.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/fixtures/c51m1/partdesign-groove-uptofirst-body.json`
- `cad-core/fixtures/c51m1/partdesign-groove-uptoface-body.json`
- `docs/CADCore5.1-PartDesign-剩余deferred语义实现/工作步骤细分/6-20-17-35-【已实现】C51X-S1-GrooveUpTo-native证据复核.md`

## 操作

1. 用 `rg` / `sed` 复核 FreeCAD Groove / Revolved / TopoShapeExpansion 调用链。
2. 读取 C51X-S1 native evidence，必要时只做轻量 FreeCADCmd probe 复核；若 FreeCADCmd 环境不稳定，记录为 runtime evidence blocker，不伪造 native success。
3. 运行 current recompute 或 focused test，确认两个 fixtures 的 exact diagnostic。
4. 更新 source / scope / contract / blocker 矩阵中 S1 行。

## 复核结论

- 本轮 live 起点为 `pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=fdeea2443e`（`fdeea2443e 文档：冻结 C12-M7 S0 live 基线`），起点 worktree clean。
- FreeCAD source chain 已复核：`src/Mod/PartDesign/App/FeatureGroove.cpp::Groove::execute()` 调用 `executeRevolved(Part::RevolMode::CutFromBase)`；`src/Mod/PartDesign/App/FeatureRevolved.cpp::Revolved::tryExecuteRevolved()` 对 `ToFirst` / `ToFace` 取得 up-to face 后进入 `tryToRevolveToFace()`；`tryToRevolveToFace()` 调用 `base.makeElementRevolution(..., revolMode, Standard_True)`；`src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRevolution()` 对 profile face 调用 `BRepFeat_MakeRevol::Init()` / `Perform(uptoface)`，`!IsDone()` 时抛出 `Revolution: Up to face: Could not revolve the sketch!`。
- 旧 C51X native evidence 已复核并继续采用：`FreeCADCmd 1.2.0 revision 20260519` 下 `partdesign-groove-uptofirst-body` 与 `partdesign-groove-uptoface-body` 仍报 `Groove: Revolution: Up to face: Could not revolve the sketch!`；本轮轻量探测 `freecadcmd --version` 输出同一版本 `FreeCAD 1.2.0 Revision: 20260519 (Git shallow)`。本轮未刷新或改写 oracle expected，也未声称 native success。
- CAD Core current path 已复核：`cad-core/src/part_design/feature_revolved.cpp::buildRevolvedUntil()` 调用 `part::makeElementRevolutionUntilFromSources()`；`cad-core/src/part/topo_shape_expansion.cpp::makeElementRevolutionUntilFromSources()` 使用 `BRepFeat_MakeRevol`，失败时返回 primary diagnostic `BRepFeat_MakeRevol could not revolve profile up to face`。
- current focused test 已通过：`python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers`。两个 fixtures 仍是 exact diagnostic：第一条 `BRepFeat_MakeRevol could not revolve profile up to face`，第二条 `Could not revolve the sketch`；`Groove` 为 `error`，`Body` 为 `skipped`。
- S1 只关闭 native/current evidence 复核和 `C12M7-BLOCKER-101`；不批准 product diagnostic contract，不改 `cad-core/src`、`cad-core/include`、fixtures、expected、tests、adapters 或 capability source。下一步仍是 S2 product diagnostic contract 准入裁决。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n "Groove::execute|tryExecuteRevolved|makeElementRevolution|BRepFeat_MakeRevol" src/Mod/PartDesign/App src/Mod/Part/App cad-core/src/part_design cad-core/src/part
cd cad-core && python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers
cd .. && python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M7-PartDesignGrooveUpTo产品契约准入批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M7-PartDesignGrooveUpTo产品契约准入批次/矩阵/*.tsv
git diff --check
```
