# 【已实现】Rust 几何内核 C4M6 TopoNamingState 权威对齐重构方案

> 完成时间：2026-07-11（Asia/Shanghai）
> 当前状态：已实现；`StringIDRef`/`ElementMapSnapshot`、typed evidence/projector、FaceMaker source boundary 与 state-backed Compound Link 均已收口。
> 最终裁决：FreeCAD native 与 Rust FFI 的 C4M6 `semanticStatus=green`、`releaseGatePassed=true`；保留的 `exactStatus=red` 均为已登记的 `protocol_divergence`，无未接受语义差异。

## 2026-07-11 完成记录

- Rust 将 `ElementMap` 的 SID 持久化升级为 typed `StringIDRef`/`mapping_sids` round-trip，并拒绝旧的无类型 snapshot；topo-state 只由 request-local typed evidence ledger 投影。
- FaceMaker/Sketch producer 只发布有唯一 source evidence 的终端 mapper 事实；ReferenceShadow 不再把内部 BREP 或调试 ledger 泄漏到 public response。
- `App::Link` 的 `StableSubListSource=topoNamingState` 仅在 resolver、当前选择和 private mapper fact 三者一致时恢复 stable token；冲突 mapper evidence 会拒绝投影。
- C++ native 的矩形面 XTR producer 在 logical prism indexing 后替换临时候选账本，避免 generic mapper 的 `:U` 关系及重复 mapper history 覆盖 native ElementMap 语义。

已执行并通过的最终验证：

- Rust `cargo check -p opencascade --lib`、C4M6 focused unit tests（6）与 `cad-core-ffi` integration tests（36）。
- fresh `--build-ffi --release-gate` 10/10 materialization，Rust FFI parity：9 cases、`semanticStatus=green`、`releaseGatePassed=true`、`unaccepted=0`。
- FreeCAD native parity：9 cases、`semanticStatus=green`、`releaseGatePassed=true`、`unaccepted=0`；FreeCADCmd check 为 `processed=9, skipped=1, failed=0`，strict ledger 验证 9 个 expected fixture。
- `tests.test_rust_ffi_expected_source`、`tests.test_freecad_expected_public_parity` 与 `tests.test_topo_naming_state_response` 共 42 项，以及 3 个 registered contract tests。

## 来源与结论

本方案用于修复 `/Users/li/Chili3DProject/FreeCAD/cad-core/fixtures/c4m6/cad-rs-res` 相比 `/Users/li/Chili3DProject/FreeCAD/cad-core/fixtures/c4m6/expected` 暴露的 Rust 几何内核缺口。实现仓库是 `/Users/li/Chili3DProject/opencascade-rs`，FreeCAD 仓库只提供协议、native expected、ledger 和最终 release gate 权威。

下列三层基线与硬阻断是实施前快照，保留用于说明本次收口的起点；不能把任一单独快照直接当成当前实现结果。

1. **实施前源码基线**：Rust HEAD `de12c53f`，工作树包含 S1–S6 未提交改动；Pad producer ledger、`evidence_ledger.rs`、`topo_state_projection.rs`、`topo_state_reference.rs` 已出现并接入部分调用链。当时 `cargo check -p opencascade --lib` 失败于 `element_map.rs`：`StringIDRef` 与旧 `String` token 混用、snapshot 缺 `mapping_sids` 序列化/恢复，共 5 个编译错误。
2. **最后可运行 dylib 基线**：`target/debug/libcad_core_ffi.dylib` SHA-256 为 `626abeb4ef64ab19e8fef3bc1d84b065a2a7665fc053efc791b055863a8484b3`。FreeCAD `RustFfiActualSource` 对该库的报告 `/private/tmp/c4m6-freecad-audit-current-lib.json` 发现 9 个 native/rejected case，5 green、4 red、732 diffs；它早于当前 `StringIDRef`/snapshot 迁移和 Pad ledger 半迁移，只能证明更早的可运行状态。
3. **最后一次可复验 materialization 基线**：`cad-rs-res/manifest.json` 于 `2026-07-10T04:25:01Z` 生成，绑定旧 dylib/build-input/dirty-state。它完整物化 10 case，Rust focused matrix 为 10/10 pass，但已不匹配当前源码、当前 dylib 和当前 FreeCAD expected/ledger，只能作为历史 artifact。

因此实施顺序曾调整为：先完成 `StringIDRef` 与 `ElementMapSnapshot` 持久化闭环使源码恢复可编译；再完成 S3/S5 的 typed evidence 与 projector；修复 FreeCAD native ledger 闭包；随后重建 fresh dylib、重采 10 case，并用 FreeCAD parity engine 重新冻结权威红灯。S0 的 runner/materialization 基础设施已经存在，但 freshness 结论必须在源码重新可构建且 native ledger 通过后重做。

### 实施前硬阻断：`StringIDRef` 与 `ElementMapSnapshot` 持久化

实施前 `cargo check -p opencascade --lib` 的 5 个错误集中在 `crates/opencascade/src/topo_naming/element_map.rs`，不是可忽略的测试或报告问题：

- child token collector 仍把 `StringIDRef` 与旧 `String` 混入同一 iterator；
- snapshot writer/parser 仍按单一字符串 SID 写读，未覆盖 `mapping_sids`；
- `hex_encode`、child SID parser 和 `ElementMapSnapshot` initializer 尚未完成 `StringIDRef` 迁移。

这同时阻断编译和语义闭环：即使临时修到能编译，若 `mapping_sids`、child map SID 和 `StringIDRef` 的 round-trip 不保持稳定，重建后的 `ElementMap` 仍会改变 canonical identity。修复必须包含 typed round-trip/legacy rejection 测试，并在 fresh build 后重新跑 C4M6；不得把 `String` 强转回去作为兼容桥。

最终仍需按四个深 module seam 收口：

1. `TopoStateAdmission`：统一不可信 state 的请求前校验与 diagnostics-only hard fail。
2. `RecomputeProjection`：统一 public `results`、public shape DTO 与 topo-state-only evidence 的分流。
3. `TopologyEvidenceLedger` / `TopoStateProjector`：从 request-local `NamedShape`、`ElementMap`、child map 和 typed `MapperHistoryEvent` 直接投影 state，不再从 debug DTO 反推。
4. `StateBackedReferenceResolver`：统一普通 mapped name 与 Compound child path 的唯一恢复。

`c4m6` 只是最小完整验收切片，不进入长期 module 名称，不允许按 fixture 名称、JSON 后处理或输出顺序做修复。

本方案与现有文档分工如下：

- `docs/重构/7-10-06-25-【已实现】FreeCADExpectedReleaseGate深模块重构方案.md` 负责 expected/ledger/live comparator、精确 protocol divergence registry 和 release verdict。
- 本方案只负责 Rust 内核语义、C ABI 结果语义、public result/state 投影及 Rust live artifact 的可复现性，不再设计第二套 release policy engine。
- `docs/重构/7-8-00-17-【已实现】StableSubname身份账本语义重构方案.md` 继续约束 `subname`、`fullSubname`、`stableSubname` 和 `identityStatus`。
- `/Users/li/Chili3DProject/opencascade-rs/docs/几何支持/CADCoreRs10.0/RS10-M2.7-TopoStateSourceObjectEvidencePublication闭环主线/7-10-00-47-FreeCAD几何内核第十阶段TopoStateSourceObjectEvidencePublication实现方案.md` 记录此前 Rust evidence-publication 实施批次；它是 S0 复核输入，不取代本方案使用的 live binary 与 FreeCAD expected 权威。

## 2026-07-11 实施前基线（历史记录）

### 仓库与可执行状态

| 项目 | 实施前事实 | 裁决 |
| --- | --- | --- |
| Rust HEAD | `de12c53f`（`chore: 更新 C4M6 S0 新鲜运行报告`） | HEAD 之后存在大量未提交 S1–S6 改动；不能用 HEAD 或旧 receipt 代表当前源码 |
| FreeCAD HEAD | `48fb1d2384`（`fix: 将 compound 子映射冲突归入 Part 账本`） | C4M6 expected、collector、ledger validator 与 C3M2 BREP 相关改动仍在工作树；native authority 尚未重新闭包 |
| 当前 Rust 编译 | `cargo check -p opencascade --lib` 失败；`element_map.rs` 有 5 个 `StringIDRef`/snapshot 编译错误 | 必须先完成 SID 类型统一、mapping/child SID 序列化和 snapshot restore，不能生成 fresh dylib |
| 最后可运行 dylib | SHA-256 `626abeb4ef64ab19e8fef3bc1d84b065a2a7665fc053efc791b055863a8484b3` | 可用于回看迁移前语义，不证明当前源码 |
| Rust focused report | 10 discovered / 10 executed / 10 pass，materialized 10 artifacts | runner、catalog、receipt 与原子 materialization 已工作；profile 有 blind spots，不能作为 release green |
| FreeCAD Rust-FFI report | 旧报告为 9 cases、5 passed、4 red、732 diffs、728 unaccepted | 该报告绑定旧 dylib；当前源码和 native expected 已变化，必须重新生成 |
| FreeCAD native ledger | collector `processed=9, skipped=1, failed=0`；strict validator `validated=9, failed=1, errors=57` | Body/Pad identity 与 hash 闭包当前不成立；不得宣称 FreeCAD authority green |

