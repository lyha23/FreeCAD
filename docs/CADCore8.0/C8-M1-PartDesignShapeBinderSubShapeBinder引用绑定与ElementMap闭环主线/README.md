# C8-M1 PartDesign ShapeBinder / SubShapeBinder 引用绑定与 ElementMap 闭环主线

本目录承接 C7-M7 release gate 之后的下一轮 CAD Core 实现方向。C7-M7 已把 P8 Link / imported-shape 持久生命周期裁为 oracle-blocked / no backendGap，因此 C8-M1 不继续扩大 LinkElement 持久写回，而是转向当前 registry 未覆盖、但 FreeCAD 源码和测试明确存在的 `PartDesign::ShapeBinder` / `PartDesign::SubShapeBinder`。

C8-M1 的目标不是做单 case 探针，而是把同一 `src/Mod/PartDesign/App/ShapeBinder.cpp` 调用链、同一 Binder DTO / executor 边界、同一类 expected 能覆盖的多个代表性场景纳入一轮：ShapeBinder whole / subshape / multi-subshape / TraceSupport，SubShapeBinder support / MakeFace / Fuse / Offset / Refine / Relative，BindMode 与 CopyOnChange 生命周期准入，ElementMap / NamedShape / Body 后续引用恢复。

## 入口

- 主线总入口：`6-26-16-15-C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线总入口.md`
- 方案：`6-26-16-15-C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环方案.md`
- 工作步骤总入口索引：`工作步骤细分/6-26-16-15-【已实现】C8-M1工作步骤总入口.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- S0 live 基线已冻结：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=29da94dd13`（`29da94dd13 文档：完成 C7-M7 S6 发布闸门`）。S0 开始状态只包含本 C8-M1 文档包与 `docs/CADCore8.0/README.md` 未跟踪文件，未发现无关 dirty 文件。
- 当前 `cad-core/src/runtime/feature_registry.cpp` 未注册 `PartDesign::ShapeBinder`、`PartDesign::SubShapeBinder` 或 `PartDesign::SubShapeBinderPython`；S0 不能声明 supported 或 `backend_gap_requires_implementation`。
- FreeCAD source authority 明确：`ShapeBinder::updatedShape()`、`ShapeBinder::buildShapeFromReferences()`、`SubShapeBinder::update()`、`SubShapeBinder::getSubObject()`、`setupCopyOnChange()`、`setLinks()` 与 `onChanged()` 构成主链。
- 上游测试已有候选 oracle：`PartDesignTests/TestShapeBinder.py` 覆盖跨 Body ShapeBinder、SubShapeBinder edge offset、binder before / after Pad、binder as Revolution profile；`PartDesignTests/TestTopologicalNamingProblem.py` 覆盖 ShapeBinder / SubShapeBinder ElementMap。
- S0 已完成 live 基线与批量边界冻结；S1 已完成 FreeCAD source authority 与 current cad-core coverage 复核。下一 pending 是 S2 oracle 候选矩阵。索引文件已标 `【已实现】` 仅表示队列索引完成。
- S1 结论：current `feature_registry.cpp` 未注册 `PartDesign::ShapeBinder`、`PartDesign::SubShapeBinder` 或 `PartDesign::SubShapeBinderPython`；current `body.cpp`、`profile_resolver.cpp`、`topo_shape_expansion.cpp`、`property_topo_shape.cpp`、`copy_on_change.cpp`、`reference_resolution.cpp` 和 P7/P8 tests 可复用，但不能记录为 Binder supported。

## 收口边界

- 本包必须批量覆盖 ShapeBinder 与 SubShapeBinder 的同源语义，不允许长期停在单 fixture：ShapeBinder whole/subshape/multi/TraceSupport/datum fallback，SubShapeBinder support/MakeFace/Fuse/Offset/Refine/Relative/Context/nested `getSubObject()`，以及 BindMode/CopyOnChange/PartialLoad、ElementMap/NamedShape/Body replay。
- S0/S1/S2 只做证据、边界和矩阵；S3 批量采 native oracle；S4 才按 oracle 和 current mismatch 落 C++ executor；S5 发布 fixtures/tests/capability/docs；S6 release gate。
- CopyOnChange、Frozen、Detached、PartialLoad 若被证明依赖跨请求 FreeCAD 文档状态，可在本包内裁为 `oracle_blocked` 或 `diagnostic_non_goal`，但必须先审计同一调用链，不另开薄包。
- 不从 current `cad-core` 输出倒推 FreeCAD expected，不在 adapter/output 层做 subshape 猜测，不引入跨请求 BREP / shape / ElementMap 状态。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线 docs/CADCore8.0/README.md
git diff --check
```
