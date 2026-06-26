# C8-M2 SubShapeBinder CopyOnChange 下游同步与 DTO 准入主线总入口

## 主线目标

C8-M2 是 C8-M1 之后的源头方案包。它不重开 `ShapeBinder` / `SubShapeBinder` executor、ElementMap、NamedShape 或 Body replay 主路径，而是把下一步拆成可验证的两个方向：

- `C8M2-SYNC`：为下游 `opencascade-rs` / 前端接入 C8-M1 能力冻结同步合同。
- `C8M2-COC`：为 `SubShapeBinder BindCopyOnChange` request-local DTO 做 native oracle 准入，明确 full temporary-document cache 是否继续保持 `oracle_blocked`。

## 当前基线

- C8-M1 队列为空，S0-S6 均已实现。
- `cad-core/src/runtime/capability_contract.cpp` 当前发布 `part_design.shape_binder.status=supported_c8m1_expected_backed_request_local`，`remaining_gaps=[]`。
- `part_design.sub_shape_binder.status=supported_c8m1_expected_backed_request_local_with_copy_on_change_known_gap`，`remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- C8-M1 fixtures 和 expected 已在 `cad-core/fixtures/c8m1`，focused tests 已在 `cad-core/tests/test_c8_shapebinder.py`。
- C8-M2 S0 live 基线已冻结：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=dc93b0d3af`（`dc93b0d3af chore: 完成 C8-M1 S6 发布闸门`）。开始状态包含既有 C8-M2 文档/矩阵未提交改动；S0 只写文档和矩阵，不采 oracle、不改 C++、不修改 Rust 下游。
- C8-M2 S1 live 复核已完成：`HEAD=e7e07663d9`（`e7e07663d9 docs: 完成 C8-M2 S0 live 基线冻结`），开始工作区干净；已复核 FreeCAD `SubShapeBinder::setupCopyOnChange()` / `checkCopyOnChange()` / `onChanged()` / `update()`、`LinkBaseExtension::setupCopyOnChange()`、C8-M1 capability/tests/fixtures 和 current cad-core diagnostic 边界。
- C8-M2 S2 DTO 准入与 oracle 候选矩阵已完成：`HEAD=73a5acf8a8`（`73a5acf8a8 docs: 完成 C8-M2 S1 源码与能力复核`），开始工作区干净；已把 C8-M1 capability / diagnostics / fixtures 分类为 `sync_required`，CopyOnChange property-state 和 PartialLoad 分类为 `oracle_candidate`，full temporary-document cache 分类为 `known_gap_retained`，request-local DTO 分类为 `backend_gap_candidate`，GUI/session/persistent cache/Rust 下游分类为 `diagnostic_non_goal`。
- C8-M2 S3 native lifecycle probe 已完成：`HEAD=12be750a30`（`12be750a30 docs: 完成 C8-M2 S2 DTO 准入矩阵`），开始工作区干净；FreeCADCmd 采集 `freecad_version=1.2.0 revision 20260519`，`C8M2-ORACLE-101` / `102` 为 property-state collected，`C8M2-ORACLE-103` 仍为 retained `oracle_blocked`，因为 `_CopiedObjs`、`copyObject` dependency order 和 `recomputeFeature(true)` 生命周期未能导出为稳定 request-local DTO。
- C8-M2 S4 下游同步契约已完成：`HEAD=7b4eec93fe`（`7b4eec93fe docs: 完成 C8-M2 S3 native 生命周期探针`），开始工作区干净；已新增 `矩阵/c8m2_downstream_sync_contract.tsv`，关闭 `C8M2-SYNC-101..103` 和 `C8M2-BLOCKER-401`，并保持 full temporary-document cache 为 `known_gap_diagnostic` / `oracle_blocked`。
- S2 后的状态词典仍为 `already_supported`、`sync_required`、`oracle_candidate`、`known_gap_retained`、`backend_gap_candidate`、`backend_gap_requires_implementation`、`diagnostic_non_goal`。`copy_on_change_full_temporary_document_cache` 继续是 `known_gap_diagnostic` / `oracle_blocked`，不能写成 supported。

## 证明链条

