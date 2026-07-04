# 【已实现】C12-M15 S2 ledger interface 产品契约设计

## 目标

发布 `SketchGeometryIdentityLedger` 的 request-local interface、字段契约、fallback / diagnostic 规则和前后端消费边界。

## live 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD`：`499616ab4c`。
- `git log -1 --oneline`：`499616ab4c docs: 关闭 C12-M15 S1 identity 管线复核`。
- `git -c core.quotepath=false status --short -uall`：无输出。

父进程给出的 `HEAD=499616ab4c` 与本地命令一致；本步起点 worktree clean。

## 必读文件

- `../README.md`
- `../7-4-15-44-C12-M15-SketchStableGeometryIdMappedNameLedger设计批次方案.md`
- `../7-4-15-44-C12-M15-SketchStableGeometryIdMappedNameLedger设计批次总入口.md`
- `7-4-15-46-【已实现】C12-M15-S0-live基线与C12-M11继承冻结.md`
- `7-4-15-47-【已实现】C12-M15-S1-FreeCAD-source与current-identity管线复核.md`
- `../矩阵/c12m15_sketch_geometry_id_source_matrix.tsv`
- `../矩阵/c12m15_sketch_geometry_id_contract_matrix.tsv`
- `../矩阵/c12m15_sketch_geometry_id_scope_matrix.tsv`
- `../矩阵/c12m15_sketch_geometry_id_non_goal_registry.tsv`
- `../矩阵/c12m15_sketch_geometry_id_blocker_queue.tsv`
- `../矩阵/c12m15_sketch_geometry_id_validation_matrix.tsv`
- S1 已实现后的 step 文档和矩阵更新。

## Ledger interface

`SketchGeometryIdentityLedger` 是 request-local 产品账本，不是 backend sketch session。它可以沿用或包装当前 `RawSketchEdgeIdentityLedger`，但对外只暴露同一套 source identity 到 current indexed edge 的映射。

| 类别 | 契约 |
| --- | --- |
| Inputs | 当前请求的 sketch Geometry list、每个 geometry 的 `geometryIndex`、`geometryKind`、合法唯一 `geometryId` / 缺失 id 状态、raw source edge 构建结果、raw `EdgeN` / internal `InternalEdgeN` indexed name、`internal_element_map` 的 raw alias、旧引用里的 `stableSubname` / `sourceStableSubname` / `sourceGeometryId` / `sourceGeometryKind`。 |
| Outputs | 每个 current indexed edge 的 `sourceGeometryIndex`、`sourceGeometryId`、`sourceGeometryKind`、`sourceStableSubname`、对外 durable `stableSubname`、`identityStatus`、needs-reselect / diagnostic status，以及供 reference resolution 使用的 `byIndexed` / `byStableSubname` 视图。 |
| Invariants | `EdgeN` / `InternalEdgeN` 只是当前请求 indexed name；只有 `identityStatus=stable` 且有 mapped `g<ID>` / 后续外部 `e<ID>` 时才能发布可持久 `stableSubname`；fallback 不得伪装成 stable；response publisher、raw identity object 和 reference update 必须消费同一个 ledger snapshot。 |
| Lifetime | ledger 只在一次 recompute 请求内存在；它不能保存 TopoDS、NamedShape、ElementMap、BREP 或 FreeCAD `geoHistory` session state。FreeCAD `updateGeoHistory()` / `generateId()` 只作为输入 id 来源与语义依据，不是 cad-core 自动复用 id 的当前契约。 |

## 状态契约

