# FGM-N1 CAD Core ElementMap 生产者切片探针

> 本回执只证明按需 trace 诊断设施的实现状态，不证明或否定 public/ledger 一致性。public/ledger green 时无需读取这些 sidecar；trace 缺失、invalid 或 different 只影响内部诊断。

状态：`【已实现】`（2026-07-12）

## 实施基线

- live HEAD：`05f14b6b0c`（`feat: 引入 ElementMap 生产者 Trace 对齐链路`）。
- 开始时 `git status --short -uall` 只显式列出用户未跟踪的 `cad-core/include/cad_core/app/element_map_producer_trace.h`；实施在该 live 骨架上继续，没有回退或覆盖用户资产。
- 未暂存、未提交；`fixtures/**`、`expected/**`、native producer trace、public parity verdict 和 release verdict 均未修改。

## 交付边界

- `app::ElementMapProducerTrace` 提供 multi-transaction、严格 sequence/range、move-only RAII transaction/scope、parent scope、objectTag index、exception/cancel/abort、reentrancy 拒绝、内容哈希 snapshot、`create/copy/share/reset/drop` identity 生命周期、闭包校验和 drain 释放。
- recorder 在 parsed request preflight 前创建，由同一次 `ComputeContext`、`StringHasher` 和 Part producer 链共享，不建立跨请求几何 session。
- StringHasher、NamedShape/ElementMap、raw mapper 使用 value-only 投影；raw M/G/D 在原生产顺序中只捕获一次，snapshot 只 const 读取该临时值视图，recorder 不持有 OCCT shape 或业务指针。
- 通用 ElementMap/NamedShape producer 落在 `part/topo_shape.cpp`；`app/element_map.cpp` 仍只承担 Sketch `Internal*` helper。
- CLI 对每次 parsed recompute 默认写同次 `<stem>.cad-core.producer-trace.json`，写前验证闭包与 request/response canonical SHA-256 绑定，临时文件发布失败返回 non-zero 并清除 sidecar/temp。
- live CAD Core runner 独立发现、验证和物化 actual trace；trace 缺失/无效不会丢弃可比较的 public response 或改变 parity/release verdict，但 materialization 会硬失败。选中范围内全部 response+trace 先完整落临时文件，再作为一个可回滚原子组发布。
- N2 first-divergence comparator 未实现；N1 只冻结其 actual trace interface。

## 验证回执

- 只构建 `cad-core` 和 `cad-core-element-map-producer-trace-probe`，未构建完整 FreeCAD。
- focused C++ probe 通过：multi-transaction、第二个空 transaction、transaction-local SID 重启、parent/LIFO、exception/cancel、reentrancy、ElementMap Drop、`create/copy/share/reset/drop` identity、ordered entry-local refs、U/L duplicate suppression（kept/suppressed 两侧 parent、child、ordinal、entry-local refs 均独立保留）、recorder attach 前后 SID 分配/related 顺序一致、两次 drain。
- focused Python：51 项 N1 测试通过，另有 5 项共享 validator/N2 projection 兼容测试通过；覆盖 sequence/range/object index、transaction-local SID namespace、每个 required producer scope 的 final checkpoint、event-time SID（含 `element_map.write.elementIdRefs`）、ledger entry-local refs、child inventory bound/nested source ledger/collision inventory、raw mapper input/output closure、FaceMaker `namesUsed`/combo refs、Body/Sketch/U-pass guard、Body 无 replay、artifact binding、sidecar 命名/替换/写失败、preflight、dependency skip、mutation、determinism、public isolation、trace verdict 解耦和 runner 全组选中产物原子回滚。
- `producer_trace_validation_matrix.tsv` 的 21 项门禁均为 validated 或有源码依据的 runtime not-applicable。
- LinearPattern actual trace 连续运行两次逐字节一致。
- 从 `HEAD` 只读导出并独立构建接入前 CAD Core；rect-pad-pocket、LinearPattern、Body Tip 三条正常 public response 与当前版本逐字节一致，因而其中 NamedShape/ElementMap、mapperHistory 和 SID 派生 mapped-name 顺序也逐字节一致。focused probe 另以相同 StringHasher 操作直接证明 recorder attach 前后 SID allocation table/related 顺序相同。
- `git diff --check -- cad-core docs/FreeCAD几何生态迁移工程-New` 通过。

## `/tmp` actual trace

- `/tmp/fgmn1-body-tip.cad-core.producer-trace.json`
- `/tmp/fgmn1-rect-pad-pocket.cad-core.producer-trace.json`
- `/tmp/fgmn1-chamfer.cad-core.producer-trace.json`
- `/tmp/fgmn1-fillet.cad-core.producer-trace.json`
- `/tmp/fgmn1-linear-pattern.cad-core.producer-trace.json`
- `/tmp/fgmn1-open-wire.cad-core.producer-trace.json`
- `/tmp/fgmn1-self-intersection.cad-core.producer-trace.json`
- `/tmp/fgmn1-inter-edge-split.cad-core.producer-trace.json`
- `/tmp/fgmn1-preflight-rejection.cad-core.producer-trace.json`
- `/tmp/fgmn1-dressup-failure.cad-core.producer-trace.json`
- `/tmp/fgmn1-reference-shadow.cad-core.producer-trace.json`

## Not applicable

- `maker.parallel_coplanar`：CAD Core 当前 mapper 有 high-level ordinal/`shapeOffset`/`INT_MIN` 证据，但没有 FreeCAD 的 plane/parallel/coplanar 几何判别分支；trace 显式发布 `not_applicable`，不伪造测试结果。
- runtime cancel：recorder 的真实 cancel RAII 路径已验证，但 CAD Core runtime 当前没有取消源；按规格不伪造 runtime cancel。
- `dressup-failure-diagnostics` role 为 `unsupported` 且 native trace 缺失；这只表示该专项 trace 诊断不可用，不构成 public/ledger parity 结论。CAD Core actual failure/partial-write/last-checkpoint 已作为内部测试验证。
- `topo_shape_expansion.cpp` 没有独立于 `namedShapeForMakerHistory`/Boolean/Refine 的 ElementMap producer；对应切片在 `part/topo_shape.cpp` 的正式生产 seam 接入，避免重复事件。

## 矩阵状态

- blocker queue：S0-S7 全部 `closed`，N2 为 `ready`。
- slice/source/implementation：required 项均 `implemented_validated`，或使用上节源码依据标为 `not_applicable`。
- fixture：9 个 native/边界 case validated；1 个 unsupported native case 为 actual-only validated。
- validation：21 项全部 closed；N2 可只消费独立 actual trace，无需读取 CAD Core 内存或业务对象。
