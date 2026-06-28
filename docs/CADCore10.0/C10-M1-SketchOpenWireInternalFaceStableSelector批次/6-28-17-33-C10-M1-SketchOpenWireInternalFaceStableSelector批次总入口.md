# C10-M1 Sketch OpenWire InternalFace StableSelector 批次总入口

本文是 `docs/CADCore10.0` 下的 C10-M1 实施主线。它承接 C9-M5 queue-empty 后的 live 状态，专门处理 P5b Sketch `InternalShape` 的下一批可推进语义：近切线 / 重合边 / 复杂 open-wire FreeCAD oracle、WireJoiner history ledger、以及 `Profile.StableSubList=InternalFaceN` without `ReferenceShadow` 的 stable selector。

## 主线目标

- 从 live capability 和 P5b 方案出发，确认 C10-M1 不重开 CopyOnChange、Assembly 或 Part surface 已关闭线。
- 复核 FreeCAD `SketchObject::buildInternals()`、`FaceMakerBuildFace`、`WireJoiner::getOpenWires()` 和 PartDesign profile consumer 调用链。
- 用 FreeCAD oracle 固定近切线、重合边和复杂 open-wire case，再决定是否进入 `cad-core` C++ 实现。
- 对不可证明的一对多 open-wire history 保持 stable diagnostic，不做输出端猜测。
- 若证据充分，在 S6 实现 `Profile.StableSubList=InternalFaceN` without `ReferenceShadow` 的 ElementMap-backed stable selector。

## 当前基线

