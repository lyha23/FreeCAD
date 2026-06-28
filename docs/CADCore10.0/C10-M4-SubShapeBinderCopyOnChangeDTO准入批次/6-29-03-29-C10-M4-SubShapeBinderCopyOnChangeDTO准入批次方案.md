# C10-M4 SubShapeBinder CopyOnChange DTO 准入批次方案

## 背景

C10-M3 已把 stale `ReferenceShadow` / `ShadowSub` native recovery 关闭为 docs-only retained diagnostic / release gate。当前 live capability 中仍有一个非空 `remaining_gaps`：`part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`。此前 C8-M1 / C8-M2 / C9-M5 均把它保留为 known gap，因为 FreeCAD 的 CopyOnChange copied-object lifecycle 依赖 session-local temporary document/cache，不能直接进入 cad-core 无状态后端。

C10-M4 的问题不是“照搬 FreeCAD temporary document cache”，而是判断是否存在可落地的 request-local DTO：前端在请求里携带 copied-object evidence 或写回意图，cad-core 只在单次 recompute 内计算并返回 `documentObjectUpdates`，请求结束后不保留 copied graph、shape 或 cache。

## FreeCAD 调用链

- `src/Mod/PartDesign/App/ShapeBinder.cpp` 定义 `PartialLoad`、`BindCopyOnChange` enum，并在 `SubShapeBinder::setupCopyOnChange()`、`SubShapeBinder::checkCopyOnChange()`、`SubShapeBinder::update()`、`SubShapeBinder::onChanged()` 里触发 CopyOnChange / PartialLoad 生命周期。
- `src/App/Link.cpp` 与 `src/App/Link.h` 的 `LinkBaseExtension::setupCopyOnChange()`、`checkCopyOnChange()`、`makeCopyOnChange()` 是 copied-object lifecycle 的共享来源。
- FreeCAD 正式路径会生成或切换 copied object；cad-core 不能依赖该 session-local object，只能消费请求内 DTO 并返回前端 graph update 建议。

## cad-core 当前边界

- `cad-core/src/part_design/feature_shape_binder.cpp` 已读取 `BindCopyOnChange` / `PartialLoad`，并对 Enabled / Mutated / PartialLoad 发布 `copy_on_change_full_temporary_document_cache_not_supported` diagnostic。
- `cad-core/src/app/copy_on_change.cpp`、`cad-core/include/cad_core/app/copy_on_change.h` 已有 App::Link request-local CopyOnChange `documentObjectUpdates` 模型，可作为 DTO 边界参考，但不能自动等同于 SubShapeBinder supported。
- `cad-core/src/app/link.cpp` 已在 Link lifecycle 中发布 stateless `documentObjectUpdates`。
- `cad-core/src/runtime/capability_contract.cpp` 仍把 SubShapeBinder CopyOnChange 标为 known gap diagnostic / oracle blocked，并列入 `remaining_gaps`。

## 实施原则

- 先证据，后实现：没有 FreeCAD native copied-object evidence，不打开 C++ gate。
- 先 DTO，后几何：必须先写清 request-local DTO 的输入、输出、前端 graph writeback 和诊断边界，再考虑 shape 计算差异。
- 无状态边界不可退让：后端不保存 temporary document、copied object、BREP、TopoDS、NamedShape、ElementMap 或 cache。
- `backend_gap_candidate` 必须同时有 FreeCAD authority、native observable expected、产品批准 DTO 和 current cad-core mismatch。
- 只允许在正式模块落语义：`part_design/feature_shape_binder`、`app/copy_on_change`、`runtime/documentObjectUpdates`、capability 和 focused tests。

## S0-S6 拆分

| 步骤 | 目标 | 关键输出 |
| --- | --- | --- |
| S0 | 冻结 live baseline 和声明口径 | README、总入口、status vocabulary、forbidden claims 和 validation matrix 对齐。 |
| S1 | 复核 FreeCAD 源码候选和 current coverage | source candidate TSV 回写真实源码 / cad-core 路径；不升级 supported。 |
| S2 | 做范围准入与 blocker 路由 | scope / blocker / non-goal / backend-gap TSV 全部有 owner step 和 close condition。 |
| S3 | CopyOnChange native probe 与 DTO evidence 复审 | 判断 copied-object evidence 是否可 request-local 化；必要时更新 probe expected。 |
| S4 | cad-core 请求 DTO 与 `documentObjectUpdates` 复审 | 对比 current diagnostic、App::Link DTO 和 SubShapeBinder gap，打开或拒绝 implementation row。 |
| S5 | 临时文档禁用与 non-goal 边界复审 | 关闭 backend session、persistent cache、full BREP / TopoDS persistence 和 GUI lifecycle。 |
| S6 | Oracle 实现与发布闸门 | 有 row 则落 C++ / tests / fixtures / capability；无 row 则 no-code retained diagnostic。 |

## S6 代码落点规则

S6 只有在 S3-S5 产生 `backend_gap_candidate` 时才改代码。允许的落点包括：

- SubShapeBinder executor：`cad-core/src/part_design/feature_shape_binder.cpp`、`cad-core/include/cad_core/part_design/feature_shape_binder.h`。
- CopyOnChange DTO helper：`cad-core/src/app/copy_on_change.cpp`、`cad-core/include/cad_core/app/copy_on_change.h`。
- Runtime update transport：现有 `documentObjectUpdates` 结果结构和 adapter output；adapter 只能透传核心结果。
- Capability / tests：`cad-core/src/runtime/capability_contract.cpp`、`cad-core/tests/test_p7_features.py`、`cad-core/tests/test_adapters.py`。
- Fixtures / oracle：必要时新增 `cad-core/fixtures/c10m4` 和 FreeCAD collector / probe。

禁止在 adapter 层输出修剪、fixture 名称分支、几何猜测、cross-request cache 或 frontend-only mock 中补 CopyOnChange 语义。

## 验收分层

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0/C10-M4-SubShapeBinderCopyOnChangeDTO准入批次 docs/CADCore10.0/README.md
git diff --check
```

代码闸门触发后：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_adapters
```

重型收口只在 S6 实际修改 collector、fixtures、capability 或核心 C++ 后执行；S0-S5 文档 / 准入步骤不跑 cad-core build。
