# 【已实现】C12-M15 S3 current gap 与最小实现边界裁决

## 目标

对照 S2 ledger 契约和当前 cad-core coverage，裁决是否需要后续 C++ implementation package，以及最小完整语义批次应覆盖哪些场景。

## live 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD`：`86749f91cb`。
- `git log -1 --oneline`：`86749f91cb 文档：发布 C12-M15 S2 ledger 产品契约`。
- `git -c core.quotepath=false status --short -uall`：无输出。

父进程给出的 `HEAD=86749f91cb` 与本地命令一致；本步起点 worktree clean。

## 必读文件

- `../README.md`
- `../7-4-15-44-C12-M15-SketchStableGeometryIdMappedNameLedger设计批次方案.md`
- `../7-4-15-44-C12-M15-SketchStableGeometryIdMappedNameLedger设计批次总入口.md`
- `7-4-15-47-【已实现】C12-M15-S1-FreeCAD-source与current-identity管线复核.md`
- `7-4-15-48-【已实现】C12-M15-S2-ledger-interface产品契约设计.md`
- `../矩阵/c12m15_sketch_geometry_id_contract_matrix.tsv`
- `../矩阵/c12m15_sketch_geometry_id_scope_matrix.tsv`
- `../矩阵/c12m15_sketch_geometry_id_blocker_queue.tsv`
- `../矩阵/c12m15_sketch_geometry_id_validation_matrix.tsv`
- `../矩阵/c12m15_sketch_geometry_id_non_goal_registry.tsv`
- `cad-core/tests/test_p5_sketch.py`
- `cad-core/include/cad_core/sketcher/sketch_edge_identity.h`
- `cad-core/src/sketcher/sketch_edge_identity.cpp`
- `cad-core/src/sketcher/sketch_object_geometry.cpp`
- `cad-core/src/runtime/recompute.cpp`
- `cad-core/src/runtime/reference_resolution.cpp`

## 操作

1. 检查当前 cad-core 是否已覆盖合法 geometry id 发布、fallback、invalid/duplicate id diagnostic 和 reference resolution。
2. 对每个 contract row 判定 `current_supported`、`implementation_needed`、`design_only` 或 `blocked`。
3. 如果需要实现，定义后续最小完整语义批次：reorder、insert/delete、kind drift、missing id fallback、internal edge source mapping。
4. 明确不把前端 consumer sync 或 open wire mesh contract 塞进本实现包。
5. 更新 scope / contract / blocker / validation 矩阵。
6. 将本步骤重命名为 `【已实现】`。

## current coverage 裁决

S3 结论：当前 coverage 足够 no-code 收口；不授权后续 C++ implementation package。本步没有修改 `cad-core/src`、fixtures、expected、tests 或 adapters。

逐行裁决如下：

