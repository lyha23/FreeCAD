# CADCore4.0_New topoNamingState 完整状态记录方案包入口

本目录是新的 CAD Core topoNamingState 实现方案包。它不继承旧 `docs/CADCore4.0` 的专题包队列，也不把目标混入旧 C4-M4 ReferenceRecovery / TopoNamingPressure 压力回归包。

本包只围绕一个目标：把完整 `topoNamingState` 作为客户端携带的协议状态记录下来，并让 `cad-core` runtime 发布、校验、消费和回写的内容对齐 FreeCAD expected。

## 必读顺序

| 文件 | 用途 |
| --- | --- |
| `C4N-S1/7-9-09-19-CADCore4.0_New-topoNamingState完整状态记录总览方案.md` | C4N-S1 总览、边界、落点和验收分层 |
| `C4N-S1/矩阵/topo_state_scope.tsv` | C4N-S1 状态字段、代码落点、当前基线和目标 |
| `C4N-S1/矩阵/topo_state_fixture_matrix.tsv` | C4N-S1 fixture / expected / protocol contract 覆盖矩阵 |
| `C4N-S1/矩阵/topo_state_blocker_queue.tsv` | C4N-S1 实现队列和完成状态 |
| `C4N-S1/工作步骤细分/7-9-09-19-【已实现】C4N-S1-topoNamingState完整状态记录基线与exact-parity方案.md` | 已完成的第一批 c4m6 exact parity 步骤 |
| `C4N-S2/7-9-10-26-【已实现】C4N-S2-FreeCAD-ElementMap-MappedName-producer语义通用化方案.md` | 已完成的 p2/p6 producer mapped-name 通用化方案 |
| `C4N-S2/矩阵/c4n_s2_fixture_matrix.tsv` | C4N-S2 p2/p6 红线和 c4m6 回归守卫矩阵 |
| `C4N-S2/工作步骤细分/7-9-10-26-【已实现】C4N-S2-S1-p2p6-producer语义通用化实施步骤.md` | 已完成的 C4N-S2 可执行步骤 |
| `C4N-S3/7-9-11-01-C4N-S3-FreeCAD-mapped-name-producer-tag-deterministic-ledger方案.md` | 当前下一批 producer tag 跨入口确定性方案 |
| `C4N-S3/矩阵/c4n_s3_scope.tsv` | C4N-S3 范围、落点和验收边界 |
| `C4N-S3/矩阵/c4n_s3_blocker_queue.tsv` | C4N-S3 producer tag 与语料准入队列 |
| `C4N-S3/矩阵/c4n_s3_fixture_intake.tsv` | C4N-S3 expected corpus 准入分层矩阵 |
| `C4N-S3/工作步骤细分/7-9-11-01-C4N-S3-S1-producer-tag-deterministic-ledger实施步骤.md` | C4N-S3 第一批可执行步骤 |

## 执行规则

- `docs/接口规定/7-8-11-08-topoNamingState客户端携带状态接口方案.md` 是协议权威；本包负责实现拆分和验收组织。
- `DocumentObject graph` 仍是唯一建模事实；`topoNamingState` 只能参与旧引用恢复、diagnostics 和 update 建议。
- expected 以 `cad-core/fixtures/**/expected/*.freecad.json` 的 native FreeCADCmd 输出为权威；不得用当前 `cad-core` 输出反推 expected。
- C4N-S1 已聚焦并关闭 `cad-core/fixtures/c4m6`，覆盖首次状态、Body Tip child map、Compound child map、mapperHistory、hard-fail 和 ReferenceShadow 边界。
- C4N-S2 已关闭 p2 / p6 producer mapped-name 通用化红线；`test_topo_naming_state_response.py` 中对应 expectedFailure 已移除，c4m6 回归守卫保持通过。
- C4N-S3 当前聚焦 FreeCAD mapped-name producer tag deterministic ledger：先让 raw `:H...` tag 在 CLI / C API / worker / wasm 等入口之间确定一致，再按准入矩阵扩大 expected parity。
- 完成步骤后，按仓库文档规则把方案或步骤文件改名为 `【已实现】...`，再刷新矩阵和 README。