### S0–S6 实施前状态总表

| 批次 | 当前状态 | 已落地事实 | 仍需完成/重验 |
| --- | --- | --- | --- |
| S0 | 工具完成，freshness/authority 失效 | 10-case catalog、receipt、materialization 与 protocol role 已落地 | 必须等 Rust 编译和 FreeCAD strict ledger 都通过后重采 artifact |
| S1 | 基本完成，待回归 | admission、foreign owner、encoding gate、status-0 exact payload、runtime OCCT accessor 已存在 | 当前源码恢复后重跑 admission/FFI 全套 |
| S2 | 部分完成 | target-only projection、summary、mesh picking fields 已存在 | public subshape/result DTO 仍需以新 fresh parity 报告为准收敛 |
| S3 | **进行中，已越过 Pad 初始断点但被 SID 持久化阻断** | evidence ledger、Pad producer recording、topo-state projector/reference module 已创建 | 完成 `StringIDRef` 类型统一、snapshot round-trip，并删除 response/debug fallback |
| S4 | 部分完成 | Compound tuple evidence、正式 `topo_state_reference.rs`、no-change 抑制已存在 | 需用新报告证明 Compound、ReferenceShadow 完整 keyed parity |
| S5 | 部分完成 | `MapperHistoryProjection` 已迁入 `topo_state_projection.rs`，typed events 已接入 | `response.rs` 仍有旧 helper/DTO 反推路径，需删除并测试单一事实源 |
| S6 | 工具完成，release 未完成 | Rust FFI adapter、runner、manifest 机制已存在 | 旧报告过期；必须 fresh build、10-case materialization、Rust FFI gate、native ledger 和跨阶段回归全部闭合 |

### 最后可运行 dylib 的历史差异（已过期）

报告：`/private/tmp/c4m6-freecad-audit-current-lib.json`。

| category | diffs |
| --- | ---: |
| `topoNamingState.mapperHistory` | 470 |
| `results.subshapes` | 244 |
| `results` | 10 |
| `json` | 6 |
| `topoNamingState.elementMap` | 1 |
| `geometry.numeric` | 1 |
| diagnostics / childElementMaps / objects / topo-state subshapes | 0 |

| case | verdict | diffs | 实施前解释 |
| --- | --- | ---: | --- |
| `topo-state-first-recompute-empty` | red | 53 | public result/subshape DTO 仍未精确对齐 |
| `topo-state-body-tip-stable-recovery` | red | 392 | Pad/Body provenance 与 mapper history 是主差异 |
| `topo-state-link-compound-child-maps` | red | 37 | BackendGap/update 已消失，但 result identity、terminal/ambiguous history 尚未闭合 |
| `topo-state-reference-shadow-brep` | red | 250 | public subshape identity、Pad/Sketch ledger 与 writeback 形态尚未闭合 |
| 5 个 rejected native case | green | 0 | S1 admission/FFI exact envelope 已由最后可运行库证明 |

`accepted=4` 是旧 comparator 已批准的精确 protocol divergence 数，不代表 4 个 accepted native case 通过。该表只保留用于解释前一轮工作，不得用于当前实现裁决；当前源码没有新的可运行 dylib。

### FreeCAD authority 实施前状态：ledger 闭包失败

实施前 collector discovery 本身没有失败，但 strict ledger validator 已发现 Body Tip expected/ledger 不一致：

- collector check：`processed=9, skipped=1, failed=0`，唯一 skip 为 MapperHistory `protocol_only`；
- strict ledger validator：`validated=9, failed=1, errors=57`；失败 fixture 为 `topo-state-body-tip-stable-recovery`；包含 `expectedPayloadHash`、`topoNamingStateHash`、Body/Pad 26 个 subshape identity 和 round-trip hash 不一致。
- 当前 FreeCAD 工作树还包含 C3M2 BREP snapshot、collector、ledger validator 和 C4M6 expected 改动；这些改动必须在同一 native collector 运行中重新闭包，不能手工改 hash 或 expected。

因此当时 Rust parity 尚无有效 authority verdict。需要先修复 FreeCAD native ledger 闭包，再以同一 expected/ledger 基线重建 Rust artifact；不得把旧 732-diff 报告或旧 10/10 focused report 继续作为当前结论。

## 权威层级与 artifact provenance

### 9 个 native/rejected case

以下 9 个 case 必须同时存在同名 `.freecad.json` 与 `.freecad.ledger.json`，两者成对才构成权威：

- `topo-state-first-recompute-empty`
- `topo-state-body-tip-stable-recovery`
- `topo-state-link-compound-child-maps`
- `topo-state-reference-shadow-brep`
- `topo-state-schema-incompatible`
- `topo-state-producer-incompatible`
- `topo-state-document-hash-mismatch`
- `topo-state-object-hash-mismatch`
- `topo-state-foreign-object-owner`

其中前 4 个是 accepted native case，后 5 个是 rejected native case。仅“文件成对存在”不算权威闭包：accepted ledger 还必须满足 `expectedPayloadHash`、`topoNamingStateHash` 匹配，所有 published/dropped object 都有 projection 解释，`coverage.uncoveredInputReferenceIds=[]`，required reference 全部进入 terminal event，且 `roundTrip.status=passed`。rejected ledger 不要求新 `topoNamingState`，但 `rejection.diagnosticCodes` 必须覆盖 expected diagnostics。

### 1 个 protocol-only case

`topo-state-mapper-history-events` 的权威是：

```text
cad-core/fixtures/c4m6/expected/
  topo-state-mapper-history-events.expeted.json
```

它在 `cad-core/tools/freecad_expected_parity/fixture_roles.v1.json` 中的 role 是 `protocol_only`。FreeCAD Python 当前不能导出 producer mapper history，`CadCore::TopoNamingStateProbe` 又会构造 synthetic helper shape，因此这个 case 不能伪装成 native `.freecad.json + ledger`。

当前 focused contract 强校验它的 `diagnostics` 与 `topoNamingState`。`.expeted.json` 中的 `results=[]` 不是 native geometry authority；现有 focused test 明确允许实际响应包含 `HistoryProbe` result。除非先修改 fixture role、协议合同和 focused test，否则“多一个 HistoryProbe result”不属于本轮强制语义修复项。

### `cad-rs-res` 的地位

`cad-rs-res/*.cad-rs.json` 是 Rust FFI 运行证据，不是 expected。外层：

```text
ffiStatus + payload/error + library/build provenance
```

只用于证明调用了哪一个 binary。比较规则是：

- `ffiStatus == 0`：解包 `payload`，交给既有 parity engine。
- `ffiStatus != 0`：不是成功 public protocol payload，不能通过 expected parity，也不能从 `error.diagnostic.details.document_graph_code` 抽出内层 code 后伪装成协议通过；随后再按 invalid input、execution error 或 panic 分类。
- Rust 输出不得生成、回写、修改或反推任何 `.freecad.json`、`.freecad.ledger.json` 或 `.expeted.json`。

权威原则继续以以下文档为准：

- `docs/接口规定/7-8-11-08-topoNamingState客户端携带状态接口方案.md`
- `docs/框架/7-9-15-53-FreeCADCmd权威账本与topoNamingState裁剪原则.md`
- `docs/工具规定/7-9-16-55-FreeCADCmdExpectedLedger工具规定.md`

## 初始旧 dylib 基线（历史追溯，已被当前基线取代）

本节保留方案初稿使用的旧 dylib characterization，便于解释任务来源；它已经被上面的 2026-07-10 三层基线取代，不得继续据此判断当前实现。旧 dylib 的 10 个 raw JSON 均不与 expected 字节相同。需要拆成 stale binary、真实语义缺口和已批准 transport divergence 三类，不能把 raw diff 数量直接当实现任务数量。

