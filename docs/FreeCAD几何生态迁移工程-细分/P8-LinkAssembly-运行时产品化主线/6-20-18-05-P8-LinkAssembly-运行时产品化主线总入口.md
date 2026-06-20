# P8 Link / Assembly 运行时产品化主线总入口

本文是 `P8-LinkAssembly-运行时产品化主线` 的执行入口。后续实现应从本目录的 `工作步骤细分/` 队列启动，不直接续跑旧 P8 Assembly 子包中的残留入口。

## 主线目标

- 把完整 Link 账本、ShowElement 持久事务、cross-document hash / postfix 生命周期、多层 LinkSub / imported ElementMap、Assembly solver 扩展和 Worker / WASM / Web 合同合并为一个大功能模块。
- 让前端可持久化的 `DocumentObject graph` 成为唯一真实数据；CAD Core 只返回 shape、mesh、subshape、`elementReferenceUpdates`、`documentObjectUpdates`、diagnostics 和 capability。
- 所有实现必须先有 FreeCAD source authority，再落到 cad-core 的 `app` / `runtime` / `topo` / `assembly` / `adapters` 对应层。

## 执行队列

| 顺序 | 步骤 | 路径 | 目标 |
| --- | --- | --- | --- |
| 0 | 工作步骤总入口 | `工作步骤细分/6-20-18-05-【已实现】P8-LinkAssemblyRuntime工作步骤总入口.md` | 索引文件已建立，实际队列从 S0 开始 |
| 1 | S0 | `工作步骤细分/6-20-18-06-【已实现】P8-LinkAssemblyRuntime-S0-live基线与旧队列裁决.md` | 已完成：复核 live baseline、旧队列和当前 capability |
| 2 | S1 | `工作步骤细分/6-20-18-07-【已实现】P8-LinkAssemblyRuntime-S1-FreeCAD源码候选矩阵.md` | 已完成：精确 source authority 和 cad-core 落点 |
| 3 | S2 | `工作步骤细分/6-20-18-08-【已实现】P8-LinkAssemblyRuntime-S2-Link账本与ShowElement事务.md` | 已完成：补完整 Link ledger 和 ShowElement 持久事务 |
| 4 | S3 | `工作步骤细分/6-20-18-09-【已实现】P8-LinkAssemblyRuntime-S3-跨文档Hash与Postfix生命周期.md` | 已完成：补 cross-document hash / postfix / alias 生命周期 |
| 5 | S4 | `工作步骤细分/6-20-18-10-【已实现】P8-LinkAssemblyRuntime-S4-多层LinkSub与ImportedElementMap.md` | 已完成：补多层 LinkSub 和 imported ElementMap |
| 6 | S5 | `工作步骤细分/6-20-18-11-【已实现】P8-LinkAssemblyRuntime-S5-AssemblySolver扩展.md` | 已完成：Assembly solver 扩展关闭为 regression baseline + future oracleFirst gate |
| 7 | S6 | `工作步骤细分/6-20-18-12-【已实现】P8-LinkAssemblyRuntime-S6-WebRuntime合同冻结.md` | 已完成：冻结 Worker / WASM / Web runtime 合同并发布 |

## 旧包关系

