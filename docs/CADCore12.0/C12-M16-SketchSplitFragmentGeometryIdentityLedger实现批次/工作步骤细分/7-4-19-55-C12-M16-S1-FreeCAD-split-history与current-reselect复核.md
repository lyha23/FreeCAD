# C12-M16 S1 FreeCAD split history 与 current reselect 复核

## 目标

复核 FreeCAD split history source authority 与 cad-core current split/reselect 行为，确认 C12-M16 的 red path 和 C++ 落点。

## 必读文件

- `../README.md`
- `../7-4-19-52-C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次方案.md`
- `../矩阵/c12m16_split_fragment_identity_source_matrix.tsv`
- `../矩阵/c12m16_split_fragment_identity_scope_matrix.tsv`
- `src/Mod/Part/App/FaceMakerBuildFace.cpp`
- `src/Mod/Part/App/FaceMaker.cpp`
- `src/Mod/Part/App/WireJoiner.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `src/Mod/Sketcher/App/SketchObject.cpp`
- `cad-core/include/cad_core/sketcher/sketch_edge_identity.h`
- `cad-core/src/sketcher/sketch_edge_identity.cpp`
- `cad-core/src/sketcher/sketch_internal_result.cpp`
- `cad-core/src/runtime/recompute.cpp`
- `cad-core/src/runtime/reference_resolution.cpp`
- `cad-core/tests/test_p5_sketch.py`

## 操作

1. 记录 FreeCAD `myPreSplitHistory`、`mySplitter`、`MapperHistory`、`MapperMaker`、`getInternalElementMap()` 的调用含义。
2. 复核 cad-core 当前 `g305:split1`、`subname_split_requires_reselect`、ReferenceShadow split 诊断和 raw ledger 行为。
3. 确认哪些 split 场景已有 diagnostic，哪些缺少 durable fragment ledger。
4. 更新 source / scope / blocker / validation 矩阵。
5. 将本步骤重命名为 `【已实现】`。

## 关闭条件

- FreeCAD split history source authority 已写入 source matrix。
- current reselect / missing ledger 行为已写入 scope matrix。
- S2 red tests 可直接从 S1 证据落地。

## 非目标

- 不修改 C++。
- 不新增 fixture。
- 不运行重型 build。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'myPreSplitHistory|mySplitter|MapperHistory|MapperMaker|splitSelfIntersecting|getInternalElementMap|getMappedName' src/Mod/Part/App/FaceMakerBuildFace.cpp src/Mod/Part/App/FaceMaker.cpp src/Mod/Part/App/TopoShapeExpansion.cpp src/Mod/Sketcher/App/SketchObject.cpp
rg -n 'g305:split|subname_split_requires_reselect|raw_edge_identity|byStableSubname|ReferenceShadow|split' cad-core/src/sketcher cad-core/src/runtime cad-core/tests/test_p5_sketch.py
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/矩阵/*.tsv
git diff --check
```
