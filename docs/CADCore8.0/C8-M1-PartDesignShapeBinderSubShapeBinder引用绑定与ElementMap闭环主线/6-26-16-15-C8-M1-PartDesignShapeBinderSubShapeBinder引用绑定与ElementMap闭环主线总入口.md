# C8-M1 PartDesign ShapeBinder / SubShapeBinder 引用绑定与 ElementMap 闭环主线总入口

## 主线目标

C8-M1 是 CADCore8.0 的第一包，目标是把 FreeCAD `PartDesign::ShapeBinder` 与 `PartDesign::SubShapeBinder` 纳入 `cad-core` 的无状态 recompute 主路径。它必须一次性覆盖同一 FreeCAD 调用链下的代表性语义，而不是只做单 fixture：

- `ShapeBinder`：whole support、Face / Edge / Vertex subshape、多 subshape compound、Origin datum fallback、`TraceSupport` placement。
- `SubShapeBinder`：object / subshape / relative support、`MakeFace`、`Fuse`、`Offset`、`Refine`、Context matrix cache、Body 后续特征 profile 消费。
- lifecycle：`BindMode=Synchronized/Frozen/Detached`、`BindCopyOnChange=Disabled/Enabled/Mutated`、`PartialLoad` 的 request-local 与跨请求边界。
- topo：source ElementMap retag、ShapeBinder / SubShapeBinder output `NamedShape`、Body Tip replay 后的 stable subname / `elementReferenceUpdates`。

## 当前基线