| case / lane | 旧 dylib 事实 | 权威要求 | 当前判断 |
| --- | --- | --- | --- |
| first recompute | Box 几何与 state 核心值存在；result 用 `metadata/namedShapeDebug/mesh.summary`，缺 public `bbox/volume/topology_counts` 形态 | result 为 Box public summary；cad-core 只额外保留批准的 display mesh | public result projection 缺口 |
| schema / producer / documentHash / objectHash | 内层 diagnostic code 正确，但 FFI 返回 `status=2`、`payload=null`、`document_graph_execution_error` | `status=0` 的 diagnostics-only public payload | Admission / FFI 语义缺口 |
| foreign owner | `ForeignBox` 被忽略，正常返回 Box、新 state、无 diagnostic | `topo_state_object_owner_incompatible` hard fail | 真缺口 |
| Body Tip | visible results 为 `Body, Pad`；Body 26/26/1，但 history 112；Pad 26/64/history 123；Sketch 0/0/0 | result 只含 Body；Body 26/26/1/history 0；Pad 26/26/history 0；Sketch 1/1/history 4 | 多数 evidence 差异需先用新 binary 复核；visible result/history seam 仍需修 |
| ReferenceShadow | visible results 为 `ProbeSketch, Body`；writeback 核心存在；Pad/Sketch state 为空；Body history 112 | result 只含 ProbeSketch；state 为 Body/Pad/ProbeSketch/Sketch；完整 writeback；无 BREP 泄漏 | 多数 evidence 差异需先复核；visible result/history seam 仍需修 |
| Compound child maps | Compound 53 subshapes、3 child maps、26 history 基本存在；ChildBoxA/B 各缺 26 current-only subshapes；额外 `topo_state_backend_gap` 与 backendGap writeback | result 只含 CompoundLink；ChildBoxA/B 各 26 current-only subshapes、entries 空；无 diagnostic/update | child evidence 先复核；child-path resolver 真缺口 |
| MapperHistory | 3 entries、2 subshapes、7 events 已存在；缺 4 条 diagnostics | protocol-only 合同要求 4 diagnostics 与完整 state | diagnostic projection 真缺口；result 不作 native parity 裁决 |

跨 case 还存在这些 public DTO 差异：

- Rust `elementMapVersion=opencascade-rs.element-map.v1`，权威为 `cad-core.element-map.v1`。
- Rust 输出 `producer.occtVersion=fixture-occt-unspecified`，权威响应把 fixture placeholder 规范化为当前 kernel version `7.8.1`。
- Rust producer 额外带 `backend/cadRsVersion`；本轮按 FreeCAD public contract 删除这些自行追加字段，不能靠 comparator 忽略 producer 差异。未来若需新增必须单独修改 transport contract 与精确 registry，不在本方案内偷渡。
- Rust `ObjectResult` 把 `bbox/volume` 放在 `metadata`，没有顶层 `topology_counts`。
- Rust mesh 缺 `edgeSegments/vertexPoints`，多 `summary`；cad-core public mesh 的 C4M6 key 集合是 `vertices/normals/indices/faceIds/edgeSegments/vertexPoints`。
- Rust result subshape 的 `id/subname/fullSubname/stableSubname/ShadowSub/ReferenceShadow` 形态未统一对齐 cad-core public DTO。
- Rust 默认发布 `namedShapeDebug`；该内部账本不得成为 production public state 的反向数据源。

## FreeCAD、CAD Core 与 Rust 调用链映射

| lane | FreeCAD / CAD Core 权威调用顺序 | Rust 当前 seam | 必须保持的不变量 |
| --- | --- | --- | --- |
| request admission | `runtime::recompute()` → `topoNamingStateRequestFailureJson()` → 通过后才 `recomputeContext()` | `document_graph/topo_state.rs::validate_request_topo_state()` → `recompute.rs::recompute_document()` | 校验失败不进入 graph analysis、feature executor、mesh 或 state exporter |
| C ABI hard fail | `c_api.cpp::recomputeJsonEntrypoint()` → `cad_core::recompute()` → `makeJsonResult()` | `cad-core-ffi/src/lib.rs::recompute_entrypoint()` / `recompute_error_to_ffi()` | 协议拒绝是 status 0 public payload；真正 malformed request/执行失败才是 FFI error |
| public result | `recomputeResultJson()` 只遍历 `document.targets` → `responseSubshapes()` → `responseMesh()` → `appendObjectSummaryResultFields()` | `recompute.rs` 多个 `append_*_result_like_cad_core()` → `response.rs::export_result()` | visible result 与 state evidence object set 分离；public shape DTO 一处生成 |
| mesh/subshape | `part/shape_exporter.cpp::meshForShape()` → `edgeSegmentsForShape()` / `vertexPointsForShape()` | `response.rs::export_shape_output_like_freecad()` | display/picking ID 必须引用同一 result 的 current subshapes |
| Body Tip | `PartDesign::Body::execute()` → `Tip.Shape.getShape()` → transform → `Body.Shape.setValue()` | `part_design/body.rs::BodyExecutor::execute()`；response 层另造 Body/Tip map | Body 与 Pad 共享 request-local producer ledger，但只有 target 进入 visible results |
| Sketch internal | `SketchObject::buildShape()` → `buildInternals()` → `InternalShape.setValue()`；`convertSubName()` | `sketcher/sketch_object.rs` + response 层 `topo_state_sketch_object_from_internal_shape_like_cad_core()` | `InternalFace1` 只能来自本次 InternalShape/FaceMaker evidence |
| Compound child map | `Compound::execute()` → `TopoShape::makeElementCompound()` → `mapSubElement()` / `createChildMap()`；Link `extensionGetSubObject()` → `getTrueLinkedObject()` → `checkGeoElementMap()` | `part/topo_shape_expansion.rs`、`app/link.rs`、`document_graph/topo_state.rs` | owner/key/pathPrefix/childObject/target 必须共同验证；不能靠 path 字符串猜 |
| reference update | `GeoFeature::updateElementReference()` → `PropertyLinkSubList::updateElementReference()` → `_updateElementReference()` → `GeoFeature::resolveElement()` / `searchElementCache()` | `app/geo_feature.rs::update_element_references()` 与 topo-state resolver | 只有唯一恢复且值发生变化时写回；BREP 只作单个旧 subshape evidence |
| MapperHistory | `ShapeMapper::populate()` / `insert()`；CAD Core `executeTopoNamingStateProbe()` → `addProbeDiagnostics()` | executor 已产生 typed events；`response.rs` 又硬编码 probe state/events | typed event 是单一事实；split/deleted/ambiguous 不进入 terminal map |

主要绝对源码锚点：

- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/runtime/recompute.cpp::recompute()` / `recomputeResultJson()`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/runtime/topo_naming_state.cpp::topoNamingStateRequestFailureJson()` / `objectTopoStateJson()`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/part/shape_exporter.cpp::meshForShape()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp::Body::execute()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp::FeatureExtrude::buildExtrusion()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::buildShape()` / `buildInternals()` / `convertSubName()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FeatureCompound.cpp::Compound::execute()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::createChildMap()` / `makeElementCompound()` / `mapSubElement()`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp::ShapeMapper::populate()` / `insert()`
- `/Users/li/Chili3DProject/FreeCAD/src/App/Link.cpp::LinkBaseExtension::extensionGetSubObject()` / `getTrueLinkedObject()`
- `/Users/li/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp::PropertyLinkBase::_updateElementReference()`
- `/Users/li/Chili3DProject/FreeCAD/src/App/GeoFeature.cpp::GeoFeature::resolveElement()` / `searchElementCache()`

## 根因

以下根因保留为设计追踪，但其“当前状态”必须以上面的 S0–S6 表和 live gate 为准；S1 的两个根因已在最后可运行 dylib 中得到修复证明，S2/S4 只部分收敛，S3/S5 仍是当前主线。

### 顶层 object ownership 没有 admission gate（S1 已实现，待当前源码恢复后复验）

`validate_object_hashes()` 遇到空 `objectHash` 会跳过，`validate_state_backed_property_ownership()` 又只检查 property-local token ownership。`ForeignBox: {}` 因此既不触发 hash mismatch，也不触发 property ownership mismatch。

顶层 `topoNamingState.objects` ownership 是独立完整性检查，不能借 objectHash 或 property resolver 间接实现。