- `P8-Assembly-Joint-Placement-OndselSolver收敛主线`、`P8-Assembly-Reference-JCS-MarkerPlacement收口主线`、`P8-DistanceType*`、`P8-*Joint-OndselSolver*` 是 source material 和 regression baseline，不作为本主线的当前队列入口。
- `docs/CADCore4.0/C4-M5-AssemblyRuntimeAdapter产品化主线` 已经完成 C4 维度收口；本包只消费其 Assembly / runtime / adapter 基线，不重复执行已关闭队列。
- S0 必须用 live code、fixtures、capabilities 和队列工具重新裁决 stale / covered / backendGap，不得只按旧 memory 或旧文档结论推进。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线/工作步骤细分 --format markdown
```

## 当前状态

S0 已完成 live 基线裁决：旧 `P8-Assembly-Joint-Placement-OndselSolver收敛主线`、`P8-Assembly-Reference-JCS-MarkerPlacement收口主线`、`P8-DistanceTypeExtendedGeometry-OndselSolver收口主线` 和 `C4-M5-AssemblyRuntimeAdapter产品化主线` 都不作为本主线当前队列入口直接续跑；其中 C4-M5、MarkerPlacement 和 DistanceType 队列已空，旧 AssemblySolver 只因总入口未加 `【已实现】` 被队列工具列出，实际 S0-S7 和后续矩阵已收口。

当前 regression baseline 使用：

- `cad-core/fixtures/p8/app-link-*.json` 与 `cad-core/tests/test_p8_features.py` 中 Link / ShowElement / ElementCount / LinkGroup / CopyOnChange / FullSubList / stable history / imported ElementMap Link chain focused cases。
- `cad-core/fixtures/c3m2/xlink-*.json`、label/source rename cases 和对应 `test_p8_features.py` cross-document diagnostics。
- `cad-core/fixtures/c3m6/assembly-*.json`、`cad-core/fixtures/c3m6/expected/*.freecad.json`、`cad-core/fixtures/c4m5/*.json` 与 `test_adapters.py::test_c_api_capabilities_exposes_web_contract_facts`。
- C4-M5 adapter contract baseline：CLI / C ABI / Worker / WASM 共享 `cad-core-result-v1`，`mesh_limit_exceeded`、`adapter_resource_limit` 和 binary payload metadata 已由 `test_adapters.py` 锁定。

S1 已完成 source authority 闸门：`p8_link_assembly_runtime_source_candidates.tsv` 中 Link ledger、ShowElement、CopyOnChange、PropertyLink / PropertyXLink、plain group、mapped postfix / stable subname 和 imported ElementMap 已升级为 `sourceReady`；ElementCount collapsed lists、AssemblyLink / AssemblyObject / MarkerPlacement / DistanceType / JointGroup、C API 与 Worker / WASM rows 已裁定为 `baselineOnly`。`P8LAR-BLOCK-002` 已关闭，后续不再因“Link ledger 缺少源码依据”阻塞 S2-S4。

S2 已完成 Link ledger 与 ShowElement 持久事务：S2 focused tests 已把 synthetic / materialized / stale / toggle-off、ElementList owner sync、child sync、CopyOnChange touched 和 PlacementList / ScaleList / VisibilityList 优先级的 `documentObjectUpdates` 应用到下一次 request graph 并验证稳定。CAD Core 仍保持 stateless，只返回前端可持久化 graph mutation 建议。

S3 已完成 cross-document hash / postfix / mapped alias 生命周期收口：`cad-core/src/app/property_links.cpp`、`cad-core/src/graph/recompute_plan.cpp` 和 `cad-core/src/runtime/recompute.cpp` 已形成 request-local schema，输出 `missing_external_document`、`external_document_pending_reload`、`external_document_unloaded`、`document_hash_mismatch` diagnostics 与 `elementReferenceUpdates` / `documentReference` / `labelReferenceRename` / `sourceObjectRename` / `FullSubList` 建议；本轮补充 `xlink-mapped-postfix-rename-recovery` fixture，并把 source-object rename 与 mapped postfix mismatch 纳入 `test_p8_features.py` 的 S3 focused coverage。CAD Core 仍不缓存外部文档状态，也不传递完整外部文档 BREP。

S4 已完成多层 LinkSub / imported ElementMap / retag history 收口：`app-link-imported-element-map-chain` 把 BREP、STEP、IGES imported shape 的 stable ElementMap 分别经中间 App::Link、LinkSub 和下游 LinkGroup child map 消费；已有 multilevel LinkSub、plain group、LinkGroup、FullSubList external tag、stable split/deleted history 和 merge history retag tests 继续作为回归基线。当前实现未新增 output 层字符串修剪，未修改 expected，未把 BREP 作为跨请求状态。

S5 已完成 docs-only 审计：未发现新的产品必需、FreeCAD/Ondsel 路径清楚、且已具备 checked-in oracle 的 Assembly solver 实现缺口；旧 JointType / MarkerPlacement / DistanceType 队列继续作为 regression baseline，不直接续跑。当前发布口径以 `cad-core/src/adapters/c_api/c_api.cpp::capabilitiesJson()` 为准：`assembly.supported_joint_matrix` 覆盖 13 个 FreeCAD JointType，`unsupported_joint_matrix=[]`，representative fallback 保持 `fallback_metadata_only` 且 `available=false`。

S5 的 fixture / test 证据为：`cad-core/fixtures/c3m6/expected/assembly-*.freecad.json` 共有 50 个 Assembly expected，其中 45 个 active assertion、5 个 default/TODO 或 PointCurve `known_gap` 作为 future oracleFirst gate；`cad-core/tests/test_p8_features.py` 覆盖 Screw/RackPinion、subshape marker、multi/partial writeback、unsupported/missing grounded diagnostics 和 apply-next-request no-op；`cad-core/tests/test_adapters.py` 锁定 supported / unsupported matrix、placement writeback、fallback 和 diagnostic publication。S5 不新增 underconstrained / contradictory solver 语义 claim，后续若要扩展必须重新走 FreeCAD/Ondsel source、checked-in oracle、fixture/test 和 capability 同步。

S6 已完成，本主线队列关闭。最终 Worker / WASM / Web runtime 合同冻结为 `cad-core-result-v1`：CLI / C ABI / Worker / WASM / Web 共享 `results`、`elementReferenceUpdates`、`documentObjectUpdates`、`diagnostics`、`binaryPayloads` 通道；Worker / WASM 只增加 adapter 标记，adapter 只透传 core schema、capability 和 diagnostics，不实现 Link、Assembly、topo naming、subname、placement 或 ownership 业务。

最终 resource / binary 发布边界为：`mesh_limits` 支持 `max_vertices`、`max_triangles`、`chunk_triangles`，命中返回 `mesh_limit_exceeded`；无效 adapter limit 和 binary mesh `max_bytes` 超限返回 `adapter_resource_limit`；`cad_core_mesh_binary_json` 发布 `cad-core-binary-mesh-v1` metadata；`cad_core_export_json` 只返回 buffer + metadata 并拒绝服务端路径字段。当前不发布 timeout diagnostic、memory quota diagnostic、通用 import/export payload byte quota 或 streaming export，这些保留为 future / nonGoal。