| contract | status | S3 裁决 |
| --- | --- | --- |
| C12M15-CONTRACT-001 | `current_supported` | 唯一正整数 `geometryId` 已发布 `sourceGeometryId` / `sourceStableSubname=g<ID>`；`test_p5_open_wire_edge_identity_publishes_geometry_ids` 覆盖 edgeSegments 与 subshapes。 |
| C12M15-CONTRACT-002 | `current_supported` | `applyIdentityFields()` 只在 stable 时写 durable `stableSubname`，`responseSubshapes()` 在 `index_fallback` 清空 durable stableSubname。 |
| C12M15-CONTRACT-003 | `current_supported` | `InternalEdgeN` 只通过 `internal_element_map` / raw alias 继承 raw geometry identity；closed/internal 与 mixed internal open case 已有 focused tests。 |
| C12M15-CONTRACT-004 | `current_supported` | 缺少 geometry id 时为 `identityStatus=index_fallback`，只保留 `sourceGeometryIndex` / `sourceStableSubname=index:N` 这类当前请求证据。 |
| C12M15-CONTRACT-005 | `current_supported` | parser 对 invalid / duplicate geometry id 早退报错；duplicate 有 focused test，invalid path 由 `readGeometryIdField()` 与 `invalid_geometry_id` diagnostic 代码证据支持。 |
| C12M15-CONTRACT-006 | `current_supported` | 插入 / 重排后通过 `raw_edge_identity.byStableSubname` 把旧 `g<ID>` 解析到当前 `EdgeN`，不靠 `EdgeN` 顺序。 |
| C12M15-CONTRACT-007 | `current_supported` | 带 `ReferenceShadow` 的旧 raw edge `g<ID>` 缺失时报 `deleted_stable_subname`，不重绑到同几何新 id；普通 App::Link 无 shadow 的 `unsupported_stable_subname` 不扩成 S3 frontend consumer 同步任务。 |
| C12M15-CONTRACT-008 | `current_supported` | reference shadow resolution 比对旧 / 当前 `sourceGeometryKind`，kind drift 报 `geometry_kind_changed`。 |
| C12M15-CONTRACT-009 | `design_only` | split fragment durable identity 不在 C12-M15 自动支持范围；当前已有 mapper / reference-shadow split reselect 诊断，本包只保留“不自动继承”的产品边界，不新建 fragment ledger。 |
| C12M15-CONTRACT-010 | `design_only` | 前端 consumer sync 是 my-chili3d 独立包；本仓库只发布后端字段和消费边界。 |
| C12M15-CONTRACT-011 | `current_supported` | `rawSketchEdgeIdentity`、mesh edgeSegments、subshapes 和 reference resolution 均消费当前 raw ledger 的 `byIndexed` / `byStableSubname` 视图。 |
| C12M15-CONTRACT-012 | `current_supported` | ledger 由单次 recompute 的 parsed geometry/source edges/materialized response 生成，不保存 backend sketch session、TopoDS、NamedShape、ElementMap、BREP 或 mesh 跨请求状态。 |

## no-code 边界

- 本步不需要后续最小 C++ implementation package。
- 不新增 fixtures / expected / tests / adapters；已有 focused tests 与源码证据足以支撑 S3 current coverage 裁决。
- `split_requires_reselect` 只作为“不自动继承 fragment stable identity”的产品边界；若未来需要 durable fragment identity，必须另开 fragment ledger / ElementMap 包，先定义 source one-to-many fragment id、响应字段和引用更新规则。
- 不把 my-chili3d consumer sync 或 C12-M11 open wire mesh product contract 塞进本包。

## focused evidence

- `test_p5_open_wire_edge_identity_publishes_geometry_ids`
- `test_p5_open_wire_edge_identity_survives_insert_before`
- `test_p5_open_wire_stable_sublist_resolves_geometry_id`
- `test_p5_mixed_internal_open_edge_identity_matches_source_geometry`
- `test_p5_open_wire_reference_shadow_refreshes_geometry_id_update`
- `test_p5_open_wire_reference_shadow_reports_geometry_kind_drift`
- `test_p5_open_wire_reference_shadow_deleted_geometry_id_does_not_hit_new_edge`
- `test_p5_open_wire_reference_shadow_deleted_geometry_id_same_shape_new_id_does_not_rebind`
- `test_p5_sketch_rejects_duplicate_geometry_id`
- `test_p5_open_sketch_keeps_raw_shape_without_profile_face`

上述 10 个 `tests.test_p5_sketch.CadCoreP5SketchTest` focused tests 已在 S3 执行并通过。S3 step 原始验证命令 `python3 -m unittest tests.test_p5_features` 当前失败，因为仓库没有 `cad-core/tests/test_p5_features.py`，实际相关 module 是 `tests.test_p5_sketch`；该失败已写入 validation matrix，不伪造成通过。

## 关闭条件

- 每个 contract row 已裁决为 `current_supported` 或 `design_only`，无 `implementation_needed` / `blocked`。
- no-code 收口已明确：不授权 C++、fixtures、expected、tests 或 adapters。
- 后续仅进入 S4 设计发布闸门；若未来要 durable split fragment identity，应另开独立实现包。

## 非目标

- 不直接实现后续 C++。
- 不新增 fixtures/expected/tests/adapters。
- 不扩展到所有 sketch solver / constraint identity。
- 不处理前端 consumer sync。
- 不处理 C12-M11 open wire raw edge mesh 产品契约。
- 不处理 S4 发布闸门。
- 不运行全量 FreeCAD build。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p5_features
cd ..
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M15-SketchStableGeometryIdMappedNameLedger设计批次/矩阵/*.tsv
git diff --check
```