### 协议拒绝被提升成 FFI execution error（S1 已实现，最后 dylib 5/5 rejected green）

`recompute_document()` 通过 `?` 传播 state validation error，`recompute_error_to_ffi()` 又把它包成 `status=2 / document_graph_execution_error`。这混淆了两个不同概念：

- 合法 JSON 请求被协议拒绝：正常 public response。
- 内核执行失败或 adapter 失败：FFI error。

从嵌套 `document_graph_code` 找回原 code 只证明错误信息没有丢失，不能证明 public contract 正确。

更早一层还有 DTO seam 问题：`recompute_document_json()` 先把 raw JSON 反序列化为 `RecomputeRequest`，会让非 object state 或字段类型错误在 admission 前变成 request error；当前 `RecomputeResponse` 又会无条件序列化 `binaryPayloads/documentObjectUpdates`，通用 `Diagnostic` 也无法精确表达 rejection 字段。只改 `recompute_error_to_ffi()` 因此无法得到权威三字段 payload。

### visible results 与 state evidence 被多个 helper 分散决定（S2 主路径已收敛，旧 helper 待删除）

`recompute_document()` 先追加目标结果，又通过 Body Tip、App::Link、App::Part、profile sketch、assembly 等 helper 把依赖追加到 visible results。之后另有 `topo_state_extra_results_like_cad_core()` 生成 state-only result。

同一对象因此可能同时被“前端可见”和“只为 state 提供 evidence”两套路径选中。Body/Pad 与 ProbeSketch/Body 的额外 result 就来自这里。

### state 从 public/debug DTO 反向推导（当前仍未解决）

`export_topo_naming_state_like_cad_core()` 接收 `ObjectResult`，再从 `namedShapeDebug`、display subshapes 和多个特殊分支重建 state。其后果是：

- display DTO 没有某个依赖 result 时，state evidence 也容易变空。
- stable subshape 与 debug ElementMap 两个输入重复生成 terminal entry。
- `history_ledgers` 内部 trace 被扩展成 public `mapperHistory`，Body/Pad 泄漏 112/123 条内部事件。
- `response.rs` 已增长到 5670 行，新增一个 semantic lane 就继续叠一组 `*_like_cad_core()` helper，locality 继续下降。

### Compound child path 没有正式 resolver（S4 行为部分实现，module seam 未形成）

旧 state 已有 top-level resolved entry：

```text
stable token: Compound/ChildBoxA.#f:1;BOX,F
target:       Compound / Child0.Face1
```

当前 resolver 主要支持普通 `IndexedName` 或普通 mapped name；`state_entry_allows_target_subname_candidate()` 也不认可经过联合校验的 `child_element_map` evidence。最终没有把旧 stable token 通过当前 child map 唯一落到 `Child0.Face1`，误报 BackendGap。

### MapperHistory 事实与投影重复（当前最大差异）

`TopoNamingStateProbeExecutor` 已把 7 个 typed `MapperHistoryEvent` 放进 `NamedShape`，`response.rs` 又硬编码一套 probe entries、subshapes 和 7 events。没有一个 module 同时负责：

- terminal entry 选择；
- non-unique event 保留；
- `diagnostic_status` 到 public diagnostic 的映射；
- protocol-only provenance。

## 目标深 module 与 interface

### `TopoStateAdmission`

当前落点已创建：

```text
crates/opencascade/src/document_graph/topo_state_admission.rs
```

外部 interface：

```rust
struct AdmissionInput {
    graph: ParsedDocumentGraph,
    raw_topo_naming_state: Option<serde_json::Value>,
}

enum TopoStateAdmission {
    Absent,
    Accepted(ValidatedTopoState),
    Rejected(TopoStateRejection),
}

fn decode_admission_input(raw: &serde_json::Value) -> Result<AdmissionInput, RequestDecodeError>;
fn admit_topo_state(input: &AdmissionInput) -> TopoStateAdmission;
fn finish_decode(input: AdmissionInput) -> Result<RecomputeRequest, RequestDecodeError>;
```

当前实现已经具备 raw/typed 两阶段 decode、固定校验顺序、专用 rejection DTO、status-0 FFI 接入和真实 linked OCCT accessor。剩余工作不是重新设计 admission，而是在 Rust 编译恢复、FreeCAD ledger 闭包修复后重跑单元/C ABI/live gate。

这里必须采用两阶段解析：先在原始 `Value` 边界容忍地提取 document graph 与 raw `topoNamingState`，让 schema、state object JSON 类型和 encoding 错误进入协议 admission；通过后才完成 typed `RecomputeRequest`。与 topo-state 无关的 malformed JSON/request 仍是 request error，不能把全部反序列化错误降格为协议 diagnostic。

implementation 隐藏 schema、producer、document/object hash、top-level object owner、ElementMap/child map encoding 和 property ownership。不得复用当前会无条件序列化 `binaryPayloads/documentObjectUpdates` 的 `RecomputeResponse`，也不得复用字段不足的通用 `Diagnostic`；新增专用 DTO：

```rust
struct ProtocolRejectedPayload {
    diagnostics: Vec<PublicTopoStateDiagnostic>,
    results: Vec<PublicObjectResult>,
    element_reference_updates: Vec<ElementReferenceUpdate>,
}
```

`TopoStateRejection::into_public_payload()` 只生成这三个顶层字段，`PublicTopoStateDiagnostic` 精确承载 `severity/source/object` 及各 rejection 所需的 `actualSchemaVersion/offendingObject/...` 字段。

固定校验优先级：

1. schema；
2. producer；
3. documentHash；
4. top-level state object graph ownership 与 object state JSON 类型；
5. objectHash；
6. elementMap / childElementMap encoding；
7. property-local ownership。

该 module 的 raw/typed 两阶段 interface 是 admission test surface。FFI adapter 只消费最终 outcome，不再按 diagnostic code 猜协议拒绝。

### `RecomputeProjection`

当前落点已创建：

```text
crates/opencascade/src/document_graph/response_projection.rs
crates/opencascade/src/part/shape_exporter.rs
```

外部 interface：

```rust
struct ProjectedRecompute {
    public_results: Vec<PublicObjectResult>,
    state_evidence_objects: Vec<ObjectName>,
    diagnostics: Vec<Diagnostic>,
    element_reference_updates: Vec<ElementReferenceUpdate>,
}

fn project_recompute(
    request: &RecomputeRequest,
    analysis: &GraphAnalysis,
    context: &ComputeContext,
    updates: Vec<ElementReferenceUpdate>,
) -> Result<ProjectedRecompute, RecomputeError>;
```

当前 `response_projection.rs` 只有 `public_result_objects` target selection，`shape_exporter.rs` 已提供 summary 与 picking geometry；尚未达到上述完整 `ProjectedRecompute` interface。旧 dependency append helper 仍留在 `recompute.rs`，虽然主路径已不再调用。S2 后续以删除旧 seam、修完 244 个 public subshape diff 为主，不再重复实现 target-only selection。

它唯一决定：

- 哪些对象进入 public `results`；
- 哪些 request-local dependency 只进入 `state_evidence_objects`；
- public result 的 bbox、volume、topology counts、mesh、subshape identity 字段；
- 哪些 debug/feature fields 可以进入 production DTO。

`part/shape_exporter.rs` 隐藏 OCCT shape 到 public mesh/subshape 的转换；`response.rs` 只序列化最终 DTO。`namedShapeDebug` 与 history ledger 保持内部/debug channel，不作为默认 production field。

### `TopologyEvidenceLedger` 与 `TopoStateProjector`

目标落点已创建，但尚未完成单一事实源：

```text
crates/opencascade/src/topo_naming/evidence_ledger.rs
crates/opencascade/src/document_graph/topo_state_projection.rs
```

producer module 在 maker 执行期写 typed evidence：

```rust
struct ObjectTopologyEvidence {
    subshapes: BTreeMap<IndexedName, SubshapeEvidence>,
    mapped_name_candidates: Vec<MappedNameCandidate>,
    mapper_events: Vec<MapperHistoryEvent>,
    child_evidence: Vec<ChildElementEvidence>,
    owner_paths: Vec<OwnerPathEvidence>,
}
```

projector interface：

```rust
struct TopoStatePublication {
    state: TopoNamingState,
    diagnostics: Vec<Diagnostic>,
}

fn project_topo_state(
    request: &RecomputeRequest,
    evidence_objects: &[ObjectName],
    context: &ComputeContext,
) -> TopoStatePublication;
```

