# 【已实现】C6-M8 S1 FreeCAD 源码与 capability 批量矩阵

## 目标

批量复核六个 Part Workbench surface owner 的 FreeCAD source authority、cad-core 落点、fixtures 和 adapter assertions。S1 只做 authority / matrix，不做实现。

## S1 live baseline

- `pwd=/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`f1f32b19c8`
- `git log -1 --oneline`：`f1f32b19c8 文档：完成 C6-M8 S0 live 基线冻结`
- `git -c core.quotepath=false status --short -uall`：空输出，S1 开始时工作区干净。
- `step_goal_queue.py` 刷新后当前队列从本 S1 文件开始。

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

## S1 复核结论

- `ProjectOnSurface`：FreeCAD authority 是 `FeatureProjectOnSurface.cpp::ProjectOnSurface::tryExecute()`；cad-core 落点是 `cad-core/src/part/part_project_on_surface.cpp`；当前 capability 已有 C4-M1/C5-M9 expected-backed published slice，但 `gui_projection_task_panel` 与 `unverified_advanced_branches` 同时位于 `remaining_gaps` 和 `non_goals`，S1 不裁决，S2 必须拆成单一路由。
- `RuledSurface`：FreeCAD authority 是 `PartFeatures.cpp::RuledSurface::execute()` 与 `TopoShapeExpansion.cpp::makeElementRuledSurface()`；当前 capability 为 `supported_wire_wire_expected_backed`，active `remaining_gaps=[]`，S4 只需做发布一致性复核。
- `Loft`：FreeCAD authority 是 `PartFeatures.cpp::Loft::execute()`、`PropertyLinkList` 与 `TopoShapeExpansion.cpp::makeElementLoft()`；C6-M7 已发布 selected subelement request-local product contract non-parity，`part_loft_subelement_assignment_native_hidden` 只保留为 `narrowed_gaps` / historical native-hidden evidence，不是 active gap。
- `Sweep`：FreeCAD authority 是 `PartFeatures.cpp::Sweep::execute()` 与 `TopoShapeExpansion.cpp::makeElementPipeShell()`；C6-M4 已发布 located profile / advanced combined CAD Core product contract non-parity，两个 FreeCADCmd wrapper blocker 只保留为 narrowed historical wrapper evidence。
- `Filling`：FreeCAD authority 是 `AppPartPy.cpp::makeFilledFace()` 与 `TopoShapeExpansion.cpp::makeElementFilledFace()`；C6-M5 已发布 Surface、Support/Order、ExplicitParams、non-boundary support/order product contract non-parity，native helper crash/timeout/notCollected 证据只保留为 narrowed historical evidence。
- `GeomPlate`：FreeCAD authority 是 `Tools.cpp::makeSurface()`、GeomPlate wrapper 和 PlateSurface wrapper；C6-M6 已发布 G1 CurveOnSurface / ProjectedCurve2d request-local product contract non-parity，并把 curve criteria 与 PlateSurface.Curves 分别冻结为 diagnostic / non-goal boundary。

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
