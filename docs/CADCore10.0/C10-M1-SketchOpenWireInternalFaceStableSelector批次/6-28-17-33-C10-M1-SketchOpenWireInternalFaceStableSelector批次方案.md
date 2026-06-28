# C10-M1 Sketch OpenWire InternalFace StableSelector 批次方案

## 背景

C9-M5 关闭后，CopyOnChange full temporary document cache 继续保留为 known gap，不再作为默认实现入口。P5b 当前已经把 Sketch internal geometry 从 closed-wire baseline 推进到 `SketchObject::buildInternals()` 主路径，并实现了 FaceMaker / WireJoiner 多个 recoverable 子集。

下一批最有价值的实现面不应再拆成单个 fixture，而应按同一 FreeCAD 调用链批量推进：近切线 / 重合边 / 复杂 open-wire oracle、WireJoiner history ledger、InternalFace stable selector 和 PartDesign profile consumer。

## 原则

- `Sketch.Shape`、`Sketch.InternalShape`、`NamedShape`、`ElementMap` 和 mesh 都是 request-local 计算产物。
- `InternalFaceN` 只能来自 `Sketch.InternalShape` 当前 evidence、ElementMap 或 ReferenceShadow-backed recovery；不得映射到 raw `FaceN`。
- open wire 不得伪造成可拉伸 profile。
- split / merge / deleted 的 internal element map 必须来自 FaceMaker / WireJoiner history 或明确 diagnostic。
- 禁止用 fixture 名、source index、split 顺序、bbox、面积、长度、几何类型或输出排序猜 ownership。

## 最小完整语义批次

| 批次 | 范围 | 预期处理 |
| --- | --- | --- |
| live baseline | C9-M5 queue-empty，P5b next-step evidence | S0 冻结，不重开 CopyOnChange |
| source authority | `SketchObject::buildInternals()`、FaceMaker、WireJoiner、Profile consumer | S1 source candidates |
| near-tangent / coincident | 近切线、重合边、near-overlap bounded / open-wire behavior | S3 native oracle 与 current comparison |
| complex open-wire ledger | branch network、multi-result open wires、one-to-many history ambiguity | S4 WireJoiner ledger 与 stable diagnostics |
| InternalFace stable selector | `Profile.StableSubList=InternalFaceN` without `ReferenceShadow` | S5 product boundary 与 implementation gate |
| code landing | `sketch_internal_builder`、`wire_joiner`、`profile_resolver`、topo history、focused tests | S6 实现或 no-code release gate |

## S0 live 基线与声明口径冻结

S0 已冻结当前起点：`HEAD=382539f170`，C9-M5 队列为空，CopyOnChange retained known gap 不进入本包，P5b 下一步是 Sketch open-wire / InternalFace stable selector。S0 已写清 forbidden claims：不实现 Sketcher solver，不引入持久 shape cache，不把 raw `FaceN`、source index、split order、bbox、面积或输出排序映射成稳定 `InternalFaceN`。

S0 状态词典固定为：`native_oracle_required` 表示先采 FreeCAD oracle；`backend_gap_candidate` 必须等待 S3-S5 证据后由 S6 消费；`diagnostic_retained` 和 `diagnostic_non_goal` 不能写成 supported；`release_gate` 只在 S6 收口。

## S1 FreeCAD 源码与 current 覆盖候选矩阵

S1 已复核 FreeCAD source 和 current cad-core 落点，形成 FreeCAD authority `C10M1-SRC-101..108` 与 current cad-core coverage `C10M1-SRC-201..208`。S1 没有采 oracle、没有改 C++，只把可进入 S2/S3/S4/S5 的 source/current evidence 写入矩阵。

S1 当前结论：`SketchObject::buildInternals()` 的 FaceMakerBuildFace 后接 WireJoiner 路径已在 current `sketch_internal_builder.cpp` 中有对应落点；FaceMaker / WireJoiner producer history 已有 partial publication；`Profile.StableSubList=InternalFaceN` without `ReferenceShadow` 仍必须等 S5 产品边界批准，不能直接写成 supported。

## S2 范围准入与 blocker 矩阵

S2 把 source/current evidence 路由到 `native_oracle_required`、`current_mismatch_candidate`、`backend_gap_candidate`、`diagnostic_retained`、`diagnostic_non_goal` 或 `release_gate`。S2 不允许无 oracle / current mismatch 的 `backend_gap_requires_implementation`。

S2 已完成准入：`C10M1-BLOCKER-201` 关闭为 `closed_s2`；`C10M1-SCOPE-001` 作为 `release_gate` 保留；`C10M1-SCOPE-101` 与 `C10M1-SCOPE-103` 保持 `native_oracle_required`，并明确 oracle 未采集时只能保留 `notCollected`；`C10M1-SCOPE-102` 与 `C10M1-SCOPE-105` 保持 `backend_gap_candidate`，必须等 S3/S5 证据后才可由 S6 消费；`C10M1-SCOPE-104` 保持 `diagnostic_retained`；`C10M1-NG-001..006` 统一为 `diagnostic_non_goal`。

## S3 近切线重合边 FreeCAD oracle 专项复审

S3 负责采集或复核 near-tangent / coincident-edge fixtures。代表场景包括近切线圆弧 / 线段、重合边闭合 profile、touching open cutter、near-overlap bounded region。若 FreeCADCmd 可采集，应新增 `cad-core/fixtures/c10m1` expected；若不能采集，记录 native blocker，不转成实现任务。

## S4 复杂 open-wire 与 WireJoiner 账本专项复审

S4 负责复杂开放线网、branch open cutter、multi-result openWireCompound、one-source-to-many history ambiguity。目标是确认哪些 case 能由 `EdgeInfo` / `WireInfo` / `aHistory` 产生唯一 ElementMap alias，哪些必须保留 stable diagnostic。

## S5 InternalFace stable selector 与 reference 更新专项复审

S5 根据 S3/S4 evidence 裁决 `Profile.StableSubList=InternalFaceN` without `ReferenceShadow` 是否可以进入 request-local implementation gate。批准条件是：当前 recompute 已经有 `Sketch.InternalShape` `NamedShape` 或 ElementMap 证据，能唯一解析到当前 `InternalFaceN`，并能生成 `elementReferenceUpdates`；缺证据或多解时保持 diagnostic。

## S6 Oracle 实现与发布闸门

S6 是唯一允许代码落地的步骤。实现路径：

- code gate：实现 expected-backed near-tangent / coincident / complex open-wire 子集，或实现 S5 批准的 InternalFace stable selector；补 C++、fixtures、focused tests、capability / docs。
- no-code gate：如果 S3-S5 证据不足，保留 diagnostic / known gap，写清 reopen condition，不改 C++。

## 验收分层

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0
git diff --check
```

实现短跑只在 S6 打开 C++ gate 时执行：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p5_sketch tests.test_p6_topology tests.test_adapters
```

重型收口只在 shared topo history、ReferenceShadow recovery、ElementMap API 或 common profile resolver 行为改变时执行。
