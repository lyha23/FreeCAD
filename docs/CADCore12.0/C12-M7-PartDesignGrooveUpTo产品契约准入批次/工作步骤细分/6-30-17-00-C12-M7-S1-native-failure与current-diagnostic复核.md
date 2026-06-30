# C12-M7 S1 native failure 与 current diagnostic 复核

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

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n "Groove::execute|tryExecuteRevolved|makeElementRevolution|BRepFeat_MakeRevol" src/Mod/PartDesign/App src/Mod/Part/App cad-core/src/part_design cad-core/src/part
git diff --check
```
