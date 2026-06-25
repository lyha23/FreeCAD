# C6-M9 S1 FreeCAD 源码与 native 失败证据复核

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
