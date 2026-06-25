# 【已实现】C6-M9 S1 FreeCAD 源码与 native 失败证据复核

## 目标

批量复核 Groove UpTo exact blocker 的 FreeCAD source authority、cad-core 落点、fixtures、current diagnostic 和 adapter assertions。S1 只做 authority / evidence / matrix，不做实现。

## 必读

- `src/Mod/PartDesign/App/FeatureGroove.cpp`
- `src/Mod/PartDesign/App/FeatureRevolved.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `cad-core/src/part_design/feature_revolved.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_adapters.py`

## 动作

1. 建 source candidate 行：Groove execute、Revolved UpTo path、TopoShape native BRepFeat path、cad-core executor/helper、focused tests。
2. 对 `Groove Type=UpToFirst`、`Groove Type=UpToFace` 标明当前状态：native failure evidence、cad-core diagnostic、fixture expectation、capability exact blocker。
3. 建立 representative fixture/oracle matrix：列出现有 `c51m1` failure fixtures、expected/current diagnostic、可能的 `c6m9` product fixture 路线。
4. 标出 S2 必须裁决的项：继续保留 native failure，还是进入 CAD Core product non-parity 实现。

## S1 复核记录

- live cwd：`/Users/li/Chili3DProject/FreeCAD`。
- live HEAD：`bb03433646`（`bb03433646 文档：冻结 C6-M9 S0 live 基线`）。
- S1 执行起点 `git -c core.quotepath=false status --short -uall` 无输出，工作区在 S1 范围内干净。
- S1 执行起点队列：S1/S2/S3/S4/S5 均 pending，S1 完成后应推进到 S2。
- FreeCAD source authority 已复核：`FeatureGroove.cpp::Groove::execute()` 将 `Groove` 定义为 subtractive owner，`TypeEnums` 包含 `UpToFirst` / `UpToFace`，并调用 `executeRevolved(Part::RevolMode::CutFromBase)`；`FeatureRevolved.cpp::tryExecuteRevolved()` 对 `ToFirst` 通过 `getUpToFace()` 取目标面、对 `ToFace` 通过 `getUpToFaceFromLinkSub()` 取目标面，随后都进入 `tryToRevolveToFace()`；`tryToRevolveToFace()` 调用 `base.makeElementRevolution(..., revolMode, Standard_True)`；`TopoShapeExpansion.cpp::TopoShape::makeElementRevolution()` 对 profile faces 调用 `BRepFeat_MakeRevol::Init()` / `Perform(uptoface)`，`!IsDone()` 时抛出 `Revolution: Up to face: Could not revolve the sketch!`。
- cad-core 落点已复核：`cad-core/src/part_design/feature_revolved.cpp` 允许 `Groove` 的 `UpToFirst` / `UpToFace`，在 UpTo path 中解析 target face 并调用 `buildRevolvedUntil()`；`buildRevolvedUntil()` 对 `Groove` 使用 `revolMode=0`，调用 `cad-core/src/part/topo_shape_expansion.cpp::makeElementRevolutionUntilFromSources()`；该 helper 使用 `BRepFeat_MakeRevol`，失败时返回 `BRepFeat_MakeRevol could not revolve profile up to face`。
- fixture/current diagnostics 已复核：`c51m1/partdesign-groove-uptofirst-body` 的 recompute 诊断为 `execution_failed` / `BRepFeat_MakeRevol could not revolve profile up to face`，property=`Type`，subname=`UpToFirst`，随后追加 `Could not revolve the sketch`；`c51m1/partdesign-groove-uptoface-body` 的首个诊断同为 BRepFeat failure，property=`UpToFace`，target=`Pad`，subname=`Face4`，随后追加 `Could not revolve the sketch`。
- adapter assertions 已复核：`cad-core/src/runtime/capability_contract.cpp` 仍发布 `part_design.revolution_groove.status=supported_c51s1_advanced_with_exact_groove_upto_blocker`，fixtures 同时列出两个 `c51m1` Groove UpTo cases，`exact_blockers.id` 与 `remaining_gaps` 均为 `partdesign_groove_upto_brepfeat_cut_native_failure`；`cad-core/tests/test_adapters.py` 对同一 public contract 做断言。
- focused fixture assertion 已复核：`cad-core/tests/test_p7_features.py::test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers` 同批断言 `partdesign-groove-uptofirst-body` 和 `partdesign-groove-uptoface-body` 的两个 diagnostics，且 `Groove` 为 `error`、`Body` 为 `skipped`。
- S2 必裁决项：`Groove Type=UpToFirst` 与 `Groove Type=UpToFace` 必须作为同一 subtractive UpTo / `BRepFeat_MakeRevol` 语义批次裁决；S2 只能在 `backend_gap_requires_implementation`、`cad_core_product_contract_non_parity`、`historical_native_failure`、`retained_exact_blocker` 中选定一条公开路线，不得只处理其中一个 fixture，也不得把当前 native failure 写成 expected-backed success。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'Groove::execute|tryToRevolveToFace|generateRevolution|makeElementRevolution|BRepFeat_MakeRevol|partdesign_groove_upto_brepfeat_cut_native_failure' src/Mod/PartDesign/App src/Mod/Part/App cad-core/src cad-core/tests docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线/矩阵/*.tsv
git diff --check -- docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线
```

## 通过条件

- source / scope / oracle / input contract 矩阵都能定位 Groove UpTo blocker。
- 没有把 native failure 写成 expected-backed success。
- S1 文件名和标题标记为 `【已实现】` 后，队列推进到 S2。
