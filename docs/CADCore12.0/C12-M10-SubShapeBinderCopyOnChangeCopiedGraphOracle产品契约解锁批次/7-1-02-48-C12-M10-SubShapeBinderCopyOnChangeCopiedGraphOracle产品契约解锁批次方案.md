# C12-M10 SubShapeBinder CopyOnChange Copied Graph Oracle 产品契约解锁批次方案

## 目标

C12-M10 用 oracle-first / product-contract-first 的方式回答“CopyOnChange 是否终于能进入实现包”。它不把 live `remaining_gaps` 名称当作 C++ 任务，而是先补 C12-M8 缺失的 native copied graph artifact。

本包重新盘点：

- C12-M8 仍缺的 `_CopiedObjs` identity、`copyObject()` dependency order、support rewrite map、`recomputeFeature(true)` lifecycle 和 ElementMap / NamedShape lifecycle。
- C12-M9 `no_code_backlog_gate` 中没有 admitted mismatch-confirmed row 的事实。
- App::Link `documentObjectUpdates` transport 只能作为词汇参考，不能单独证明 SubShapeBinder `_tmp_binder` / `_CopiedObjs` lifecycle。
- current `cad-core` 对 `BindCopyOnChange=Enabled/Mutated` 和 `PartialLoad=True` 的 retained diagnostic 是否在 approved DTO 后形成 mismatch。

## 最终发布

C12-M10 S6 已发布 no-code retained 出口：S2=`native_oracle_blocked_retained`，S3=`dto_not_reviewed_due_to_native_blocker`，S4=`not_comparable` / `no_current_mismatch_retained_diagnostic`，S5=`oracle_blocked_retained` + `no_code_retained_diagnostic`。本批次不授权 implementation package，不创建后续实现包，不删除 known gap；`part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`、`known_gap_diagnostic` / `oracle_blocked` 和 `copy_on_change_full_temporary_document_cache_not_supported` 继续保留。

重开 / 删除条件固定为：新的 stable native copied graph evidence（`_CopiedObjs` identity/order、`copyObject()` mapping、support rewrite、`recomputeFeature(true)` lifecycle、ElementMap / NamedShape lifecycle）+ approved request-local copied graph DTO / product contract + mismatch-confirmed row 同时成立。

## 批次边界

本包只做 oracle、product-contract 和 implementation authorization 准入，不直接改 C++。

S5/S6 必须输出以下之一：

1. `implementation_package_authorized`：S2 native artifact、S3 DTO/product contract、S4 current mismatch 三项同时成立，并写清后续 implementation package 最小完整语义批次。
2. `product_contract_package_required`：native evidence 部分可用，但产品 DTO / contract 仍需另开批准包。
3. `oracle_blocked_retained`：FreeCAD native artifact 仍无法暴露 copied graph 核心证据。
4. `no_code_retained_diagnostic`：证据链未通过，继续保留 `copy_on_change_full_temporary_document_cache_not_supported`。

## 证据闸门

进入 implementation package 必须同时满足：

- FreeCAD source authority 可追溯到 `ShapeBinder.cpp`、`ShapeBinder.h`、`Document.cpp` 和必要的 `Link.cpp` reference vocabulary。
- Native artifact 能记录 stable copied object identity、dependency order、support rewrite、recompute status、ElementMap / NamedShape lifecycle，并区分 hidden session state 与 serializable graph evidence。
- DTO / product contract 只表达前端持久化 DocumentObject graph / `documentObjectUpdates`，不引入 backend session 或 native geometry cache。
- current comparison 在同一 request-local graph 下复现，不靠 wording、adapter repair、fixture 名、bbox、shape count、label 或 output order。

## 步骤安排

- 入口：确认包结构和矩阵入口。
- S0：冻结 current HEAD、dirty boundary、C12-M1..M9 队列、live capability snapshot 和 C12-M8/C12-M9 继承口径。
- S1：复核 FreeCAD source/current diagnostic/App::Link vocabulary，并固定 `c12m10.copy-on-change-native-copied-graph.v1` artifact schema。
- S2：运行 FreeCADCmd file-backed oracle，收集 mode matrix、single support gate、temporary binder lifecycle、copied object identities、dependency order、support rewrite、recompute status、ElementMap / NamedShape lifecycle、`PartialLoad` 与 `Cache_*` boundary。
- S3：基于 S2 evidence 裁决 DTO / product contract 字段：allowed、deferred、rejected。
- S4：比较 approved DTO / product contract 与 current retained diagnostic，判断 mismatch。
- S5：决定是否授权 implementation package，或发布 retained blocker / product-contract follow-up。
- S6：发布最终状态，更新 CADCore12.0 README、矩阵和队列。

## 初始判断

当前唯一 implementation-adjacent 项仍是 CopyOnChange，但 C12-M8 / C12-M9 已证明它不能直接进入实现。C12-M10 的最小完整语义批次必须覆盖完整 copied graph lifecycle，而不是单个 property、单个 fixture 或单个 `_CopiedLink` target。

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次 docs/CADCore12.0/README.md
git diff --check
```

Oracle / product-contract gate：

```bash
cd /Users/li/Chili3DProject/FreeCAD
cad-core/build/cad-core capabilities > /tmp/c12m10-capabilities.json
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次/工作步骤细分 --format markdown
```

重型收口只在 S5/S6 授权后续 implementation package 后执行；开包本身不跑 full build 或 full FreeCAD CI。

## 非目标

- 不直接实现 CopyOnChange full temporary document cache。
- 不刷新 checked-in expected。
- 不把 C12-M8 raw artifact 的 property / session state 升级成 copied graph success。
- 不把 App::Link transport 当作 SubShapeBinder support evidence。
- 不把 full BREP / TopoDS / persistent NamedShape / ElementMap cache 引入 CAD Core request-local 边界。
