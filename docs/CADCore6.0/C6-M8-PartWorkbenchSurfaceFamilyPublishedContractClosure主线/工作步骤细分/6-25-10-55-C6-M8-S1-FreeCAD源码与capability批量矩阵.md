# C6-M8 S1 FreeCAD 源码与 capability 批量矩阵

## 目标

批量复核六个 Part Workbench surface owner 的 FreeCAD source authority、cad-core 落点、fixtures 和 adapter assertions。S1 只做 authority / matrix，不做实现。

## 必读

- `src/Mod/Part/App/FeatureProjectOnSurface.cpp`
- `src/Mod/Part/App/PartFeatures.cpp`
- `src/Mod/Part/App/AppPartPy.cpp`
- `src/Mod/Part/App/Tools.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_adapters.py`
- C6-M4、C6-M5、C6-M6、C6-M7 对应方案和矩阵。

## 动作

1. 对 `ProjectOnSurface`、`RuledSurface`、`Loft`、`Sweep`、`Filling`、`GeomPlate` 建 source candidate 行。
2. 对每个 owner 标明当前状态：expected-backed、product contract non-parity、historical evidence、narrowed gap、non-goal、active remaining gap。
3. 建立 representative fixture/oracle matrix：列出现有 checked-in fixtures、expected-backed subset、product contract fixture 和 diagnostic fixture。
4. 标出 S2 必须裁决的项，尤其是 `project_on_surface` 的 `gui_projection_task_panel` 与 `unverified_advanced_branches`。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'ProjectOnSurface::tryExecute|RuledSurface::execute|Loft::execute|Sweep::execute|makeFilledFace|makeSurface|makeElement' src/Mod/Part/App cad-core/src docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/矩阵/*.tsv
git diff --check -- docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线
```

## 通过条件

- source / scope / oracle / input contract 矩阵都能定位六个 owner。
- 没有把 historical evidence 写成 expected-backed。
- S1 文件名和标题标记为 `【已实现】` 后，队列推进到 S2。

