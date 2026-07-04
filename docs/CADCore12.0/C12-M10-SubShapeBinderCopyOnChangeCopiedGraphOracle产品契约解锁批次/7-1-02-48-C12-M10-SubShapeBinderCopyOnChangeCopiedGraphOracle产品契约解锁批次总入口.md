# C12-M10 SubShapeBinder CopyOnChange Copied Graph Oracle 产品契约解锁批次总入口

本文是 C12-M9 `no_code_backlog_gate` 之后的 CopyOnChange oracle / product-contract 解锁主线。

C12-M10 不直接落 C++。它只处理当前唯一 live `remaining_gaps`：`part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`，并且必须继承 C12-M8 / C12-M9 的限制：旧 native evidence 只到 property / session 层，尚未证明 `_CopiedObjs` identity、`copyObject()` dependency order、support rewrite map、`recomputeFeature(true)` lifecycle 或 ElementMap / NamedShape lifecycle 可稳定转成 request-local DTO。

## 主线目标

- 冻结 C12-M9 后的 live capability 和 dirty boundary。
- 设计并执行更强 FreeCAD native copied graph oracle。
- 裁决 CopyOnChange copied graph 哪些字段能进入 DocumentObject graph / `documentObjectUpdates` 产品契约。
- 只有 stable native/product evidence、request-local boundary 和 current mismatch 同时成立时，才授权后续 implementation package。
- 若证据仍缺失，发布 retained diagnostic、oracle blocker 或 product-contract follow-up，不改 C++。

## 当前基线

- 创建基线：`pwd=/Users/li/Chili3DProject/FreeCAD`。
- 创建基线：`HEAD=3c50dfccd8`（`3c50dfccd8 fix: 修复 Body 回放面引用法线解析`）。
- 创建时 worktree clean。
- C12-M1..M9 队列均只输出表头。
- live capability 仍发布 `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- known gap 仍是 `known_gap_diagnostic` / `oracle_blocked` / `copy_on_change_full_temporary_document_cache_not_supported`。
- C12-M8 / C12-M9 未授权 implementation package。

## S0 live 冻结

- S0 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=75f7c231e2`（`75f7c231e2 文档：关闭 C12-M10 工作步骤总入口`），起点 worktree clean。
- S0 确认 C12-M1..M9 队列均只输出 markdown 表头；C12-M10 队列关闭 S0 后应从 S1 继续。
- live capability snapshot 保存到 `/tmp/c12m10-s0-capabilities.json`。
- `part_design.sub_shape_binder.status=supported_c8m1_expected_backed_request_local_with_copy_on_change_known_gap`。
- `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- known gap 继续是 `status=known_gap_diagnostic`、`route=oracle_blocked`、`diagnostic=copy_on_change_full_temporary_document_cache_not_supported`。
- delete condition：只有 FreeCADCmd 暴露不依赖 persistent backend session 的 stable request-local CopyOnChange copied-object evidence 后，才能替换该 diagnostic。
- reopen condition：只有产品批准由更强 native oracle 支撑的 request-local CopyOnChange DTO 后，才重新打开实现判断。
- C12-M8 retained diagnostic 与 C12-M9 `no_code_backlog_gate` 继续作为 S1-S5 的输入约束；S0 不运行 FreeCADCmd，不授权 C++。

## 证明链条

```text
live capability baseline
  -> C12-M8/C12-M9 retained gate inheritance
  -> stronger native copied graph oracle schema
  -> FreeCADCmd artifact collection and evidence gate
  -> request-local DTO / product contract boundary
  -> current retained diagnostic mismatch gate
  -> implementation package authorization or retained no-code publication
```

## 当前执行状态

- 工作步骤总入口已关闭：已确认包结构、S0-S6 队列顺序、矩阵入口和 TSV 字段数；入口关闭时队列从 S0 继续，当前 S0 已关闭。
- S0 live 基线与继承口径冻结已关闭：已确认 HEAD、dirty boundary、C12-M1..M9 队列、SubShapeBinder capability snapshot、C12-M8 retained diagnostic 和 C12-M9 no-code backlog；后续队列从 S1 继续。
- S1 source 与 native oracle schema 复核已关闭：`ShapeBinder.cpp`、`ShapeBinder.h`、`Document.cpp`、`Link.cpp` 与 current `cad-core` retained diagnostic 已复核；`C12M10-PROBE-001..011` 已固定为 S2 artifact schema requirements，`C12M10-BLOCKER-101` 已关闭，后续队列从 S2 继续。

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| README | `README.md` | 当前定位、口径和入口。 |
| 方案 | `7-1-02-48-C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次方案.md` | 批次规则、证据链和验收分层。 |
| 工作步骤总入口 | `工作步骤细分/7-1-02-49-【已实现】C12-M10工作步骤总入口.md` | goal 队列索引，已关闭。 |
| S0 | `工作步骤细分/7-1-02-50-【已实现】C12-M10-S0-live基线与继承口径冻结.md` | 冻结 live baseline 和 C12-M8/C12-M9 继承口径，已关闭。 |
| S1 | `工作步骤细分/7-1-02-51-【已实现】C12-M10-S1-source与native-oracle-schema复核.md` | 复核 source/current evidence 并固定 probe schema，已关闭。 |
| S2 | `工作步骤细分/7-1-02-52-C12-M10-S2-native-copied-graph-oracle采集.md` | 采集 native copied graph artifact 并做 evidence gate。 |
| S3 | `工作步骤细分/7-1-02-53-C12-M10-S3-DTO与产品契约边界裁决.md` | 裁决 DTO / product contract 字段。 |
| S4 | `工作步骤细分/7-1-02-54-C12-M10-S4-current-mismatch准入.md` | 判断 current mismatch。 |
| S5 | `工作步骤细分/7-1-02-55-C12-M10-S5-implementation-authorization裁决.md` | 授权 implementation package 或 no-code / follow-up 分流。 |
| S6 | `工作步骤细分/7-1-02-56-C12-M10-S6-发布闸门与后续分流.md` | 发布最终状态。 |
| 矩阵 | `矩阵/` | source、scope、probe、DTO、contract、blocker、validation。 |

## 执行规则

1. 每步开始前执行 live baseline：`pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git -c core.quotepath=false status --short -uall`。
2. 每步执行前刷新 C12-M10 队列；只处理当前第一条未完成 step。
3. S0-S4 默认只改本包 docs / matrices / `docs/temp` oracle artifact，不改 `cad-core/src`、`include`、fixtures、expected、tests、adapters 或 capability source。
4. S2 可运行 FreeCADCmd native oracle，但 artifact 必须写入 `docs/temp`，不得刷新 checked-in expected。
5. S3 只能批准前端可持久化 graph / `documentObjectUpdates` 字段；temporary document、native pointer、full BREP / TopoDS、persistent `NamedShape` / `ElementMap` cache 和 post-request session state 必须拒绝。
6. 只有 S5 同时确认 native/product evidence、request-local DTO / product contract 和 current mismatch，才允许创建或指向后续 implementation package。
7. 每步完成后重命名为 `【已实现】` 并更新 README / 总入口 / 矩阵中对应状态。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次 docs/CADCore12.0/README.md
git diff --check
```
