# C12-M1 CAD Core capability implementation candidate 盘点批次总入口

本文是 `docs/CADCore12.0` 下的第一轮 CAD Core 下一实现候选盘点主线。

对应上游方案入口是 `docs/CADCore12.0/README.md`。C12-M1 不直接落 C++，而是把 live capability 和历史 release gate 复核成下一包 implementation 授权依据。

## 主线目标

- 冻结 C11-M1 / C11-M2 队列关闭后的 live capability 基线。
- 区分 active `remaining_gaps`、historical `narrowed_gaps`、representative subset、non-goal 和 release gate。
- 防止把 CopyOnChange retained known gap、Part Workbench helper `notCollected`、Assembly representative fallback 或 native-hidden evidence 误当成可实现 backend gap。
- S6 已确认没有满足条件的行，最终发布 `no_code_backlog_gate`；本轮无代码落点，不授权 C++、fixtures、expected、oracle 采集、capability wording 或 adapter/test 改动。

## 当前基线

- S0 live 冻结 `pwd=/home/user/Chili3DProject/FreeCAD`。
- S0 live 冻结 `HEAD=4446df0c87`（`4446df0c87 docs: 关闭 C11-M2 S6 发布闸门`）。
- S0 起点 dirty boundary 只包含 `docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/**` 与 `docs/CADCore12.0/README.md` 未跟踪文件；未发现 `cad-core/src`、tests、fixtures、expected 或 collector 改动。
- C11-M1 / C11-M2 队列检查均只输出表头，确认两条 C11 closed line 不自动重开。
- `cad-core/cad-core capabilities` 显示唯一非空 `remaining_gaps` 是 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`，`sub_shape_binder.status=supported_c8m1_expected_backed_request_local_with_copy_on_change_known_gap`，known gap 状态仍为 `known_gap_diagnostic` / `oracle_blocked`。
- `part_workbench.sweep`、`part_workbench.filling`、`part_workbench.geomplate`、`part_workbench.loft`、`part_workbench.project_on_surface` 保留 narrowed / historical evidence，但当前 `remaining_gaps=[]`。
- `assembly.representative_solver_adapter` 是 fallback metadata，`assembly.ondsel_solver_adapter.subshape_marker_placement` 是 covered representative subset，均不是自动 implementation row。

## 证明链条

```text
live capability baseline
  -> FreeCAD / cad-core source candidates
  -> scope review / nonGoal / blocker queue
  -> active remaining_gap audit
  -> representative subset / product boundary audit
  -> historical non-parity / narrowed evidence audit
  -> next-batch code authorization or no-code backlog gate
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| SubShapeBinder CopyOnChange | `~/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::setupCopyOnChange()` / `checkCopyOnChange()` | Full path 依赖 temporary document、`copyObject()`、`recomputeFeature(true)` 和 copied-object lifecycle；当前不能无状态持久化。 |
| App Link CopyOnChange 参考 DTO | `~/Chili3DProject/FreeCAD/src/App/Link.cpp::LinkBaseExtension::setupCopyOnChange()`；`~/Chili3DProject/FreeCAD/src/App/Document.cpp::Document::copyObject()` | App::Link 可作为 request-local DTO 词汇参考，但不能自动等同 SubShapeBinder CopyOnChange supported。 |
| Assembly marker / solver | `~/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::handleOneSideOfJoint()` / `solve()` | request-local marker placement 已有 representative subset；full solver 和 persistent solver state 是 non-goal。 |
| Part Sweep / Loft | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` / `Loft::execute()` | Native DocumentObject property chain 与 helper/wrapper advanced paths 分开；helper `notCollected` 不能直接升级为 backend gap。 |
| Filling helper | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::Module::makeFilledFace()`；`~/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp` | C11-M2 已复采 helper oracle，仍无 stable native expected。 |
| GeomPlate helper | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp`；`~/Chili3DProject/FreeCAD/src/Mod/Part/App/Tools.cpp::makeSurface()` | 保留 product-contract / native-hidden / wrapper-lifecycle evidence，不是当前 active gap。 |
| ProjectOnSurface | `~/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::execute()` | 已发布 expected-backed slice；mapper-history native evidence 仍是 probe-only narrowed gap。 |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| runtime | `cad-core/src/runtime/capability_contract.cpp` | 发布 status、remaining_gaps、narrowed_gaps、non_goals 和 known gap 边界。 |
| adapters/tests | `cad-core/tests/test_adapters.py` | 锁定 capability JSON 字段边界和 release wording。 |
| part_design | `cad-core/src/part_design/feature_shape_binder.cpp` | SubShapeBinder request-local support 与 CopyOnChange retained diagnostic。 |
| app | `cad-core/src/app/copy_on_change.cpp`、`cad-core/src/app/link.cpp` | App::Link CopyOnChange `documentObjectUpdates` 参考通道。 |
| assembly | `cad-core/src/assembly`、`cad-core/src/runtime/capability_contract.cpp` | request-local assembly solver / marker / writeback contract。 |
| part | `cad-core/src/part/part_sweep.cpp`、`part_filling.cpp`、`part_geomplate.cpp`、`part_loft.cpp`、`part_project_on_surface.cpp` | Part Workbench product-contract、narrowed evidence 和 current comparison target。 |
| fixtures/tests | `cad-core/fixtures/*`、`cad-core/tests/*` | 只有 S6 证明 implementation row 时才新增或扩展。 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-29-16-27-【已实现】C12-M1工作步骤总入口.md` | goal 队列索引。 |
| S0 live 基线 | `工作步骤细分/6-29-16-28-【已实现】C12-M1-S0-live能力基线与候选声明口径冻结.md` | 已冻结 C11 队列、capability JSON 和声明边界。 |
| S1 source candidates | `工作步骤细分/6-29-16-29-【已实现】C12-M1-S1-FreeCAD源码与capability候选矩阵.md` | 已复核 FreeCAD / cad-core source authority 并关闭 `C12M1-BLOCKER-101`。 |
| S2 scope review | `工作步骤细分/6-29-16-30-【已实现】C12-M1-S2-范围准入与blocker矩阵.md` | 已把候选行路由到 active gap、representative subset、narrowed evidence、non-goal 或 S6-only implementation placeholder。 |
| S3 CopyOnChange audit | `工作步骤细分/6-29-16-31-【已实现】C12-M1-S3-CopyOnChange剩余gap复审.md` | 已复核唯一 active remaining gap 并关闭为 retained known gap / oracle blocked。 |
| S4 representative audit | `工作步骤细分/6-29-16-32-【已实现】C12-M1-S4-代表子集与产品边界候选复审.md` | 已审计 Assembly / representative subset，关闭为 no-code product-boundary retained。 |
| S5 historical audit | `工作步骤细分/6-29-16-33-【已实现】C12-M1-S5-历史non-parity与narrowed证据复审.md` | 已审计 Part Workbench narrowed / non-parity evidence，关闭为 no-code retained / probe-only 输入。 |
| S6 release gate | `工作步骤细分/6-29-16-34-【已实现】C12-M1-S6-NextBatch发布闸门与代码授权.md` | 已发布 `no_code_backlog_gate`，确认本轮无代码落点且无 implementation candidate。 |
| source candidates | `矩阵/c12m1_capability_candidate_source_candidates.tsv` | source authority seed。 |
| scope review | `矩阵/c12m1_capability_candidate_scope_review_matrix.tsv` | scope / owner step / route / close condition。 |
| blocker queue | `矩阵/c12m1_capability_candidate_blocker_queue.tsv` | S0-S6 blocker 和关闭条件。 |
| non-goal registry | `矩阵/c12m1_capability_candidate_non_goal_registry.tsv` | 禁止声明和 reopen condition。 |
| backend gap classification | `矩阵/c12m1_capability_candidate_backend_gap_classification.tsv` | next-batch 分类。 |
| validation matrix | `矩阵/c12m1_capability_candidate_validation_matrix.tsv` | 验收命令索引。 |

当前 S0、S1、S2、S3、S4、S5、S6 均已实现；矩阵已关闭 S0 baseline、S1 source authority blocker、S2 scope admission blocker、S3 CopyOnChange oracle/product gate、S4 Assembly representative/product gate、S5 Part Workbench historical narrowed gate 与 S6 next-batch publication gate。CopyOnChange 保持 retained known gap / oracle blocked：没有 stable native copied-object expected、没有产品批准的 SubShapeBinder request-local DTO，也没有 current cad-core mismatch。Assembly representative solver 仅保留 fallback metadata；subshape marker placement 与 placement writeback 已是 expected-backed current-covered request-local subset，没有新的产品批准 expected/current mismatch。Sweep、Filling、GeomPlate、Loft、ProjectOnSurface 均没有 stable expected/current mismatch，继续作为 no-code retained non-parity、probe-only 或 native-hidden evidence。C12-M1 最终 action 是 `no_code_backlog_gate`，本轮无 C++ implementation row，`C12M1-CAT-005` 仍为 `none_s2_placeholder`。
