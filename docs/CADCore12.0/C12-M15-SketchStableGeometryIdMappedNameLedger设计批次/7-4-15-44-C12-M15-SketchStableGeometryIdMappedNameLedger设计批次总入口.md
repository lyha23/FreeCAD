# C12-M15 Sketch stable geometry id / mapped-name ledger 设计批次总入口

## 目标

承接 C12-M11 S4/S5 的 `C12-M11-StableGeometryIdMappedNameLedger设计批次` follow-up，为 sketch raw/internal edges 设计 request-local `geometryId -> g<ID> -> current EdgeN/InternalEdgeN` 账本。

## 必读文件

- `README.md`
- `7-4-15-44-C12-M15-SketchStableGeometryIdMappedNameLedger设计批次方案.md`
- `矩阵/c12m15_sketch_geometry_id_source_matrix.tsv`
- `矩阵/c12m15_sketch_geometry_id_scope_matrix.tsv`
- `矩阵/c12m15_sketch_geometry_id_contract_matrix.tsv`
- `矩阵/c12m15_sketch_geometry_id_blocker_queue.tsv`
- `矩阵/c12m15_sketch_geometry_id_non_goal_registry.tsv`
- `矩阵/c12m15_sketch_geometry_id_validation_matrix.tsv`
- `docs/CADCore12.0/README.md`

## 队列顺序

1. `7-4-15-45-C12-M15工作步骤总入口.md`
2. `7-4-15-46-【已实现】C12-M15-S0-live基线与C12-M11继承冻结.md`
3. `7-4-15-47-【已实现】C12-M15-S1-FreeCAD-source与current-identity管线复核.md`
4. `7-4-15-48-【已实现】C12-M15-S2-ledger-interface产品契约设计.md`
5. `7-4-15-49-【已实现】C12-M15-S3-current-gap与最小实现边界裁决.md`
6. `7-4-15-50-C12-M15-S4-设计发布闸门.md`

## 当前状态

- 包结构、矩阵和 S0-S4 队列文件已创建；工作步骤总入口已关闭。
- S0 live 基线与 C12-M11 继承冻结已关闭：`HEAD=b3d2df945a`（`b3d2df945a docs: 关闭 C12-M15 工作步骤总入口`），起点 worktree clean。
- C12-M11 与 C12-M14 队列均为空；live capability 递归检查确认所有 `remaining_gaps` 均为空，`known_gaps` 为空数组 / 空对象。
- C12-M15 起点是 C12-M11 stable geometry id ledger 设计 follow-up，不是 capability remaining gap，也不是 C12-M14 helper lifecycle 后续；不在总入口直接修改 C++。
- S1 FreeCAD source 与 current identity 管线复核已关闭：FreeCAD authority 是 `GeometryFacade` extension id 与 mapped `g<ID>` / `e<ID>`，`GeoId` / `EdgeN` 只代表当前索引；cad-core current landing 已定位到 parser、raw ledger、response publisher 和 reference resolution consumer。
- S2 ledger interface 产品契约已关闭：`SketchGeometryIdentityLedger` 是 request-local 产品账本，输入当前请求 geometry/source edge/internal alias/old reference shadow，输出 `byIndexed`、`byStableSubname`、stable/fallback/diagnostic 状态和 response fields；`mesh.edgeSegments[]`、`subshapes[]`、`rawSketchEdgeIdentity`、`elementReferenceUpdates` 必须共享同一账本来源。
- S3 current gap 与最小实现边界裁决已关闭：001..008、011、012 为 `current_supported`，009 split fragment durable identity 与 010 frontend consumer boundary 为 `design_only`；当前 coverage 足够 no-code 收口，不授权后续 C++ implementation package。
- 后续 worker 从 S4 继续，只做设计发布闸门，不把前端 consumer sync 或 open wire mesh contract 塞回本包。

## 执行规则

- 每次只处理队列中的第一个未实现步骤，完成后刷新队列。
- `EdgeN` / `InternalEdgeN` 只作为当前请求 indexed name；不能把顺序稳定性写成 FreeCAD-grade stable id。
- `geometryId` / `sourceGeometryId` / `sourceStableSubname` / `identityStatus` 必须作为同一账本的字段统一裁决。
- 如果没有合法 geometry id，只能发布 `index_fallback` 或空 `stableSubname`，不得让前端误以为引用可跨编辑稳定。
- `deleted_stable_subname` 必须等价于 needs-reselect；`geometry_kind_changed` 必须显式返回；缺少 fragment ledger / ElementMap 证据时 split 场景不能自动继承 stable identity，应输出 `split_requires_reselect` 或进入 S3 implementation target。
- 前端只能消费后端发布字段，不得靠 `Edge` / `InternalEdge` prefix、mesh 顺序、subshape 顺序或当前 `EdgeN` 发明长期 topology identity。
- 不修改 `my-chili3d`；前端消费同步另包处理。

## 关闭条件

- 队列可由 `step_goal_queue.py` 读出 S0-S4，并按文件名顺序推进。
- TSV 字段数检查通过。
- README、方案、总入口、矩阵与 root README 均指向同一 C12-M15 设计范围。
- blocker queue 中每个未关闭 blocker 都有明确 reopen / next action。

## 非目标

- 不在总入口实现 C++。
- 不重开 C12-M11 closed profile backend contract。
- 不处理 open wire raw edge mesh 产品契约。
- 不引入 persistent backend session 或几何缓存。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次 docs/CADCore12.0/README.md
git diff --check
```
