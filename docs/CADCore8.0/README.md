# CADCore8.0

CADCore8.0 承接 C7-M7 之后的下一轮 CAD Core 收口工作。C7-M7 已确认 P8 Link / imported-shape stable reference 方向没有 `backend_gap_requires_implementation`：完整 imported ElementMap、ShowElement 持久写回和 cross-document hash / postfix 生命周期均保持 `oracle_blocked` 或 `oracle_blocker`，不应继续在同一包里硬扩。

C8-M1 转向 `PartDesign::ShapeBinder` / `PartDesign::SubShapeBinder` 外部引用绑定与 ElementMap 闭环。这个方向有清晰 FreeCAD 源码入口、上游测试案例和当前 `cad-core` registry 缺口，且能复用已落地的 `PropertyLink*`、Link retag、ElementMap、Body replay、CopyOnChange 和 request-local `documentObjectUpdates` 语义。

## 入口

- C8-M1 总入口：`C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/6-26-16-15-C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线总入口.md`
- C8-M1 方案：`C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/6-26-16-15-C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环方案.md`
- C8-M1 工作步骤：`C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/工作步骤细分/`
- C8-M1 矩阵：`C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/矩阵/`

## 当前状态

- S0 live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=29da94dd13`（`29da94dd13 文档：完成 C7-M7 S6 发布闸门`），开始状态只包含本 C8-M1 文档包与 `docs/CADCore8.0/README.md` 未跟踪文件。
- C8-M1 为新建方案包；工作步骤总入口是索引文件，已标记 `【已实现】`。S0 已完成 live 基线冻结，S1 已完成 FreeCAD source authority 与 current cad-core coverage 复核，S2-S6 待执行。
- C8-M1 不重开 C7-M7 的 oracle-blocked Link 持久化行，不声明完整 CopyOnChange / Frozen / Detached 持久状态已支持。
- S1 复核确认 `cad-core/src/runtime/feature_registry.cpp` 未覆盖 `PartDesign::ShapeBinder`、`PartDesign::SubShapeBinder` 或 `PartDesign::SubShapeBinderPython`；`body.cpp`、`profile_resolver.cpp`、`topo_shape_expansion.cpp`、`property_topo_shape.cpp`、`copy_on_change.cpp` 和 `reference_resolution.cpp` 仅为可复用能力，不是 Binder 支持。
- C8-M1 的目标是在同一 FreeCAD 调用链下批量推进 ShapeBinder / SubShapeBinder：ShapeBinder whole/subshape/multi/TraceSupport/datum fallback，SubShapeBinder support/MakeFace/Fuse/Offset/Refine/Relative/Context/nested `getSubObject()`，BindMode/CopyOnChange/PartialLoad 审计，以及 ElementMap/NamedShape/Body replay；批量采集 oracle 后再补 `cad-core` C++ executor / DTO、fixtures、focused tests、capability/docs 和验收记录。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0
git diff --check
```