当前 `evidence_ledger.rs` 已记录 producer mapped-name candidates，`topo_state_projection.rs` 已负责 terminal/MapperHistory 投影；但 `response.rs` 仍保留从 `ObjectResult.named_shape_debug`、public subshape 和旧 helper 反推 state 的路径。必须删除 fallback，并补充 producer-to-public parity tests；同时先修复 `ElementMapSnapshot` 的 SID 持久化编译阻断。

不变量：

- producer ledger 只记录原始 typed candidate/event/owner/path evidence；terminal entries、public mapper history 与 diagnostics 只能由 projector 产生。
- ordinary seed/indexed `FaceN/EdgeN/VertexN` 只能是 `current_only`。
- stable evidence 必须有 producer-backed raw/canonical mapped name 或 terminal ledger/history 证明。
- entry key 必须等于 `mappedName.canonical`。
- terminal target 必须唯一；split/deleted/ambiguous/needs_reselect 不得进入 entries。
- internal `history_ledgers` 与 public `mapperHistory` 分开；前者不能自动发布。
- Body owner prefix、Pad object-local token 与 Compound child path 都来自 typed owner/path 数据，不靠字符串拼接。
- `elementMapVersion` 与 encoding 都是 `cad-core.element-map.v1`。
- admission 只按 FreeCAD 合同校验 `cadCoreVersion ∈ {fixture-contract-v1, cad-core-runtime-v1}`，删除 Rust 当前对 `freeCADVersion/occtVersion/backend/cadRsVersion` 的额外拒绝条件。响应保留输入 producer 的既有字段，只把 `fixture-occt-unspecified` 替换为真实运行时 OCCT version，不自行追加 `backend/cadRsVersion`。真实 runtime accessor 已在 `topo_state_runtime_occt_version()` 接入 `opencascade_sys::standard::occt_version_complete()`；当前任务是恢复编译后用 fresh FFI 报告证明其值来自实际 OCCT，且不存在硬编码回退。

### `StateBackedReferenceResolver`

目标独立落点已创建：

```text
crates/opencascade/src/document_graph/topo_state_reference.rs
```

interface：

```rust
enum StateReferenceResolution {
    ResolvedNoChange { provenance: ResolutionProvenance },
    RecoveredWriteback {
        item_index: usize,
        indexed: IndexedName,
        stable: String,
        provenance: ResolutionProvenance,
    },
    Deleted,
    Split { candidates: Vec<IndexedName> },
    Ambiguous { candidates: Vec<IndexedName> },
    Unsupported,
    BackendGap,
}

fn resolve_state_reference(
    context: &ReferenceItemContext,
    state: &ValidatedTopoState,
    evidence_graph: &RequestEvidenceGraph,
) -> StateReferenceResolution;
```

当前 Compound child-path 的 tuple evidence、联合验证、no-change 抑制和正式 resolver module 均已存在；但最新源码无法编译，且没有新 Rust FFI report 证明 ReferenceShadow/Compound keyed parity。因此 S4 仍是“代码存在、验收未闭合”。

`ReferenceItemContext` 携带 owner object、property、item index 与完整 reference item；`RequestEvidenceGraph` 提供当前 owner/child `NamedShape`、element-map key、path prefix 和 child object。ordinary mapped name 与 child path 是 module 内部实现 seam，完整 writeback 只从带 item index/provenance 的 outcome 投影。`app/geo_feature.rs` 只消费决定，不读取 topo-state JSON 细节。

### `MapperHistoryProjection`

它作为 `TopoStateProjector` 的内部 seam，不再成为第二套对外 interface：

- generated/modified/merge 的唯一 resolved 关系可以支撑 terminal entry。
- split/deleted/ambiguous 保留在 public history，并按 stable source 去重生成 diagnostics。
- Probe executor 只补 `unsupported_native_mapper_history` provenance diagnostic。
- Compound canonical collision 与 probe event 使用同一 typed projection。
- 删除 `response.rs` 中硬编码 probe state/events 的重复 implementation。

当前 `MapperHistoryProjection` 已迁入 `topo_state_projection.rs`；typed event 和 diagnostic projection 已存在，但 `response.rs` 仍保留旧 probe/helper 与 debug-history 反推路径。必须删除第二套实现，并以新 fresh report 重新证明 terminal/history/diagnostics 一致。

### depth、leverage 与 deletion test

这些 module 的 interface 很小，但 implementation 隐藏 request validation、对象发布 closure、OCCT shape DTO、ledger terminalization、child-path recovery 和 diagnostic projection，具有足够 depth。

删除任一 module 都会让其规则重新散落到 `recompute.rs`、`response.rs`、`geo_feature.rs`、FFI 和测试中，因此通过 deletion test。测试跨 interface 写，内部 helper 调整不应迫使所有 fixture test 重写，提升 locality 与 leverage。

## 必须锁定的目标不变量

### response object set

| case | public `results[].object` | `topoNamingState.objects` |
| --- | --- | --- |
| first recompute | `Box` | `Box` |
| Body Tip | `Body` | `Body, Pad, Sketch` |
| ReferenceShadow | `ProbeSketch` | `Body, Pad, ProbeSketch, Sketch` |
| Compound Link | `CompoundLink` | `ChildBoxA, ChildBoxB, Compound` |
| MapperHistory protocol-only | 当前允许 `HistoryProbe`；不作 native result parity 裁决 | `HistoryProbe` |

除 protocol-only probe 外，state-only object 不得因为需要 evidence 而进入 public results。

### public result DTO

普通 shape result 至少对齐：

```text
object
bbox
volume
topology_counts {faces, edges, vertices}
mesh {vertices, normals, indices, faceIds, edgeSegments, vertexPoints}
subshapes[]
```

subshape 至少对齐 `id/kind/indexed/subname/fullSubname/stableSubname/identityStatus/ShadowSub/ReferenceShadow`。数组比较可以按 `indexed` keyed alignment，不把 JSON 顺序当身份。

允许的 native-vs-frontend transport divergence 只来自既有精确 registry，例如目标 result 的 display mesh 和 ProbeSketch 当前 subshape transport。不得批准 whole result、额外 result object、全部 `metadata` 或全部 `namedShapeDebug`。

### first recompute

- Box result：bbox 为 `[0,0,0]..[2,3,4]`，faces=6、edges=12、vertices=8、26 subshapes，volume 约为 24。
- Box state：26 subshapes、0 entries、0 child maps、0 public history，status=`indexed_only`。
- 几何浮点量使用 comparator tolerance；不以浮点 JSON 字节相等代替语义比较。

### Body Tip ledger

- Body：26 subshapes、26 entries、1 child map、public history 0。
- Body child map：`key=Body:Pad:Pad`、`childObject=Pad`、`pathPrefix=Pad`、26 entries。
- Pad：26 subshapes、26 entries、public history 0。
- Body entry canonical 是 `Pad.` + Pad object-local canonical。
- Pad 每个 current target 恰好一个 terminal entry；内部 alias/history 不重复进入 entries。
- Sketch：`InternalFace1` 1 subshape、`g1;SKT;FAC` 1 entry、4 条 `Edge1..4 -> InternalFace1` generated/resolved history。

### ReferenceShadow

- Body：26 subshapes、26 entries、1 child map、public history 0。
- Pad：26 subshapes、26 entries、0 child maps、public history 0。
- ProbeSketch state：0 subshapes、0 entries、0 child maps、public history 0，status=`indexed_only`；其 result 中 3 个 current subshapes 属于已批准 transport divergence，不能反向填入 state。
- Sketch：1 subshape、1 entry、0 child maps、public history 0；entry 精确为 `g1;SKT;FAC`。
- 恰好 1 个 `ProbeSketch.ExternalGeometry` update。
- item-local `SubList/StableSubList/ShadowSub/ReferenceShadow` 基数一致；允许合同明确支持的 `SubList=[] + StableSubList-only`。
- `Face1` 恢复为 `Pad.#d:4;:G;XTR;:H*:*,F`。
- BREP 只允许位于 `elementReferenceUpdates[0].SubSet[0].ReferenceShadow[0].brep`。
- `topoNamingState`、results、完整对象 state 不得含 BREP。

### Compound child map

