# C12-M1 S1 FreeCAD 源码与 capability 候选矩阵

## 目标

复核 C12-M1 的 source authority：live capability 发布点、adapter tests、FreeCAD source entries、current cad-core landing 和历史 release evidence。S1 只更新 source candidates，不升级 support，不采 oracle，不改 C++。

## 输入

- `矩阵/c12m1_capability_candidate_source_candidates.tsv`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_adapters.py`
- `src/Mod/PartDesign/App/ShapeBinder.cpp`
- `src/App/Link.cpp`
- `src/App/Document.cpp`
- `src/Mod/Assembly/App/AssemblyObject.cpp`
- `src/Mod/Part/App/PartFeatures.cpp`
- `src/Mod/Part/App/AppPartPy.cpp`
- `src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp`
- `src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp`
- `src/Mod/Part/App/Tools.cpp`
- `src/Mod/Part/App/FeatureProjectOnSurface.cpp`

## 范围

1. 用 `rg` 或源码阅读刷新所有 source path、symbol 和 concise evidence。
2. 确认每个 candidate 的 current cad-core landing 是否真实存在。
3. 确认 adapter tests 是否覆盖 status / remaining_gaps / narrowed_gaps / non_goals。
4. 关闭 `C12M1-BLOCKER-101`，但不得创建 implementation row。

## 必须回写的矩阵行

- `C12M1-SRC-001..304`
- `C12M1-SCOPE-101..305`
- `C12M1-BLOCKER-101`
- `C12M1-VAL-101..104`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n "copy_on_change_full_temporary_document_cache|representative_solver_adapter|narrowed_gaps|part_sweep_located_profile|filling_surface_native_helper|part_loft_subelement|native_project_on_surface_mapper_history" cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py cad-core/tests/test_c8_shapebinder.py
rg -n "SubShapeBinder::setupCopyOnChange|SubShapeBinder::checkCopyOnChange|LinkBaseExtension::setupCopyOnChange|Document::copyObject|AssemblyObject::solve|handleOneSideOfJoint|Sweep::execute|Loft::execute|makeFilledFace|BRepOffsetAPI_MakeFillingPy|BuildPlateSurfacePy|ProjectOnSurface::execute" src/Mod/PartDesign/App/ShapeBinder.cpp src/App/Link.cpp src/App/Document.cpp src/Mod/Assembly/App/AssemblyObject.cpp src/Mod/Part/App/PartFeatures.cpp src/Mod/Part/App/AppPartPy.cpp src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp src/Mod/Part/App/Tools.cpp src/Mod/Part/App/FeatureProjectOnSurface.cpp
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次 docs/CADCore12.0/README.md
git diff --check
```

通过条件：

- source candidate 行不含 stale path。
- current landing 和 focused tests 均被明确记录。
- S1 不运行 FreeCADCmd，不新增 fixtures，不修改 C++。
- 验证后将本文件重命名为 `6-29-16-29-【已实现】C12-M1-S1-FreeCAD源码与capability候选矩阵.md`，并更新工作步骤索引。

## 非目标

- 不判断 CopyOnChange 是否可实现。
- 不把 source candidate 升级为 backend gap。
- 不改 capability contract。

## S1 结论

- `C12M1-SRC-001..304` 已完成 source authority 复核：capability 发布点、adapter assertions、`test_c8_shapebinder.py` CopyOnChange assertions、FreeCAD source entries 和 current cad-core landing 均可定位。
- `C12M1-SCOPE-101..305` 已回写 S1 source/test/landing evidence，但仍保留原 owner step：S3 处理 CopyOnChange，S4 处理 Assembly representative subset，S5 处理 Part Workbench narrowed / historical evidence。
- `C12M1-BLOCKER-101` 已关闭为 `closed_s1`；S1 没有采 oracle、没有运行 FreeCADCmd、没有修改 C++、没有新增 fixtures，也没有创建 implementation row。
- `C12M1-VAL-101..104` 已记录本轮 source/capability/current-landing/docs-only 验证口径；后续 S2-S6 仍需各自完成准入、复审与发布闸门。
