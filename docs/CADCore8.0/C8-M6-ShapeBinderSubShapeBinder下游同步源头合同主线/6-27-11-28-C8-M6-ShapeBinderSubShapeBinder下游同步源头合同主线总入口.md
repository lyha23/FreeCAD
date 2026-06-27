# C8-M6 ShapeBinder/SubShapeBinder 下游同步源头合同主线总入口

本文是 `docs/CADCore8.0` 下的 C8-M6 实施主线。它只在 FreeCAD 仓库内发布 ShapeBinder/SubShapeBinder 下游同步源头合同，不进入下游仓库实现。

## 主线目标

- 冻结 C8-M5 后的 ShapeBinder/SubShapeBinder live 合同基线。
- 把 C8-M1/C8-M2/C8-M5 的 TypeId、capability、diagnostic、fixture expected 和 request-local 输出合同合并成新的 C8-M6 source package。
- 明确哪些合同可以被下游同步，哪些仍是 `known_gap` / `oracle_blocked` / non-goal。
- 给后续下游实现提供可验证清单，但本包不修改下游仓库、不写 Rust、不写前端。

## 当前基线

- 仓库：`/home/user/Chili3DProject/FreeCAD`
- S0 live 基线提交：`9361ddc83a`（`docs: 新增 C8-M6 下游同步源头合同包`）
- S0 开始工作区干净；C8-M1 到 C8-M5 工作步骤队列均为空，C8-M6 队列首项为 S0。
- C8-M5 阶段回归已恢复；C8-M1 expected fixture gate 和 stage regression 在 C8-M5 S6 通过。当前 expected 不再要求无输入 `Body.BaseFeature` 时合成 `BodyBaseFeature`，`SubShapeBinder Support` self-link 诊断为 `cycle_rejected_by_property_link`。
- S1 已完成源头合同与能力面复核：所有 `C8M6-SRC-101..403` source candidate 都已从 C8-M5 后 live 状态回写；`C8M6-SCOPE-101..104` 均为 `sync_required_source_contract`；CopyOnChange full temporary-document cache 继续保留在 `known_gap_retained`，未升级为 supported。
- S2 已完成同步范围准入与 non-goal 路由；S3 已完成 TypeId 与 DocumentGraph 合同复审。TypeIds、`DocumentObject graph`、property-link 字段、request-local update channels、Body replay no-synthesis 和 `ReferenceShadow.brep` 单 subshape snapshot 边界均已回写为 S3 源头合同。
- S4 已完成 fixture expected 与 diagnostics 合同复审。12 个 C8-M1 fixture / expected 行已逐行发布为 S4 合同；`shape-binder-subshape-binder-element-map-namedshape-body-replay` 采用 C8-M5 refreshed expected，不要求无 input `Body.BaseFeature` 时出现 `BodyBaseFeature`；`subshape-binder-setlinks-normalization-diagnostics` 采用 setter-level `cycle_rejected_by_property_link`，generic graph cycle 保持 `cycle_dependency`；CopyOnChange fixture 只证明 request-local property-state 和 diagnostic 边界。
- S5 已完成 capability 与前端消费边界发布。`part_design.shape_binder` 保持 request-local supported 且无 remaining gap；`part_design.sub_shape_binder` 保持 request-local supported 并只保留 `copy_on_change_full_temporary_document_cache` known gap；下游前端可消费 capability、diagnostics、fixture seeds、request-local mesh / subshape / full subname / reference update evidence、`documentObjectUpdates` 与 `elementReferenceUpdates` 建议，不得持久消费 full BREP、TopoDS_Shape、NamedShape / ElementMap 原始内核对象、temporary-document cache 或 request 结束后的 shape cache。
- current capability 中 `part_design.shape_binder.remaining_gaps=[]`，`part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`；该 gap 保持 C8-M2 `known_gap` / `oracle_blocked`，不被本包升级为 supported。

## 证明链条

