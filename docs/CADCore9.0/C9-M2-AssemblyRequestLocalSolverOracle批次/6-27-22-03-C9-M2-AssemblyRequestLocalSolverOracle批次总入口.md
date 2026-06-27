# C9-M2 Assembly request-local solver oracle 批次总入口

本文是 `docs/CADCore9.0` 下的 C9-M2 实施主线。它承接 C9-M1 队列清空后的 live 状态，聚焦同一 FreeCAD Assembly request-local solver 调用链里的剩余 oracle / implementation gate：fixed-joint bundled `offsetPlc`、已有 custom placement-chain expected 的测试激活、zero Angle fallback native oracle，以及扩面后的 diagnostics / capability 发布边界。

## 主线目标

- 批量采集或激活同一调用链的代表性 evidence，不再只做单个 fixture：`AssemblyObject::handleOneSideOfJoint()` marker placement、`runPreDrag()`、`setNewPlacements()` / `validateNewPlacements()` 和 `makeMbdJointOfType()` zero Angle fallback。
- 为 non-identity bundled `offsetPlc` 建立 native oracle：至少覆盖 object marker、subshape marker、solver writeback 三类 request-local 场景。
- 把 C9-M1 发现但未直接断言的 `assembly-marker-custom-placement-chain-real-solver` expected 接入 focused tests。
- 为 zero Angle fallback 采集 native expected，判断它只是 expected-backed closure 还是 current mismatch。
- 只有 FreeCAD native oracle 与 current `cad-core` 结果同时证明 mismatch 时，才在 S6 打开 C++ 实现 gate。

## 当前基线