- S0 live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=382539f170`（`382539f170 docs: 关闭 C9-M5 S6 发布闸门`）。
- S0 起始 `git -c core.quotepath=false status --short -uall` 仅显示 C10-M1 seed 文档、矩阵和 `docs/CADCore10.0/README.md` 未跟踪；无 `cad-core/src`、fixtures、expected 或 tests 改动。
- S1 source/current 审计执行基线：`HEAD=3493d948f5`（`3493d948f5 docs: 修正 C10-M1 S1 cad-core 路径口径`），起始工作区干净；S1 未运行 FreeCADCmd，未新增 fixture / expected / tests，未修改 C++。
- C9-M5 `工作步骤细分` 队列输出只有 Markdown 表头；CopyOnChange 保持 retained known gap，不进入 C10-M1。
- P5b 已支持 `InternalFaceN` 作为 explicit `Profile.SubList`、ReferenceShadow-backed recovery 和 recoverable WireJoiner 子集；without `ReferenceShadow` 的 stable selector 仍需 S5 重新准入。

## 状态词典

| 状态词 | C10-M1 口径 |
| --- | --- |
| `baseline_frozen_s0` | 只表示 S0 已冻结 live 起点、允许声明和 forbidden claims。 |
| `native_oracle_required` | 需要 FreeCAD oracle 或明确 native blocker，不能从 current 输出倒推 expected。 |
| `backend_gap_candidate` | 只有 S3-S5 证明 oracle / current mismatch / 产品边界后，S6 才能转成实现任务。 |
| `diagnostic_retained` | 保留稳定诊断或 known gap，不写 supported，不做输出端猜测。 |
| `diagnostic_non_goal` | 本包明确排除，除非另开包或修改上游范围文档。 |
| `release_gate` | S6 对已证明的实现或 no-code retained diagnostic 做发布收口。 |

## 证明链条

```text
C9-M5 queue empty
  -> P5b next-step source authority
  -> source candidates / scope review / blocker queue
  -> near-tangent and coincident oracle
  -> complex open-wire and WireJoiner ledger review
  -> InternalFace stable selector product boundary
  -> S6 C++ implementation or no-code retained diagnostic
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| Sketch internal build | `/home/user/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::buildInternals()` | 先调用 `makeElementFace(..., "Part::FaceMakerBuildFace")`，再调用 `WireJoiner::getOpenWires(openWires, "SKF")` 合并 open wires。 |
| FaceMaker concrete producer | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp::FaceMakerBuildFace::Build_Essence()` | `splitSelfIntersecting()` / `splitAtIntersections()` / `BOPAlgo_BuilderFace` 决定 bounded faces 与 split edge evidence。 |
| FaceMaker post-build history | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/FaceMaker.cpp::FaceMaker::Build()` / `FaceMaker::postBuild()` | `myPreSplitHistory` 和 post-build history 是 InternalShape producer evidence 来源。 |
| WireJoiner open wires | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::build()` / `getOpenWires()` | `EdgeInfo`、`WireInfo`、tight bound、split / done / openWireCompound 和 `aHistory` 决定 open-wire export 与 history。 |
| Profile consumer | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp::ProfileBased::getTopoShapeVerifiedFace()` | PartDesign profile 需要明确的 closed face；`InternalFaceN` selection 不能退回 raw `FaceN` 或 open wire。 |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| Sketch executor | `cad-core/src/sketcher/sketch_object.cpp` | 执行 Sketch 顺序、发布 `InternalShape`、subshape 和 metadata。 |
| Internal builder | `cad-core/src/sketcher/sketch_internal_builder.cpp` | 组合 FaceMaker bounded result 与 WireJoiner open-wire result，保持 `profileShape` 与 `internalShape` 分离。 |
| FaceMaker | `cad-core/src/part/face_maker.cpp` | bounded split、pre-split / splitter producer evidence、summary diagnostics。 |
| WireJoiner | `cad-core/src/part/wire_joiner.cpp` | EdgeInfo / WireInfo 账本、openWireCompound export、history events、noOriginal 过滤。 |
| History publisher | `cad-core/src/part/internal_shape_history_publisher.cpp` | 把 FaceMaker / WireJoiner evidence 写入 `NamedShape`、mapper events 和 diagnostics。 |
| Stable selector | `cad-core/src/part_design/profile_resolver.cpp` | 解析 `Profile.SubList` / `StableSubList`，选择 InternalFace profile 或发出稳定诊断。 |
| Topo naming | `cad-core/src/app/element_map.cpp`、`cad-core/include/cad_core/part/topo_shape.h`、`cad-core/src/part/topo_shape.cpp` | ElementMap alias、split / deleted / generated history 和 stable subname resolution。 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 包 README | `README.md` | 本包定位、当前状态和验收命令。 |
| 方案 | `6-28-17-33-C10-M1-SketchOpenWireInternalFaceStableSelector批次方案.md` | C10-M1 实施策略与 S0-S6 拆分。 |
| 工作步骤总入口 | `工作步骤细分/6-28-17-34-【已实现】C10-M1工作步骤总入口.md` | S0-S6 队列索引。 |
| S0 | `工作步骤细分/6-28-17-35-【已实现】C10-M1-S0-live基线与声明口径冻结.md` | 已冻结 live baseline、allowed claim、forbidden claim 和状态词典。 |
| S1 | `工作步骤细分/6-28-17-36-【已实现】C10-M1-S1-FreeCAD源码与current覆盖候选矩阵.md` | 已复核 FreeCAD source authority 和 current cad-core coverage。 |
| S2 | `工作步骤细分/6-28-17-37-C10-M1-S2-范围准入与blocker矩阵.md` | 对 oracle、implementation、diagnostic 和 non-goal 做路由。 |
| S3 | `工作步骤细分/6-28-17-38-C10-M1-S3-近切线重合边FreeCADOracle专项复审.md` | 采集 / 复核 near-tangent、coincident-edge oracle。 |
| S4 | `工作步骤细分/6-28-17-39-C10-M1-S4-复杂open-wire与WireJoiner账本专项复审.md` | 复核复杂 open-wire 与 WireJoiner history ledger。 |
| S5 | `工作步骤细分/6-28-17-40-C10-M1-S5-InternalFaceStableSelector与reference更新专项复审.md` | 裁决 InternalFace stable selector 与 reference update contract。 |
| S6 | `工作步骤细分/6-28-17-41-C10-M1-S6-Oracle实现与发布闸门.md` | 按 S3-S5 evidence 实现或发布 retained diagnostic。 |
| source candidates | `矩阵/c10m1_sketch_openwire_source_candidates.tsv` | FreeCAD / cad-core source authority。 |
| scope review | `矩阵/c10m1_sketch_openwire_scope_review_matrix.tsv` | scope 状态、owner step、route。 |
| blocker queue | `矩阵/c10m1_sketch_openwire_blocker_queue.tsv` | S0-S6 blocker 闭环。 |
| non-goal registry | `矩阵/c10m1_sketch_openwire_non_goal_registry.tsv` | forbidden claims 与 reopen condition。 |
| backend gap classification | `矩阵/c10m1_sketch_openwire_backend_gap_classification.tsv` | oracle / backendGap / releaseGate 分类。 |
| validation matrix | `矩阵/c10m1_sketch_openwire_validation_matrix.tsv` | 分层验收命令。 |

当前工作步骤总入口索引、S0、S1 标为 `【已实现】`；S2-S6 仍是待执行状态。S1 已关闭 source-authority blocker，但 near-tangent、complex open-wire 和 without-ReferenceShadow stable selector 仍只是后续复审候选，不是 supported 发布结论。
