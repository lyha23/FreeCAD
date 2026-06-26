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
- S0 冻结后的状态词典为 `already_supported`、`sync_required`、`oracle_candidate`、`known_gap_retained`、`backend_gap_candidate`、`backend_gap_requires_implementation`、`diagnostic_non_goal`。`copy_on_change_full_temporary_document_cache` 继续是 `known_gap_diagnostic` / `oracle_blocked`，不能写成 supported。

## 证明链条

```text
S0 live 基线与声明冻结
  -> S1 FreeCAD source / C8-M1 capability 复核
  -> S2 CopyOnChange DTO 与下游同步 scope 裁决
  -> S3 native CopyOnChange 生命周期探针
  -> S4 下游 opencascade-rs 同步契约
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
| S1 | `工作步骤细分/6-26-22-22-C8-M2-S1-FreeCAD源码与C8-M1能力复核.md` | source / capability 复核 |
| S2 | `工作步骤细分/6-26-22-23-C8-M2-S2-CopyOnChangeDTO准入与oracle候选矩阵.md` | oracle / backend gate 候选 |
| S3 | `工作步骤细分/6-26-22-24-C8-M2-S3-native-CopyOnChange生命周期探针与blocker证据.md` | native lifecycle 证据 |
| S4 | `工作步骤细分/6-26-22-25-C8-M2-S4-下游opencascade-rs同步契约方案.md` | 下游同步契约 |
| S5 | `工作步骤细分/6-26-22-26-C8-M2-S5-capability协议与前端接入边界发布.md` | capability / 前端边界 |
| S6 | `工作步骤细分/6-26-22-27-C8-M2-S6-实现准入与发布闸门.md` | 实现准入与发布闸门 |
| source candidates | `矩阵/c8m2_copyonchange_source_candidates.tsv` | FreeCAD / cad-core authority |
| scope review | `矩阵/c8m2_copyonchange_scope_review_matrix.tsv` | scope 与状态 |
| blocker queue | `矩阵/c8m2_copyonchange_blocker_queue.tsv` | blocker 与关闭条件 |
| non-goal | `矩阵/c8m2_copyonchange_non_goal_registry.tsv` | 非目标 |
| backend gap | `矩阵/c8m2_copyonchange_backend_gap_classification.tsv` | implementation gate |
| oracle plan | `矩阵/c8m2_copyonchange_oracle_plan.tsv` | native oracle / downstream sync 计划 |
| validation | `矩阵/c8m2_copyonchange_validation_matrix.tsv` | 验收命令 |

当前 S0 已实现，S1-S6 仍为待执行状态。矩阵中 S0 范围、non-goal 和 live blocker 行已冻结；oracle、下游同步、capability 和 release gate 结论仍不是发布闸门结论。