- ChildBoxA/B 各 26 个 current-only subshapes；entries 为空、status=`indexed_only`。
- Compound 53 subshapes。
- `Compound:ChildBoxA:Child0` 与 `Compound:ChildBoxB:Child1` 各 26 entries。
- 引用驱动 `Compound:ChildBoxA` map：`pathPrefix=Child0`，1 entry。
- top-level 唯一 resolved entry 是 `Compound/ChildBoxA.#f:1;BOX,F -> Child0.Face1`。
- 26 条 canonical collision 留在 ambiguous mapperHistory，不覆盖 terminal entry。
- 不发布 `topo_state_backend_gap`，不产生 backendGap writeback。
- CompoundLink native geometry：9 subshapes、faces 1、edges 4、vertices 4、volume 0、bbox `[0,0,0]..[0,2,2]`。

### MapperHistory protocol contract

- 7 events：generated、modified、split×2、deleted、merge、ambiguous。
- 只有 generated/modified/merge 的 3 个 resolved token 进入 entries。
- entries 精确为 `Source.#f:1;MHS,F`、`Source.#e:1;MHS,E`、`Source.#f:4;MHS,F`。
- subshapes 只含 `Face1/Edge1`。
- diagnostics 必须与 `.expeted.json` 的完整有序数组相等，顺序为：
  - `unsupported_native_mapper_history`
  - `split_stable_subname`
  - `deleted_stable_subname`
  - `stable_identity_ambiguous`
- 每项同时锁定 `severity/source/object/message`；后三项还锁定对应 `stableSubname`，不能只比较 code 集合。
- 此 case 不进入 native ledger/release verdict；只有未来 native C++ exporter 提供同等 producer history 后才能改变 role。

## 最小完整实施批次

以下编号保留依赖关系，但不再表示每步都从零开始。当前恢复顺序是 **S3 ElementMap SID 编译/持久化修复 → S3/S5 单一 projector → FreeCAD native ledger 闭包 → S2/S4 accepted-case 收敛 → S0 fresh rebuild/materialization → S6 双门禁**；S1 只做回归复验。

### S0：fresh binary、10-case catalog 与真实红灯

当前状态：runner/receipt/catalog/materialization 已实现并曾通过；当前 receipt、manifest、源码、dylib 和 FreeCAD native ledger 均不在同一闭包，freshness 与 authority 均 invalid，必须在两侧稳定后重跑。

目标是删除 stale artifact 不确定性，不改运行语义。

1. **已落地**：Rust C4M6 manifest 已加入 foreign-owner 第 10 行；MapperHistory 已改读 `.expeted.json`，source class 为 protocol-only/diagnostic contract。
2. **已落地**：`run_matrix.py` 对 `--ffi-lib`、环境变量和默认发现执行同一 build-receipt freshness 校验；stale/unknown dylib invalid，`--allow-stale-ffi` 禁止用于 materialization/release。
3. **已落地**：runner 已有 `--materialize-dir` 与 `--artifact-manifest`，通过同级临时目录原子替换 10 case artifacts 与 manifest。
4. **已落地基础设施、待重采**：receipt 已记录 library/build-input/dirty-state/Cargo.lock/features/profile/toolchain/build command/linked OCCT；当前旧 receipt 正确拒绝当前 dylib和源码，证明 freshness gate 在工作。
5. **待重做**：当前语义实现稳定后，用 receipt-generating runner clean/fresh build，重采 10 case 并重新生成 gap matrix；不能复用历史 materialization。

S0 退出条件：最终必须重新证明完整 build receipt freshness、10 cases、正确 native/protocol role 与 foreign case，并且输入的 FreeCAD native strict ledger 已通过；否则不得生成新的权威差异结论或发布 artifact。

### S1：`TopoStateAdmission` 与 diagnostics-only hard fail

当前状态：主要实现已落地，真实 OCCT accessor 也已接入；保留为回归 gate，不再作为当前首要编码批次。待 Rust 编译和 native ledger 恢复后复验。

1. 引入 raw `Value` → admission input → typed request 两阶段解析的 `TopoStateAdmission`，按固定优先级完成全部 request gate。
2. 增加独立 top-level object owner 校验；即使 object state 为空或 objectHash 为空也必须拒绝 foreign key。
3. 增加 `topo_state_object_owner_incompatible` diagnostic，带 `object` 与 `offendingObject`。
4. schema、producer、documentHash、objectHash、foreign-owner 五类拒绝都返回精确 public diagnostic 字段，不再塞进通用 `value` wrapper。
5. rejection 转为 status 0 public payload，顶层严格只有 `diagnostics/results/elementReferenceUpdates`。
6. `recomputeContext()`、feature executor、mesh、reference update 和 state exporter 不得被调用。
7. 真正 unknown type、OCCT failure、panic 等继续使用 FFI status 2；malformed JSON/request 继续使用 request status。
8. 增加 object `elementMap.encoding` 与 child map `elementMap.encoding` 两类 focused rejection；均返回 `topo_state_element_map_encoding_incompatible`、不发布新 state，且仍服从前述校验优先级。
9. producer admission 只保留 `cadCoreVersion` 白名单；删除 Rust 对其它 producer 字段的额外 hard reject，并通过 OCCT runtime accessor 精确完成响应规范化。

S1 应原子落地 admission tests、FFI tests 和实现，不提交永久 failing test。

### S2：`RecomputeProjection` 与 public shape DTO

当前状态：target-only object set、public summary/picking 字段与 debug 抑制已落地；剩余集中在 244 个 subshape DTO 差异、10 个 result 字段差异、6 个 JSON 形态差异和 1 个几何数值差异。

1. 把 visible result 选择集中到 `RecomputeProjection`；普通 case 只按 `document.targets` 发布。
2. 复用当前 state-only dependency closure，但返回 `state_evidence_objects`，不再构造可被误塞进 results 的伪 display result。
3. Body 只发布 Body；ReferenceShadow 只发布 ProbeSketch；Compound 只发布 CompoundLink。
4. MapperHistory 维持当前 protocol-only result 裁决，不在本步擅自删除 `HistoryProbe`。
5. 抽出 `part/shape_exporter.rs`，对齐 bbox、volume、topology_counts、mesh、subshape fields 与 display/picking ID。
6. `mesh.summary` 的 bbox/volume 转到 public result 顶层；补 `edgeSegments/vertexPoints`。
7. `namedShapeDebug` 和通用 `metadata` 不再默认进入 production DTO；确需保留的 feature transport 通过明确字段或 debug adapter 发布。
8. 保留 display mesh 和 ProbeSketch current subshapes 的现有前端合同，不能为了 raw expected 相等删除。

S2 的 test surface 必须精确断言 result object set 与 public field set，不能只断言“包含目标对象”。

### S3：typed evidence ledger 与 topo-state projector

当前状态：Pad producer/evidence/projector 已推进，但工作树被 `StringIDRef`/snapshot 迁移阻断。第一动作是恢复 ElementMap 编译并完成 snapshot round-trip；随后让 public/topo-state projector 只读 ledger，迁移测试并删除 response fallback；完成前不得构建新 artifact。

1. **立即修复**：统一 `StringIDRef` 与旧 token 类型，补齐 `ElementMapSnapshot.mapping_sids`、child SID 的 token/index 序列化与 restore，恢复 `cargo check`。
2. **已部分落地、待证明**：Pad compact provenance 已前移到 `FeatureExtrude`/`NamedShape` producer ledger；opaque `:H...` tag 必须来自本次 producer evidence，不能由 response 固定或猜测，并需通过 arbitrary Pad focused parity。
3. 把 Sketch InternalShape/FaceMaker evidence 写成 typed object evidence，不从 serialized `ObjectResult` 反向查找。
4. 把 Body Tip owner prefix 与 child map 保存为 typed owner/path 字段。
5. 把 Compound child local/current-only evidence 直接从 request-local child `NamedShape` 发布。
6. **已创建、待收口**：`TopologyEvidenceLedger`/`TopoStateProjector` 已存在；projector 只发布 terminal entries 与明确 public history，`history_ledgers` 保持 debug-only，并删除 `response.rs` 的反推路径。
7. 删除 Pad/Body duplicate target、112/123 public history 泄漏、response 层 first-match 选择及约 60 个已被新 projector 覆盖的 topo-state helper。
8. **已落地**：`elementMapVersion=cad-core.element-map.v1`、producer 去除 `backend/cadRsVersion`、实际 OCCT runtime accessor 已完成；需在新 dylib 中复验。
9. `response.rs` 中旧 topo-state/probe 特殊 helper 在新 projector 覆盖后直接删除，不保留两套 implementation。

S3 完成后，Body Tip、ReferenceShadow 和 ChildBox 的完整 object state 都必须与 authority keyed comparison 相等。