```text
C8-M5 live baseline
  -> C8-M2 downstream contract rebase
  -> ShapeBinder/SubShapeBinder source and capability scan
  -> TypeId / DocumentGraph / request response contract review
  -> fixture expected / diagnostics / output contract review
  -> capability and frontend-consumer boundary publication
  -> release gate and downstream handoff checklist
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| ShapeBinder 引用构形 | `~/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp::ShapeBinder::updatedShape()` | 从引用对象和 subname 构造 binder shape，并带入 placement / TraceSupport 语义。 |
| SubShapeBinder 支持项更新 | `~/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::update()` | 处理 whole / subshape support、BindMode、MakeFace、Offset2D、Fuse、Refine 与 CopyOnChange 分支。 |
| SubShapeBinder 链接 setter | `~/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::setLinks()` | 对 self link / cyclic support 做 setter-level 拒绝，C8-M5 对齐 `cycle_rejected_by_property_link`。 |
| Body replay | `~/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp::Body::onChanged()` | `BaseFeature` 存在时才创建 `PartDesign::FeatureBase`；C8-M5 已裁决无输入 `Body.BaseFeature` 时不合成 `BodyBaseFeature`。 |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| runtime capability | `cad-core/src/runtime/capability_contract.cpp` | 发布 `part_design.shape_binder` / `part_design.sub_shape_binder` status、covered、remaining_gaps 和 diagnostics vocabulary。 |
| PartDesign executor | `cad-core/src/part_design/feature_shape_binder.cpp` | ShapeBinder/SubShapeBinder request-local 语义、ElementMap / NamedShape producer evidence 和 binder output。 |
| Body replay | `cad-core/src/part_design/body.cpp` | 只在 input graph 显式具备 `Body.BaseFeature` 时产出 request-local `documentObjectUpdates`。 |
| reference lifecycle | `cad-core/src/runtime/reference_lifecycle.cpp` | 处理 `SubShapeBinder Support` self-link setter-cycle 诊断，不在 adapter 改字符串。 |
| fixtures/tests | `cad-core/fixtures/c8m1`、`cad-core/tests/test_c8_shapebinder.py`、`cad-core/tests/test_expected_fixtures.py` | 作为下游黑盒合同和阶段回归证据。 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-27-11-29-【已实现】C8-M6工作步骤总入口.md` | S0-S6 队列索引。 |
| S0 | `工作步骤细分/6-27-11-30-【已实现】C8-M6-S0-live基线与同步声明冻结.md` | 已冻结 C8-M5 后的 live 基线和同步声明。 |
| S1 | `工作步骤细分/6-27-11-31-【已实现】C8-M6-S1-源头合同与能力面复核.md` | 已复核 FreeCAD/cad-core source candidates、capability 面、fixture 和 focused test 证据，未发现 unexpected mismatch。 |
| S2 | `工作步骤细分/6-27-11-32-【已实现】C8-M6-S2-同步范围准入与non-goal路由.md` | 已分类 sync required、known gap、non-goal 和 unexpected mismatch。 |
| S3 | `工作步骤细分/6-27-11-33-【已实现】C8-M6-S3-TypeId与DocumentGraph合同复审.md` | 已复核 TypeId、DocumentGraph、request / response DTO 合同。 |
| S4 | `工作步骤细分/6-27-11-34-【已实现】C8-M6-S4-fixtureExpected与diagnostics合同复审.md` | 已复核 C8-M1 expected、C8-M5 drift delta 和 diagnostics vocabulary。 |
| S5 | `工作步骤细分/6-27-11-35-【已实现】C8-M6-S5-capability与前端消费边界发布.md` | 已发布 capability 和前端消费边界。 |
| S6 | `工作步骤细分/6-27-11-36-C8-M6-S6-发布闸门与下游交接清单.md` | 执行发布闸门并形成下游交接清单。 |
| source candidates | `矩阵/c8m6_downstream_sync_source_candidates.tsv` | FreeCAD、cad-core、fixture、测试源头候选。 |
| scope review | `矩阵/c8m6_downstream_sync_scope_review_matrix.tsv` | 同步范围和下一步 owner。 |
| blocker queue | `矩阵/c8m6_downstream_sync_blocker_queue.tsv` | 阻塞项、证据和关闭条件。 |
| contract | `矩阵/c8m6_downstream_sync_contract.tsv` | 下游同步源头合同正文。 |
| fixture contract | `矩阵/c8m6_downstream_sync_fixture_contract_matrix.tsv` | C8-M1 fixture / expected 黑盒合同。 |
| non-goal | `矩阵/c8m6_downstream_sync_non_goal_registry.tsv` | 本包明确不做的内容。 |
| validation | `矩阵/c8m6_downstream_sync_validation_matrix.tsv` | 短跑、阶段复核、发布闸门命令。 |

当前 S0 已完成 live 基线冻结，S1 已完成源头合同与能力面复核，S2 已完成同步范围准入与 non-goal 路由，S3 已完成 TypeId 与 DocumentGraph 合同复审，S4 已完成 fixture expected 与 diagnostics 合同复审，S5 已完成 capability 与前端消费边界发布；队列下一项应为 S6。矩阵中 S5 指定 contract/scope/blocker/validation 行已回写 live evidence，S6 release gate 行仍待执行，不是发布闸门结论。