- C9-M1 工作步骤队列已清空，最终提交 `d52cd67a19 docs: 关闭 C9-M1 S6 发布闸门`。
- C9-M2 S0 live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=d52cd67a19`（`d52cd67a19 docs: 关闭 C9-M1 S6 发布闸门`）。S0 起始 status 仅包含 `docs/CADCore9.0/README.md` 与本 C9-M2 seed 文档 / 矩阵 / step 文件；本包不接管 cad-core source、fixtures、expected 或 tests 的未提交变更。
- live capability 中 `assembly.ondsel_solver_adapter.status=covered_full`、`subshape_marker_placement.status=covered_representative_subset`、`placement_writeback.status=covered_full`，`assembly.remaining_gaps=[]`。
- C9-M1 保留的 Assembly 证据缺口不是 backendGap：`non_identity_bundled_offsetPlc` 为 oracle candidate / forbidden guessing，`non_assembly_link_subshape_primitive_frame_generalization` 为 diagnostic non-goal，zero Angle fallback 为 known-gap-retained oracle evidence。
- `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache` 仍是 C8 known gap / oracle blocked，不进入 C9-M2。
- S0 冻结声明：C9-M2 只能在 native oracle 或 focused tests 证明 current mismatch 后，才由后续 S6 打开 C++ implementation gate；S0 不采 oracle、不改 expected、不把 oracle-only row 写成 supported / backendGap。
- S2 范围准入已关闭于 `HEAD=87f289aaba`（`87f289aaba docs: 关闭 C9-M2 S1 源码候选矩阵`）：bundled `offsetPlc` object/subshape/writeback 保持 `native_oracle_required` 到 S3；custom placement-chain 为 `expected_activation` 到 S4；zero Angle fallback 为 `native_oracle_required;known_gap_retained` 到 S5；unsupported diagnostics guard 为 `already_covered_review;release_gate` 到 S5；capability/final validation 为 `release_gate` 到 S6；primitive frame generalization、persistent solver state、GUI/session 与 non-source-backed `offsetPlc` guessing 继续作为 forbidden claims。
- S3 bundled `offsetPlc` oracle 已采集于 `HEAD=f500c34407` 之后：object marker、subshape marker、placement writeback 三条 expected 均证明 `offsetPlc=[2,0,0]` 非 identity；当前 cad-core 对 `ComponentC` writeback 仍输出 `[4,0,0]`，native expected 为 `[6,0,2]`，因此 `C9M2-SCOPE-101/102/103` 路由为 `backend_gap_candidate`，交 S6 消费。
- S4 custom placement-chain expected activation 已关闭于 `HEAD=fe1b38727b` 之后：`assembly-marker-custom-placement-chain-real-solver` 已被 focused test 直接断言，测试锁定 `native_marker_oracle` 与 `offset_boundary=identity_offset_for_two_box_assembly_link_fixture`；该 identity boundary 不作为 bundled `offsetPlc` parity 证据。

## 证明链条

```text
C9-M1 no-code closure
  -> FreeCAD bundled offsetPlc / zero Angle source authority
  -> scope review / blocker / nonGoal route
  -> native oracle batch collection
  -> existing custom-chain expected activation
  -> zero Angle fallback oracle comparison
  -> cad-core implementation only for source-backed mismatch
  -> capability / focused tests / release gate
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| bundled part offset | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.h::AssemblyObject::MbDPartData::offsetPlc` | 字段注释为 bundled parts 内部 offset。 |
| fixed-joint bundling | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::getMbDData()` | `bundleFixed` 下把 fixed-joint connected part 复用同一 `ASMTPart`，并记录 `plc.inverse() * plci`。 |
| marker placement | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::handleOneSideOfJoint()` | `getGlobalPlacement(nullptr, ref) * PlacementN` 转 object-global，再以 `getGlobalPlacement(part, ref).inverse()` 转 part-local，最后 `data.offsetPlc * plc`。 |
| solver writeback | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::setNewPlacements()` | solver placement 组合 `getMbdPlacement(mbdPart) * offsetPlc` 后写回。 |
| placement validation | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::validateNewPlacements()` | drag validation 同样比较 `getMbdPlacement(mbdPart) * offsetPlc`。 |
| zero Angle fallback | `/home/user/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | `Angle` 为 0 或 2pi 时返回 `ASMTParallelAxesJoint`。 |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| solver marker | `cad-core/src/assembly/joint_solver.cpp` | `resolveJointMarkerPlacement()`、real Ondsel adapter、Angle joint class mapping、diagnostics。 |
| writeback JSON | `cad-core/src/assembly/assembly_utils.cpp` | `documentObjectUpdates.action=assembly_set_placement`。 |
| display request use | `cad-core/src/assembly/assembly_object.cpp` | 同一 request 内应用 placement update 做 display summary，不持久写回。 |
| capability | `cad-core/src/runtime/capability_contract.cpp` | Assembly solver capability、non-goals、remaining gaps、diagnostics。 |
| tests / fixtures | `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_adapters.py`、`cad-core/fixtures/c3m6` | native expected、focused parity、capability smoke。 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 包 README | `README.md` | 本包定位、当前状态、批次边界和验收分层。 |
| 方案 | `6-27-22-03-C9-M2-AssemblyRequestLocalSolverOracle批次方案.md` | C9-M2 实施策略。 |
| 工作步骤总入口 | `工作步骤细分/6-27-22-04-【已实现】C9-M2工作步骤总入口.md` | S0-S6 队列索引。 |
| S0 | `工作步骤细分/6-27-22-05-【已实现】C9-M2-S0-live基线与声明口径冻结.md` | 已冻结 live baseline、claim 和 forbidden claim。 |
| S1 | `工作步骤细分/6-27-22-06-【已实现】C9-M2-S1-FreeCAD源码与oracle候选矩阵.md` | FreeCAD source authority 与 oracle candidate 复核。 |
| S2 | `工作步骤细分/6-27-22-07-【已实现】C9-M2-S2-范围准入与blocker矩阵.md` | scope / blocker / non-goal / backend gap 路由。 |
| S3 | `工作步骤细分/6-27-22-08-【已实现】C9-M2-S3-bundledOffsetPlcOracle批量采集.md` | bundled `offsetPlc` native oracle 批量采集。 |
| S4 | `工作步骤细分/6-27-22-09-【已实现】C9-M2-S4-customPlacementChain测试激活.md` | 现有 custom placement-chain expected 已接入 focused test，identity offset boundary 已锁定。 |
| S5 | `工作步骤细分/6-27-22-10-C9-M2-S5-zeroAngleFallback与diagnostics复审.md` | zero Angle native oracle 与 diagnostics guard。 |
| S6 | `工作步骤细分/6-27-22-11-C9-M2-S6-Oracle实现与发布闸门.md` | 根据 oracle 结果实现或 release gate。 |
| source candidates | `矩阵/c9m2_assembly_solver_oracle_source_candidates.tsv` | FreeCAD / cad-core source authority。 |
| scope review | `矩阵/c9m2_assembly_solver_oracle_scope_review_matrix.tsv` | scope 状态、owner step、route。 |
| blocker queue | `矩阵/c9m2_assembly_solver_oracle_blocker_queue.tsv` | S0-S6 blocker 闭环。 |
| non-goal registry | `矩阵/c9m2_assembly_solver_oracle_non_goal_registry.tsv` | forbidden claims 与 reopen condition。 |
| backend gap classification | `矩阵/c9m2_assembly_solver_oracle_backend_gap_classification.tsv` | oracle / backendGap / releaseGate 分类。 |
| validation matrix | `矩阵/c9m2_assembly_solver_oracle_validation_matrix.tsv` | 分层验收命令。 |

当前 S0-S4 已关闭；S5-S6 仍是待执行状态。矩阵已经完成 S3 oracle 裁决：bundled `offsetPlc` 三类 case 均为 expected-backed `backend_gap_candidate`，但发布闸门和 C++ 实现仍只由 S6 消费并关闭。S4 仅关闭 custom placement-chain expected activation，不宣称 non-identity bundled offset coverage。