### S4：Compound child-path 与 reference writeback 收敛

当前状态：tuple evidence、联合验证、no-change 抑制和正式 resolver module 已存在；旧 report 的 diagnostics/childElementMaps=0 不能代表当前，仍需新 fresh report 证明 Compound elementMap status/result identity 与 ReferenceShadow 完整 update。

1. **代码已落地、验收待做**：使用 `StateBackedReferenceResolver`，先读取已经 admission 的旧 state evidence，再用当前 request-local `NamedShape`/child map 验证 target。
2. **已落地 tuple 校验**：child path 恢复必须联合校验 `ownerObject/key/pathPrefix/childObject/target.object/target.subname`。
3. **已落地主路径**：允许使用已经通过 objectHash/ownership 校验且 recoverability 唯一的 `target.subname=Child0.Face1`，并确认当前 Compound `NamedShape` 存在该 subshape。
4. **已落地 no-change 抑制**：`ResolvedNoChange` 不产生 diagnostic 或 update；只有 current name/stable token 真变化才返回 `RecoveredWriteback`。
5. deleted/split/ambiguous 保持结构化失败，不能按 bbox、面积、顺序或字符串后缀任选候选。
6. ReferenceShadow 继续复用同一 resolver decision；BREP/fingerprint 只参与证据验证，不参与 shape 构造。
7. `app/geo_feature.rs` 删除对 topo-state DTO 的重复解释，只投影 resolver outcome。

S4 退出条件是 Compound row 无 BackendGap/无 update，同时 ReferenceShadow row 的完整 update 不退化。

### S5：MapperHistory 与 diagnostics 单一投影

当前状态：typed events/diagnostics projector 已迁入独立 module，但旧 probe/debug-history 路径未删除；旧 470 个 mapperHistory diff 仅为历史证据，需在新 fresh report 中重新测量。

1. **producer 已有、消费未唯一化**：以 executor 已生成的 typed `MapperHistoryEvent` 为单一事实。
2. 删除 `response.rs` 硬编码但仍残留的第二套 probe events/entries/subshapes，以及 debug history 自动扩展路径。
3. **已迁入 projector**：`MapperHistoryProjection` 生成 terminal entries/subshapes/events，但必须删除 response 第二套实现并由 focused contract 证明 exact equality。
4. **diagnostics last gate 已对齐**：按 source stable token 去重生成 3 条 warning，并补 1 条 `unsupported_native_mapper_history` info；保持 diagnostics category 为 0。
5. Compound canonical collision 也走同一投影，不另造 JSON。
6. **已落地**：Rust test 读取 `.expeted.json`，不得退回已不存在的 `.freecad.json`。
7. focused test 只把 diagnostics/topo state 当 protocol-only 强合同；是否公开 HistoryProbe result 保持当前既定裁决。

### S6：strict gate、artifact 与 release 收口

当前状态：Rust blackbox 工具和 FreeCAD `RustFfiActualSource` adapter 已实现；release 仍未可判定。旧 adapter 测试曾 4/4 pass，但当前 native ledger 失败且 Rust 无法构建，不能宣称 Rust accepted-case verdict。

1. Rust blackbox comparator 补齐 blind spots：exact result object set、state object set、elementMapVersion/status、canonical entry key/target/source/recoverability/evidence、child maps、完整 mapper history、diagnostics、writeback 与应为空的 absence。
2. hard-fail profile 改为要求 `ffiStatus=0 + exact payload`，不再要求非零 FFI status。
3. 增加 foreign-owner 与 stale-library mutation tests。
4. Rust runner 只负责 live execution、artifact 和无宽泛忽略的结构检查；最终 protocol divergence/release verdict 复用 FreeCAD `freecad_expected_parity` module。
5. **已落地并通过 4 个单元测试**：`cad-core/tools/freecad_expected_parity/sources.py` 已增加 `RustFfiActualSource`，由 `compare_freecad_expected.py --actual-source rust-ffi --ffi-lib <path>` 选择；它只负责一次 FFI 调用、要求 status 0、释放 `CadRsResult` 并返回解包 payload，比较仍由既有 engine/registry 完成。
6. 重生成 `cad-rs-res` 和 manifest；final gate 必须发现 10 cases，无 anonymous/unclassified diff。
7. 4 个 accepted native case semantic green；5 个 rejected native case exact envelope green；MapperHistory protocol-only focused test green。
8. 更新 Rust capabilities/RS10 文档只能发生在 live report 之后，不能用旧 snapshot 宣布完成。

## 优先负向测试

- foreign state object 的 objectHash 为空时仍必须 owner hard fail。
- schema 与 foreign owner 同时错误时必须由 schema 优先裁决。
- documentHash 与 object owner 同时错误时必须由 documentHash 优先裁决。
- `topoNamingState.objects[*]` 为非 object JSON 值时必须走协议 admission 的 status 0 rejection，不能被 typed request 反序列化提前变成 FFI request error；与 topo-state 无关的 malformed request 仍必须是 request error。
- hard fail 一旦调用任何 feature executor、mesh 或 state exporter，测试必须失败。
- hard fail 出现 `topoNamingState/documentObjectUpdates/binaryPayloads` 任一顶层字段必须失败。
- FFI 把任一协议拒绝放进 `error` buffer 或返回 status 2 必须失败。
- object `elementMap.encoding` 或 child map `elementMap.encoding` 不兼容时，缺少 `topo_state_element_map_encoding_incompatible` 或仍生成新 state 必须失败。
- public results 新增 Pad、Body dependency 或其它 state-only object 必须失败。
- `namedShapeDebug/historyLedgers` 泄漏进 production payload 必须失败。
- terminal map 同一 current target 出现第二个非 merge entry 必须失败。
- entry key 不等于 `mappedName.canonical` 必须失败。
- split/deleted/ambiguous event 进入 terminal map 必须失败。
- child map owner、key、pathPrefix、childObject 或 target 任一被篡改时不得恢复。
- Compound 已唯一 resolved 且名字未变时出现 diagnostic/update 必须失败。
- ReferenceShadow BREP 出现在允许路径之外必须失败。
- MapperHistory 缺任一 event/diagnostic 或把 protocol-only case 放进 native discovery 必须失败。
- build-input、dirty state、features/profile/toolchain 或 linked OCCT 任一与 build receipt 不匹配时 runner 必须 invalid；显式路径与环境变量路径不得绕过。

## 非目标

- 不修改或手工补 `expected/*.freecad.json`、`.freecad.ledger.json`、`.expeted.json`。
- 不从 Rust output、bbox、mesh、面积或 subshape 顺序反推 expected。
- 不把 `topoNamingState`、ReferenceShadow BREP 或 ledger 当几何输入。
- 不引入服务端 session、数据库、跨请求 NamedShape/ElementMap 缓存。
- 不删除前端需要的 display mesh、edgeSegments、vertexPoints 或 ProbeSketch current subshapes。
- 不把 MapperHistory protocol-only probe 宣称为 native FreeCAD parity。
- 不在 response/FFI/comparator 中按 fixture 名称拼 Pad token、child path、diagnostic 或 result。
- 不在本轮顺带修 C4M6 之外的全部 phase；共享 module 的现有 regression 必须通过，但不扩大 parity 声明。
- 不覆盖 FreeCAD 当前 dirty fixture、collector、release-gate、接口文档或用户的 `AGENTS.md/DESIGN.md` 修改。

## 风险与控制

- 风险：旧 binary 让已实现能力继续显示为红。控制：freshness 是 S0 preflight，build receipt 绑定完整 build-input/dirty-state digest 与 dylib digest。
- 风险：为 expected 相等删除生产 transport。控制：semantic fields 与精确 protocol divergence 分层，mesh/picking contract 有独立 test。
- 风险：把 `response.rs` 拆小但仍由 debug DTO 反推 state。控制：seam 以 typed ledger 为输入，文件移动本身不算完成。
- 风险：Pad compact bridge 固化成 fixture-shaped token generator。控制：producer ledger 必须来自 FeatureExtrude/TopoShape history；bridge 在正式 ledger 覆盖后删除。
- 风险：利用旧 state `target.subname` 变成 FaceN 猜测。控制：只有完整 admission + unique recoverability + 当前 child map 联合验证才允许 target candidate。
- 风险：public history 太少而丢诊断，或太多而泄漏内部 trace。控制：terminal map、public mapper history、debug history ledger 三类模型分开。
- 风险：改变 FFI error 语义破坏真正执行错误。控制：protocol rejection 返回 Ok payload；unknown type、OCCT error、panic 继续 status 2 并有负向 test。
- 风险：Rust comparator 与 FreeCAD release engine 再次漂移。控制：Rust runner 只做执行/结构 gate，最终 verdict 复用既有 parity interface。
- 风险：协议 producer 版本自相矛盾。控制：admission compatibility 与 response producer 分开建模；输入 placeholder 只作迁移兼容，输出记录实际 kernel version。

