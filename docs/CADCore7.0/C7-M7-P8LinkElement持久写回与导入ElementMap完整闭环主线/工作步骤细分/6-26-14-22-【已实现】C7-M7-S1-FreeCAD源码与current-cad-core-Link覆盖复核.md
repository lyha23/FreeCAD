# 【已实现】C7-M7 S1 FreeCAD 源码与 current cad-core Link 覆盖复核

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
- `cad-core/src/app/link.cpp`
- `cad-core/src/app/document_object.cpp`
- `cad-core/src/app/property_links.cpp`
- `cad-core/src/app/element_map.cpp`
- `cad-core/src/part/part_import.cpp`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/src/part/topo_shape_mapper.cpp`
- `cad-core/src/runtime/element_reference_update.cpp`
- `cad-core/src/mesh/feature_mesh_import.cpp`
- `cad-core/tests/test_p8_features.py`
- `cad-core/fixtures/p8`
- C7-M7 README、方案和矩阵

## 执行要点

1. 记录 live baseline 和 C7-M7 queue。
2. 记录 FreeCAD source authority：Link update / LinkElement lifecycle、ElementList / ElementCount / ShowElement、LinkSub routing、PropertyXLink / FullSubList、imported shape stable naming。
3. 复核 current `cad-core` 能力：Link display、alias retag、terminal / merge history propagation、documentObjectUpdates、elementReferenceUpdates、import shape indexed NamedShape、capability publication。
4. 复核哪些 fixture / expected 已经覆盖，哪些只是 current runtime diagnostic，哪些缺 native lifecycle。
5. 更新 `source_authority.tsv`、`scope.tsv`、`blocker_queue.tsv`、`backend_gate.tsv`、`oracle_plan.tsv`、`validation_matrix.tsv` 和方案 S1 小节。
6. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S2。

## S1 结论

- live 基线：执行时 `pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=7d014c588e`（`7d014c588e 文档：完成 C7-M7 S0 基线冻结`），`git status --short -uall` 无输出；队列显示 S1-S6 pending。
- FreeCAD source authority：`LinkBaseExtension::update()` 是 ElementCount / ElementList / ShowElement 和 LinkElement create / claim / sync / delete 的持久生命周期来源；`DocumentObject::getSubObject()` 经 `LinkBaseExtension::extensionGetSubObject()` 和 `getElementIndex()` 处理 numeric index、child object、`$Label`、collapsed `_iN`、linked target name/label、group / child-cache recursion；`PropertyXLink*` 负责对象与 subvalue 的保存、恢复和引用更新。
- Import / ElementMap source authority：BREP / STEP / IGES import 通过 Part import feature 读入 `TopoShape` 并由 `PropertyPartShape::setValue()` remap / restore `ElementMap`；`Mesh::Import` 是独立 mesh 路径；`PartFeature::getTopoShape()`、`PropertyTopoShape` 和 `TopoShapeMapper` 是 linked/imported ElementMap retag、MapperHistory 和 `NamedShape` 传播的依据。
- current `cad-core` coverage：当前实现路径不是旧文档里的 `cad-core/src/features`、`cad-core/src/document`、`cad-core/src/topo`，而是 `cad-core/src/app`、`cad-core/src/part`、`cad-core/src/runtime` 和 `cad-core/src/mesh`。已覆盖 request-local Link display / alias / FullSubList / mapped postfix / LinkElement / LinkGroup / ElementList / ElementCount / ShowElement `documentObjectUpdates`、`elementReferenceUpdates`、BREP / STEP / IGES `history_partial` import、STL `indexed_only` import 和 imported Link chain fixtures/tests。
- S2 输入池：完整 imported-shape `ElementMap` / stable reference lifecycle、ShowElement LinkElement / LinkGroup persistent writeback transaction、复杂多层 LinkSub / cross-document hash-postfix lifecycle。GUI / ViewProvider / Workbench、frontend sync protocol、跨请求 backend cache / persistent BREP、Worker / WASM / Web 继续是 diagnostic / non-goal。
- S1 未采集 FreeCAD oracle，未新增 fixture / expected / test，未改 C++；S1 blocker 和 S1 gate 已关闭，implementation gate 仍关闭。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'LinkBaseExtension|ShowElement|ElementList|ElementCount|LinkElement|LinkGroup|PropertyXLink|FullSubList|ElementMap|NamedShape|elementReferenceUpdates|documentObjectUpdates|ImportStep|ImportBrep|Mesh::Import' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线 src/App src/Mod/Part/App src/Mod/Mesh/App cad-core/src/app cad-core/src/part cad-core/src/mesh cad-core/src/runtime cad-core/include/cad_core/app cad-core/include/cad_core/part cad-core/tests/test_p8_features.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线 docs/CADCore7.0/README.md
git diff --check
```

## 完成标准

- S2 有明确的 oracle candidate 输入池和 already-covered baseline。
- S1 没有修改 C++、fixtures、expected 或 tests。
- 队列推进到 S2。