```text
S0 live 基线与声明冻结
  -> S1 FreeCAD source / C8-M1 capability 复核（已完成）
  -> S2 CopyOnChange DTO 与下游同步 scope 裁决（已完成）
  -> S3 native CopyOnChange 生命周期探针
  -> S4 下游 opencascade-rs 同步契约（已完成）
  -> S5 capability / 前端协议发布边界
  -> S6 实现准入与发布闸门
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| SubShapeBinder CopyOnChange setup | `src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::setupCopyOnChange()` | 调用 `LinkBaseExtension::setupCopyOnChange()`，依赖 copied object cache |
| CopyOnChange mutation | `src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::checkCopyOnChange()` | copied property 分歧时进入 Mutated 边界 |
| PartialLoad | `src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::onChanged()` | `PartialLoad` 影响 Support allow-partial 行为 |
| Link CopyOnChange baseline | `src/App/Link.cpp::LinkBaseExtension` | 提供 App::Link CopyOnChange 的本地对照，不等同于 Binder support |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| Binder executor | `cad-core/src/part_design/feature_shape_binder.cpp` | 已实现 C8-M1 request-local Binder 主路径；C8-M2 只准入 CopyOnChange DTO 子集 |
| CopyOnChange helper | `cad-core/src/app/copy_on_change.cpp` | 可复用 App::Link request-local update 思路，不能搬成 backend session |
| capability | `cad-core/src/runtime/capability_contract.cpp` | 发布 known_gap / remaining_gaps / downstream sync contract |
| tests | `cad-core/tests/test_c8_shapebinder.py`、`cad-core/tests/test_diagnostics.py` | 锁定 C8-M1 supported 子集和 CopyOnChange diagnostic |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-26-22-20-【已实现】C8-M2工作步骤总入口.md` | 队列索引 |
| S0 | `工作步骤细分/6-26-22-21-【已实现】C8-M2-S0-live基线与同步范围冻结.md` | 冻结声明口径 |
| S1 | `工作步骤细分/6-26-22-22-【已实现】C8-M2-S1-FreeCAD源码与C8-M1能力复核.md` | source / capability 复核 |
| S2 | `工作步骤细分/6-26-22-23-【已实现】C8-M2-S2-CopyOnChangeDTO准入与oracle候选矩阵.md` | oracle / backend gate 候选 |
| S3 | `工作步骤细分/6-26-22-24-【已实现】C8-M2-S3-native-CopyOnChange生命周期探针与blocker证据.md` | native lifecycle 证据 |
| S4 | `工作步骤细分/6-26-22-25-【已实现】C8-M2-S4-下游opencascade-rs同步契约方案.md` | 下游同步契约 |
| S5 | `工作步骤细分/6-26-22-26-C8-M2-S5-capability协议与前端接入边界发布.md` | capability / 前端边界 |
| S6 | `工作步骤细分/6-26-22-27-C8-M2-S6-实现准入与发布闸门.md` | 实现准入与发布闸门 |
| source candidates | `矩阵/c8m2_copyonchange_source_candidates.tsv` | FreeCAD / cad-core authority |
| scope review | `矩阵/c8m2_copyonchange_scope_review_matrix.tsv` | scope 与状态 |
| blocker queue | `矩阵/c8m2_copyonchange_blocker_queue.tsv` | blocker 与关闭条件 |
| non-goal | `矩阵/c8m2_copyonchange_non_goal_registry.tsv` | 非目标 |
| backend gap | `矩阵/c8m2_copyonchange_backend_gap_classification.tsv` | implementation gate |
| oracle plan | `矩阵/c8m2_copyonchange_oracle_plan.tsv` | native oracle / downstream sync 计划 |
| downstream sync | `矩阵/c8m2_downstream_sync_contract.tsv` | 下游 TypeIds / fixtures / capability / diagnostics / ElementMap 合同 |
| validation | `矩阵/c8m2_copyonchange_validation_matrix.tsv` | 验收命令 |

当前 S0/S1/S2/S3/S4 已实现，S5-S6 仍为待执行状态。矩阵中 `C8M2-ORACLE-101..102` 已完成 S3 property-state evidence，`C8M2-ORACLE-103` 保持 `oracle_blocked` retained blocker，`C8M2-SYNC-101..103` 与 `C8M2-BLOCKER-401` 已关闭；S5 capability 发布和 S6 implementation/no-code 裁决仍不是本步结论。