| 状态 | 产品语义 | 对外字段 |
| --- | --- | --- |
| `stable` | 当前 edge 能从合法唯一 `geometryId` 回到 source geometry，且 current edge 与 source identity 一致。 | 发布 `sourceGeometryId=<ID>`、`sourceStableSubname=g<ID>`、`stableSubname=g<ID>`、`identityStatus=stable`；前端可持久化 `g<ID>`。 |
| `index_fallback` | input 没有 geometry id 或无法产生 FreeCAD-grade mapped name，但当前请求仍可展示 / 拾取。 | 可发布 `sourceGeometryIndex` 和 `sourceStableSubname=index:N` 作为调试 / 单请求证据；不得发布 durable `stableSubname`，前端必须视为需重新选择的非稳定 token。 |
| `invalid_geometry_id` / `duplicate_geometry_id` | input id 非正整数或重复，不能进入 stable ledger。 | parser diagnostic 早于 ledger 发布；不得降级成看似 stable 的 `EdgeN`。 |
| `deleted_stable_subname` / `needs_reselect` | 旧 `g<ID>` 在当前 ledger 的 `byStableSubname` 中不存在。 | reference update 报 deleted / needs_reselect，不用 bbox、mesh 顺序或新 `EdgeN` 猜测替代。 |
| `geometry_kind_changed` | 旧 `sourceGeometryKind` 与当前同 id geometry kind 不一致，可能是 Line -> Arc 等语义漂移。 | reference update 保留当前 `sourceGeometryId` / `sourceStableSubname`，同时报 kind drift，交给调用方重选或确认。 |
| `split_requires_reselect` | 一个 source edge 在当前请求拆成多个 fragment，且没有正式 fragment ledger / ElementMap 证据。 | 不自动把任一 fragment 声明为原 edge 的稳定延续；S3 只能把它归类为 implementation target、explicit diagnostic 或后续非目标，不能用 mesh/source 顺序补猜。 |

## Response contract

- `rawSketchEdgeIdentity` 是 ledger 的结构化发布形态，至少包含 `byIndexed` 与 `byStableSubname` 两个消费视图。
- `mesh.edgeSegments[]` 必须从同一 ledger materialize `sourceGeometryId`、`sourceGeometryIndex`、`sourceGeometryKind`、`sourceStableSubname`、`stableSubname` 和 `identityStatus`。
- `subshapes[]` 必须使用同一 ledger：stable 时发布 mapped `stableSubname=g<ID>`；`index_fallback` 时清空或不发布 durable `stableSubname`。
- `elementReferenceUpdates` / reference shadow update 必须用同一 `byStableSubname` 做旧 `g<ID>` 到当前 indexed edge 的解析，输出 deleted / needs_reselect / kind drift，而不是自己按 prefix、mesh segment 顺序或 `EdgeN` 顺序重新发明 identity。
- `InternalEdgeN` 只有在 `internal_element_map` 能追溯到 raw source-backed `EdgeN` 时才能继承 raw identity；WireJoiner / split / fragment history 不完整时保持 diagnostic 或 fallback，不扩大成稳定承诺。

## 前端消费边界

前端只消费后端发布字段：`id`、`subname`、`stableSubname`、`sourceGeometryId`、`sourceGeometryIndex`、`sourceStableSubname`、`sourceGeometryKind`、`identityStatus` 与 reference update diagnostic。前端不得靠 `InternalEdge` / `Edge` prefix、mesh segment 顺序、subshape 数组顺序或当前 `EdgeN` 名称发明长期 topology identity。`my-chili3d` consumer sync 是独立包；本步只定义 FreeCAD repo 内后端 response contract。

## 本步改动边界

- 更新 S2 文档、C12-M15 README / 方案 / 总入口 / root README 当前状态，以及 source / scope / contract / non-goal / blocker / validation 矩阵。
- 未修改 C++、fixtures、expected、tests、adapters 或 capability source。
- 未运行 FreeCADCmd、build 或重型回归。

## 关闭条件

- ledger interface 可以作为后续 C++ 实现 seam。
- 所有 fallback 和 diagnostic 状态都有产品语义。
- 不把 `EdgeN` 顺序稳定性伪装成 FreeCAD-grade stable id。
- S3 可以直接按本契约裁决 current coverage、implementation_needed 或 no-code 收口。

## 非目标

- 不实现 C++。
- 不新增 fixtures、expected、tests 或 adapters。
- 不运行 FreeCADCmd、build 或重型回归。
- 不处理 open wire mesh 产品契约。
- 不处理前端 consumer sync。
- 不裁决 S3 current coverage。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次 docs/CADCore12.0/README.md
git diff --check
```
