# C7-M1 S1 FreeCAD 源码与 oracle 候选矩阵

## 目标

按 FreeCAD 源码复核 Hole ModelThread、标准孔表 head cut、profile source 和 history 调用链；批量列出本轮 oracle / fixture 候选。S1 仍不改 C++、fixtures、expected 或 tests。

## 必读

- `src/Mod/PartDesign/App/FeatureHole.cpp`
- `cad-core/src/part_design/feature_hole.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_p7_features.py`
- `cad-core/fixtures/p7/hole-*.json`
- `cad-core/fixtures/p7/expected/hole-*.freecad.json`
- `docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/*.tsv`

## 动作

1. 记录 FreeCAD 调用链：`Hole::Hole()`、`readCutDefinitions()`、`updateHoleCutParams()`、`determineDiameter()`、`execute()`、`makeThread()`、`findHoles()`。
2. 对照 cad-core `feature_hole.cpp`：thread table、standard head cut lookup、ModelThread pipe-shell tool、compound tool、history freeze、metadata 输出。
3. 列出 supported native oracle fixtures、legacy pending expected rows、ModelThread + head cut rows、point/circle/arc profile source rows。
4. 更新 source、scope、input contract、oracle fixture、backend gap 矩阵；只写证据和候选，不做 route 结论。
5. 如果需要重新采集 FreeCAD expected，写清 collector 命令和本机 FreeCAD/LibPack/OCCT 基线；不要在 sandbox Qt 失败时直接判定实现失败。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'ModelThread|updateHoleCutParams|determineDiameter|getThreadPitch|getThreadClassClearance|makeThread|findHoles|readCutDefinitions' src/Mod/PartDesign/App/FeatureHole.cpp cad-core/src/part_design/feature_hole.cpp
find cad-core/fixtures/p7 -maxdepth 2 -type f \( -name '*hole*json' -o -name '*Hole*json' \) | sort
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线
```

## 通过条件

- oracle fixture matrix 覆盖 supported native oracle、legacy pending rows、ModelThread + head cut、profile source。
- source matrix 写清 FreeCAD 源文件、函数和 cad-core 落点。
- S1 文件名和标题标记为 `【已实现】` 后，队列推进到 S2。
