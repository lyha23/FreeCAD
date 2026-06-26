# C7-M7 S1 FreeCAD 源码与 current cad-core Link 覆盖复核

## 目标

复核 P8 Link / imported-shape stable reference 的 FreeCAD source authority、当前 `cad-core` implementation、fixtures、expected、capability 和 focused tests。S1 只写文档和矩阵，不新增 fixtures/expected/tests，不运行 FreeCAD oracle，不改 C++。

## 必读文件

- `src/App/Link.cpp`
- `src/App/Link.h`
- `src/App/DocumentObject.cpp`
- `src/App/PropertyLinks.cpp`
- `src/Mod/Part/App/PartFeature.cpp`
- `src/Mod/Part/App/PropertyTopoShape.cpp`
- `src/Mod/Part/App/TopoShape.cpp`
- `cad-core/src/features/link.cpp`
- `cad-core/src/features/part.cpp`
- `cad-core/src/document`
- `cad-core/src/topo`
- `cad-core/tests/test_p8_features.py`
- `cad-core/fixtures/p8`
- C7-M7 README、方案和矩阵

## 执行要点

1. 记录 live baseline 和 C7-M7 queue。
2. 记录 FreeCAD source authority：Link update / LinkElement lifecycle、ElementList / ElementCount / ShowElement、LinkSub routing、PropertyXLink / FullSubList、imported shape stable naming。
3. 复核 current `cad-core` 能力：Link display、alias retag、terminal / merge history propagation、documentObjectUpdates、elementReferenceUpdates、import shape indexed NamedShape、capability publication。
4. 复核哪些 fixture / expected 已经覆盖，哪些只是 current runtime diagnostic，哪些缺 native lifecycle。
5. 更新 `source_authority.tsv`、`scope.tsv`、`blocker_queue.tsv` 和方案 S1 小节。
6. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S2。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'LinkBaseExtension|ShowElement|ElementList|ElementCount|LinkElement|LinkGroup|PropertyXLink|FullSubList|ElementMap|NamedShape|elementReferenceUpdates|documentObjectUpdates|ImportStep|ImportBrep|Mesh::Import' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线 src/App src/Mod/Part/App cad-core/src/features cad-core/src/document cad-core/src/topo cad-core/tests/test_p8_features.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线 docs/CADCore7.0/README.md
git diff --check
```

## 完成标准

- S2 有明确的 oracle candidate 输入池和 already-covered baseline。
- S1 没有修改 C++、fixtures、expected 或 tests。
- 队列推进到 S2。