## 验收命令

以下命令中的 `--materialize-dir/--artifact-manifest`、`--build-ffi/--release-gate`、`--actual-source rust-ffi` 与 `tests.test_rust_ffi_expected_source` 已在同一 fresh dylib/authority 基线上完成复验。最终 C4M6 native 与 Rust FFI 均为 semantic green、release gate passed；精确差异仅保留在既有 protocol-divergence registry。

### 本轮文档校验

本方案本身只需要：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- \
  docs/重构/7-10-08-24-【已实现】Rust几何内核C4M6TopoNamingState权威对齐重构方案.md
```

### 每步短跑

```bash
cd /Users/li/Chili3DProject/opencascade-rs

# S1 admission / FFI
cargo test -p opencascade topo_naming_state_dto --lib
cargo test -p opencascade topo_naming_state_c4m6 --lib
cargo test -p cad-core-ffi --test c_api_errors

# S2 public result / state-only projection
cargo test -p opencascade \
  topo_naming_state_c4m6_body_tip_stable_recovery_matches_freecad_expected --lib
cargo test -p opencascade \
  topo_naming_state_c4m6_reference_shadow_brep_writeback_matches_freecad_expected --lib
cargo test -p opencascade \
  topo_naming_state_c4m6_link_compound_child_maps_matches_freecad_expected --lib

# S3-S5 evidence / resolver / mapper
cargo test -p opencascade topo_naming_state_response_snapshot --lib
cargo test -p opencascade topo_naming_state_reference_recovery --lib
cargo test -p opencascade part_design_pad --lib
cargo test -p opencascade topo_naming_state_c4m6 --lib
```

每步结束只对本步文件运行 `cargo fmt --all -- --check` 与 `git diff --check`，不等待最终阶段才发现格式问题。

### fresh artifact 与阶段回归

```bash
cd /Users/li/Chili3DProject/opencascade-rs

python3 tools/cad_core_blackbox/run_matrix.py \
  --manifest target/cad_core_blackbox/rs10_m2_topo_naming_state/manifest.tsv \
  --profiles docs/几何支持/CADCoreRs5.0/RS5-M1-cad-core黑盒矩阵等价主线/矩阵/rs15_assertion_profiles.tsv \
  --build-ffi \
  --all \
  --release-gate \
  --report target/cad_core_blackbox/rs10_m2_topo_naming_state/focused_report.json \
  --materialize-dir /Users/li/Chili3DProject/FreeCAD/cad-core/fixtures/c4m6/cad-rs-res \
  --artifact-manifest /Users/li/Chili3DProject/FreeCAD/cad-core/fixtures/c4m6/cad-rs-res/manifest.json

python3 -m py_compile \
  tools/cad_core_blackbox/compare.py \
  tools/cad_core_blackbox/run_matrix.py
python3 -m unittest discover \
  -s tools/cad_core_blackbox/tests \
  -p 'test_*.py'

cargo test -p cad-core-ffi --tests
cargo test -p opencascade topo_naming_state_c4m6 --lib
```

不得先独立执行 `cargo build` 再把该 dylib 交给 release materialization；独立 build 不会刷新 runner 的 content-addressed receipt。需要清理构建缓存时，在同一命令增加 `--clean-ffi`，不能把 `--build-ffi` 与 `--ffi-lib` 或 `CAD_RS_FFI_LIB` 混用。

阶段报告必须同时记录 10-case discovery、library SHA/freshness、native/protocol role、FFI status、exact failures 和 semantic category；不能只看进程退出码。

### FreeCAD authority 与重型收口

只在 S6 最终收口时复核 native authority：

```bash
cd /Users/li/Chili3DProject/FreeCAD

FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd \
  python3 cad-core/tools/collect_freecad_expected.py \
  --phase c4m6 \
  --check \
  --skip-unsupported

python3 cad-core/tools/validate_freecad_expected_ledger.py \
  --phase c4m6 \
  --strict

cd cad-core
cmake --build build --target cad-core cad_core_ffi
python3 -m unittest \
  tests.test_freecad_expected_public_parity \
  tests.test_topo_naming_state_response
python3 tools/compare_freecad_expected.py --phase c4m6 --release-gate
python3 tools/compare_freecad_expected.py --phase c4m6 --run-contract-tests
python3 -m unittest tests.test_rust_ffi_expected_source
python3 tools/compare_freecad_expected.py \
  --phase c4m6 \
  --release-gate \
  --actual-source rust-ffi \
  --ffi-lib /Users/li/Chili3DProject/opencascade-rs/target/debug/libcad_core_ffi.dylib
```

命令退出 0 之外还必须锁定统计：collector `processed=9, skipped=1, failed=0`；其中唯一 skipped case 是 MapperHistory protocol-only。strict ledger validator 覆盖 9 个 native/rejected pair；MapperHistory 由 focused contract test 单独覆盖。release report 必须断言 `summary.cases=9`，禁止 0-case 或漏 discovery 假绿。

Rust FFI actual source 接入既有 parity seam 后，还必须用同一 engine 对 live Rust payload 运行 C4M6 gate。最终 report 中：

- 4 个 accepted native case 必须 semantic green。
- 5 个 rejected native case 必须 exact diagnostics-only envelope green。
- MapperHistory 只由 protocol-only focused test 判定。
- 所有 raw diff 都必须是精确 registry 项或明确 red；不得出现 anonymous/unclassified acceptance。

## 完成条件与推荐顺序（历史记录）

原始依赖顺序是 S0 → S1 → S2 → S3 → S4 → S5 → S6；当时按工作树恢复时执行：

1. 先完成 `StringIDRef` 类型统一与 `ElementMapSnapshot` mapping/child SID round-trip，恢复 `cargo check`；不得只加回旧 `String` bridge 维持两套实现。
2. 完成 S3 typed ledger/projector 与 S5 MapperHistory 单一投影，删除 response/debug 反推和死 helper。
3. 修复 FreeCAD native collector/ledger 的 Body Tip strict 闭包，再以 accepted-case keyed diff 收敛 S2 public subshape DTO 与 S4 ReferenceShadow/Compound 剩余差异。
4. 用 runner `--build-ffi --release-gate` 重新执行 S0 freshness/materialization，确保源码、receipt、dylib、10 artifacts、current expected/ledger 属于同一基线。
5. 执行 S6 Rust focused + FreeCAD Rust-FFI 双门禁和全套回归；只有此时才更新 capabilities/RS10 状态并考虑重命名文档。

每次 fresh gate 后，如果某个旧差异已经消失，删除对应任务，不为旧 dylib 重做实现。

完成条件：

- `cad-rs-res/manifest.json` 绑定可复验 build receipt 与 fresh dylib，发现 10 个 case，且 artifact 由一次原子 materialization 产生。
- 五种 state request rejection 都在几何执行前裁决，并以 status 0 exact public payload 返回。
- foreign top-level object 无论 hash 是否为空都不能绕过 admission。
- public result object set 与 state evidence object set 分离，Body/ReferenceShadow/Compound 无额外 dependency result。
- public result 的 summary、mesh、subshape identity 与 cad-core transport contract 对齐；只剩既有精确 divergence。
- Body/Pad/Sketch、Compound/ChildBox 的 state counts、canonical keys、owner/path、terminal uniqueness 与权威一致。
- internal history ledger 不再泄漏为 Body/Pad public mapper history。
- Compound stable token 唯一恢复到 `Child0.Face1`，无 BackendGap、无无效 writeback。
- ReferenceShadow update 完整对齐且 BREP 无泄漏。
- MapperHistory protocol-only case 的 3 entries、2 subshapes、7 events、4 diagnostics 全部通过。
- Rust blackbox 无 stale-library 旁路、无缺 case、无 partial-field 假绿。
- FreeCAD native check、ledger strict validation、Rust live gate、focused contract tests 全部闭合。

上述完成条件已全部闭合；本文件已保留原时间前缀并重命名为 `7-10-08-24-【已实现】Rust几何内核C4M6TopoNamingState权威对齐重构方案.md`。
