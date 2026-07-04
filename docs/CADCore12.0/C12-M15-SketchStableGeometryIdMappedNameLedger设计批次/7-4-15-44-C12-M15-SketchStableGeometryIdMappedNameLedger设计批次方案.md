# C12-M15 Sketch stable geometry id / mapped-name ledger 设计批次方案

## 背景

C12-M11 已证明 closed internal profile 的后端 response 当前支持：`mesh.edgeSegments[]` 和 `subshapes[]` 都能返回 `Sketch:InternalEdgeN`，并有 request-local `stableSubname=EdgeN`。但 C12-M11 同时明确：`EdgeN` 只是当前请求的 indexed name，不足以证明跨编辑稳定。

C12-M15 只做 stable geometry id / mapped-name ledger 设计。它不是继续 C12-M14 helper lifecycle，也不是 open wire mesh 产品契约。

## 核心设计

把 sketch edge identity 收敛到一个深模块：

| 项 | 设计 |
| --- | --- |
| Module | `SketchGeometryIdentityLedger` |
| Seam | sketch raw/internal edge 生成完成后、`NamedShape` / `mesh.edgeSegments[]` / `subshapes[]` 发布前 |
| Interface | 输入当前 raw shape、source edges、source geometry identities；输出 indexed edge 到 source geometry id / stable mapped name / fallback status 的账本 |
| Implementation | 内部处理 TopExp edge 枚举、source edge matching、geometry id mapping、fallback、diagnostics 和 response field materialization |
| Callers | sketch executor、runtime response、reference resolution、future frontend consumer |

对调用方来说，账本只回答：

1. `EdgeN` / `InternalEdgeN` 当前对应哪个 `sourceGeometryId`。
2. 是否能发布 FreeCAD-grade `stableSubname=g<ID>`。
3. 如果不能稳定，为什么只能 `index_fallback`。
4. 旧 `StableSubList` / `ReferenceShadow.sourceGeometryId` 在本次请求中应该更新到哪个 current indexed edge。

## 字段契约草案

| 字段 | 含义 | 发布条件 |
| --- | --- | --- |
| `indexed` | 当前请求内 `EdgeN` / `InternalEdgeN` | 总是发布，表示当前 shape 枚举名。 |
| `sourceGeometryIndex` | 当前 input Geometry list 的索引 | 有 source geometry 时发布，只能作为调试 / fallback 证据。 |
| `sourceGeometryId` | FreeCAD-style geometry extension id | input 有合法唯一 id 且 edge 能映射回 source geometry 时发布。 |
| `sourceStableSubname` | `g<ID>` 或 fallback token | 有 `sourceGeometryId` 时为 `g<ID>`；无 id 时只能是 `index:N` 这类非稳定证据。 |
| `stableSubname` | 对外可持久引用的稳定名 | 只有 `identityStatus=stable` 时发布 `g<ID>`；fallback 时必须为空或不用于持久引用。 |
| `identityStatus` | `stable` / `index_fallback` / 后续 diagnostic status | 表示引用是否能跨编辑延续。 |
| `sourceGeometryKind` | 源 geometry 类型 | 用于检测 Line -> Arc 等语义漂移。 |

## 行为规则

- 有唯一合法 `geometryId` 且 source edge 能映射到 current edge：发布 `stableSubname=g<ID>`，`identityStatus=stable`。
- 没有 `geometryId`：发布 `identityStatus=index_fallback`，不得把 `EdgeN` 作为长期 stable id。
- `geometryId` 重复或非法：输入解析阶段报 diagnostic，不进入 stable ledger。
- 旧引用的 `sourceGeometryId` 还存在但 current `sourceGeometryKind` 变化：reference resolution 输出 `geometry_kind_changed`。
- 旧引用的 `sourceStableSubname=g<ID>` 在当前账本中找不到：输出 deleted / needs reselect，不靠 bbox 或 mesh 顺序猜。
- raw edge 与 source edge 一对多、一对零或 split 场景必须显式分类；未设计清楚前不得把 split 后任一 fragment 自动声明为原 edge 的稳定延续。

## 最小完整语义批次

设计批次应覆盖同一条 source/current 管线上的代表场景，而不是只写一个 fixture：

1. raw sketch edge with geometry id：`EdgeN` 输出 `g<ID>`。
2. closed internal profile：`InternalEdgeN` 能追溯到 raw `g<ID>`，但不重开 C12-M11 response contract。
3. geometry list reorder：`EdgeN` 改名后仍能通过 `g<ID>` 解析。
4. inserted/deleted geometry：旧 `EdgeN` 顺序漂移时，reference resolution 不用顺序猜测。
5. invalid/duplicate/missing id：明确 diagnostic 或 fallback。
6. geometry kind drift：同一 id 的类型变化需要 diagnostic。

如果 S1/S2 证明当前代码已覆盖其中一部分，S3 只授权缺失项的最小实现包；不能因为已有字段存在就跳过账本契约。

## 工作步骤

- 入口：核对包结构、矩阵、步骤队列。
- S0：冻结 live 基线、C12-M11 / C12-M14 队列状态、capability remaining gap 为空的事实。
- S1：复核 FreeCAD `updateGeoHistory()` / `generateId()` / `convertSubName()` / `getEdge()` 和 cad-core current identity 管线。
- S2：发布 ledger interface、字段契约、fallback / diagnostic 规则和 non-goal。
- S3：比较 current coverage 与 S2 契约，裁决是否需要后续 C++ implementation package。
- S4：发布设计结果，更新 root README、矩阵和验证记录。

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次 docs/CADCore12.0/README.md
git diff --check
```

阶段回归候选：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p5_features
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

重型收口候选：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests/test_mvp.py
```

## 非目标

- 不在设计批次直接落大规模 C++ 改造。
- 不把 `EdgeN` 顺序稳定性升级为 FreeCAD-grade identity。
- 不用 mesh 点线几何相似性恢复身份。
- 不处理 open wire mesh 产品契约。
- 不处理前端 token consumer sync。