- C7-M7 队列已清空，P8 Link / import 持久生命周期没有 C++ implementation gate。
- S0 live 基线已冻结：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=29da94dd13`（`29da94dd13 文档：完成 C7-M7 S6 发布闸门`），开始状态只包含本 C8-M1 文档包与 `docs/CADCore8.0/README.md` 未跟踪文件。
- `cad-core/src/runtime/feature_registry.cpp` 当前未注册 `PartDesign::ShapeBinder` / `PartDesign::SubShapeBinder` / `PartDesign::SubShapeBinderPython`；当前只能记录为 `backend_gap_candidate` / `oracle_candidate`，不能声明 supported。
- `cad-core/src/part_design/` 当前没有 ShapeBinder 专用 executor 文件。
- `cad-core` 已具备可复用能力：`part_design/body.cpp`、`part_design/profile_resolver.cpp`、`part/topo_shape_expansion.cpp`、`part/property_topo_shape.cpp`、`app/copy_on_change.cpp`、`runtime/element_reference_update.cpp` 和 `runtime/reference_resolution.cpp`。

## 证明链条

```text
S0 live 基线与批量边界冻结
  -> S1 FreeCAD ShapeBinder.cpp 调用链和 current cad-core 缺口复核（已完成）
  -> S2 oracle 候选与 backend gate 矩阵（已完成）
  -> S3 批量 native oracle / expected / blocker 采集（已完成）
  -> S4 cad-core C++ executor / DTO / topo 实现（已完成）
  -> S5 fixtures / focused tests / capability / docs 发布
  -> S6 release gate
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| ShapeBinder support | `src/Mod/PartDesign/App/ShapeBinder.cpp::ShapeBinder::getFilteredReferences()` | 只取第一个 `Part::Feature`，同对象 subshape 合并；无 subshape 时取 whole shape |
| ShapeBinder shape build | `src/Mod/PartDesign/App/ShapeBinder.cpp::ShapeBinder::buildShapeFromReferences()` | whole shape、单 subshape、多 subshape compound、Line / Plane / Point datum fallback |
| ShapeBinder placement | `src/Mod/PartDesign/App/ShapeBinder.cpp::ShapeBinder::updatedShape()` | `TraceSupport` 使用 source / target container placement 计算 transform |
| SubShapeBinder lifecycle | `src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::update()` | Support 解析、relative Context、matrix cache、CopyOnChange temporary copy、shape collection |
| SubShapeBinder geometry | `src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::update()` | `makeElementCompound`、`Fuse`、`makeElementWires`、`makeElementFace`、`makeElementOffset2D`、`makeElementRefine` |
| Link / nested object route | `src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::getSubObject()` | Support 子对象链、`$Label`、placement transform 和 link depth guard |
| CopyOnChange | `src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::setupCopyOnChange()` | 调用 `LinkBaseExtension::setupCopyOnChange()` 并维护 copied object cache |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| registry | `cad-core/src/runtime/feature_registry.cpp` | 注册 `PartDesign::ShapeBinder`、`PartDesign::SubShapeBinder`，必要时注册 Python variant diagnostic |
| executor | `cad-core/src/part_design/feature_shape_binder.cpp` | 实现 ShapeBinder / SubShapeBinder request-local executor |
| public header | `cad-core/include/cad_core/part_design/feature_shape_binder.h` | 暴露 executor 函数和必要 DTO |
| topo / geometry reuse | `cad-core/src/part/topo_shape_expansion.cpp`、`cad-core/src/part/property_topo_shape.cpp` | 使用已有 makeElement* / ElementMap / NamedShape 语义，不做输出端修剪 |
| runtime updates | `cad-core/src/runtime/element_reference_update.cpp`、`cad-core/src/runtime/reference_resolution.cpp` | 发布 stable subname / reference update 建议 |
| capability | `cad-core/src/runtime/capability_contract.cpp` | 发布 `part_design.shape_binder` / `part_design.sub_shape_binder` 能力与边界 |
| tests | `cad-core/tests/test_c8_shapebinder.py` 或 `cad-core/tests/test_p8_features.py` focused section | 验证 oracle parity、diagnostics、capability |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-26-16-15-【已实现】C8-M1工作步骤总入口.md` | 队列索引 |
| S0 | `工作步骤细分/6-26-16-16-【已实现】C8-M1-S0-live基线与Binder批量边界冻结.md` | 冻结声明口径 |
| S1 | `工作步骤细分/6-26-16-17-【已实现】C8-M1-S1-FreeCAD源码与current-cad-core覆盖复核.md` | 源码与现状复核 |
| S2 | `工作步骤细分/6-26-16-18-【已实现】C8-M1-S2-ShapeBinderSubShapeBinder-oracle候选矩阵.md` | oracle 候选与分类 |
| S3 | `工作步骤细分/6-26-16-19-【已实现】C8-M1-S3-native-oracle批量采集与expected固化.md` | 批量采集 native expected |
| S4 | `工作步骤细分/6-26-16-20-【已实现】C8-M1-S4-cad-core-ShapeBinderSubShapeBinder实现.md` | C++ executor / topo 实现 |
| S5 | `工作步骤细分/6-26-16-21-C8-M1-S5-fixtures-tests-capability发布.md` | fixtures/tests/capability 发布 |
| S6 | `工作步骤细分/6-26-16-22-C8-M1-S6-release-gate.md` | release gate |
| source authority | `矩阵/c8m1_shapebinder_source_authority.tsv` | FreeCAD source 证据 |
| scope | `矩阵/c8m1_shapebinder_scope.tsv` | 范围与状态 |
| oracle plan | `矩阵/c8m1_shapebinder_oracle_plan.tsv` | native oracle 采集计划 |
| backend gap | `矩阵/c8m1_shapebinder_backend_gap_classification.tsv` | implementation gate 分类 |
| blocker queue | `矩阵/c8m1_shapebinder_blocker_queue.tsv` | 阻塞项和关闭条件 |
| non-goal | `矩阵/c8m1_shapebinder_non_goal_registry.tsv` | 非目标与 reopen 条件 |
| validation | `矩阵/c8m1_shapebinder_validation_matrix.tsv` | 验收命令 |

当前 S0、S1、S2、S3、S4 已完成；S5-S6 为待执行状态。S4 已在 `cad-core` 落 `PartDesign::ShapeBinder` / `PartDesign::SubShapeBinder` executor、registry、CMake、capability 合约和 `tests.test_c8_shapebinder`，覆盖 Binder executor / ElementMap / BindMode request-local 子集；CopyOnChange full temporary-document cache 保持 `known_gap` diagnostic。
