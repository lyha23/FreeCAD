# CADCore4.0_New topoNamingState 完整状态记录方案包入口

本目录是新的 CAD Core topoNamingState 实现方案包。它不继承旧 `docs/CADCore4.0` 的专题包队列，也不把目标混入旧 C4-M4 ReferenceRecovery / TopoNamingPressure 压力回归包。

本包只围绕一个目标：把完整 `topoNamingState` 作为客户端携带的协议状态记录下来，并让 `cad-core` runtime 发布、校验、消费和回写的内容对齐 FreeCAD expected。

## 必读顺序

| 文件 | 用途 |
| --- | --- |
| `7-9-09-19-CADCore4.0_New-topoNamingState完整状态记录总览方案.md` | 新方案总览、边界、落点和验收分层 |
| `矩阵/topo_state_scope.tsv` | 状态字段、代码落点、当前基线和目标 |
| `矩阵/topo_state_fixture_matrix.tsv` | fixture / expected / protocol contract 覆盖矩阵 |
| `矩阵/topo_state_blocker_queue.tsv` | 后续实现队列 |
| `工作步骤细分/7-9-09-19-C4N-S1-topoNamingState完整状态记录基线与exact-parity方案.md` | 第一批可执行步骤 |

## 执行规则

- `docs/接口规定/7-8-11-08-topoNamingState客户端携带状态接口方案.md` 是协议权威；本包负责实现拆分和验收组织。
- `DocumentObject graph` 仍是唯一建模事实；`topoNamingState` 只能参与旧引用恢复、diagnostics 和 update 建议。
- expected 以 `cad-core/fixtures/**/expected/*.freecad.json` 的 native FreeCADCmd 输出为权威；不得用当前 `cad-core` 输出反推 expected。
- 当前第一批验收聚焦 `cad-core/fixtures/c4m6`，因为它同时覆盖首次状态、Body Tip child map、Compound child map、mapperHistory、hard-fail 和 ReferenceShadow 边界。
- 完成步骤后，按仓库文档规则把步骤文件改名为 `【已实现】...`，再刷新队列。
