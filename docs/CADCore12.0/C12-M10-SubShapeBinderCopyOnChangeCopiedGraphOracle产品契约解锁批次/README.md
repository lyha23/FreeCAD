# C12-M10 SubShapeBinder CopyOnChange Copied Graph Oracle 产品契约解锁批次

C12-M10 承接 C12-M9 的 `no_code_backlog_gate`，专门为当前唯一 live `remaining_gaps`：`part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache` 建立新的 oracle / product-contract 解锁包。

本包不是 C++ implementation 包。它只重新打开 CopyOnChange 的证据链：先采集更强 FreeCAD native copied graph artifact，再裁决哪些内容能进入前端持久化 DocumentObject graph / `documentObjectUpdates` 产品契约，最后才判断 current `cad-core` retained diagnostic 是否形成真实 mismatch。

## 当前基线

- 创建基线：`pwd=/Users/li/Chili3DProject/FreeCAD`。
- 创建基线：`HEAD=3c50dfccd8`（`3c50dfccd8 fix: 修复 Body 回放面引用法线解析`）。
- 创建时 `git -c core.quotepath=false status --short -uall` 无输出，worktree clean。
- C12-M1..M9 工作步骤队列均只输出 markdown 表头。
- live capability snapshot 保存到 `/tmp/c12m10-capabilities.json`。
- `part_design.sub_shape_binder.status=supported_c8m1_expected_backed_request_local_with_copy_on_change_known_gap`。
- `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- known gap 继续是 `status=known_gap_diagnostic`、`route=oracle_blocked`、`diagnostic=copy_on_change_full_temporary_document_cache_not_supported`。
- C12-M8 最终事实继续有效：S2=`native_evidence_retained_blocker`，S3=`dto_not_reviewed_due_to_native_blocker`，S4=`no_current_mismatch_retained_diagnostic`，S5=`no_code_retained_diagnostic`。
- C12-M9 最终事实继续有效：`no_code_backlog_gate`，没有 admitted mismatch-confirmed row，没有 implementation source / landing / validation surface。

## 入口关闭

- 工作步骤总入口已关闭：`工作步骤细分/7-1-02-49-【已实现】C12-M10工作步骤总入口.md`。已确认包结构、S0-S6 队列顺序、矩阵入口和 TSV 字段数；入口关闭时队列从 S0 继续，当前 S0 状态见下节。

## S0 live 冻结

- S0 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=75f7c231e2`（`75f7c231e2 文档：关闭 C12-M10 工作步骤总入口`）。
- S0 起点 dirty boundary：`git -c core.quotepath=false status --short -uall` 无输出，即 worktree clean。
- S0 执行前 C12-M10 队列第一项为 `7-1-02-50-C12-M10-S0-live基线与继承口径冻结.md`，后续为 S1-S6；S0 关闭后队列应从 S1 继续。
- C12-M1..M9 `工作步骤细分` 队列均只输出 markdown 表头，继承口径可作为 closed release gate 输入。
- live capability snapshot 保存到 `/tmp/c12m10-s0-capabilities.json`；`part_design.sub_shape_binder.status=supported_c8m1_expected_backed_request_local_with_copy_on_change_known_gap`。
- `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`，仍是唯一需要 C12-M10 处理的 CopyOnChange live gap。
- known gap 继续是 `status=known_gap_diagnostic`、`route=oracle_blocked`、`diagnostic=copy_on_change_full_temporary_document_cache_not_supported`。
- delete condition 继续继承 capability：只有 FreeCADCmd 暴露不依赖 persistent backend session 的 stable request-local CopyOnChange copied-object evidence 后，才能替换该 diagnostic。
- reopen condition 继续继承 capability：只有产品批准由更强 native oracle 支撑的 request-local CopyOnChange DTO 后，才重新打开实现判断。
- C12-M8 最终事实继续有效：S2=`native_evidence_retained_blocker`，S3=`dto_not_reviewed_due_to_native_blocker`，S4=`no_current_mismatch_retained_diagnostic`，S5=`no_code_retained_diagnostic`。
- C12-M9 最终事实继续有效：`no_code_backlog_gate`，没有 admitted mismatch-confirmed row，没有 implementation source / landing / validation surface。
- S0 不运行 FreeCADCmd，不做 source schema、native oracle、DTO approval、current mismatch 或 implementation authorization，不修改 production code、fixtures、expected、tests、adapters 或 capability source。

## 解锁目标

C12-M10 只有在以下三项同时成立时，才允许后续 implementation package：

1. Native copied graph evidence ready：FreeCADCmd artifact 能稳定暴露 `_CopiedObjs` identity、`copyObject()` dependency order、support rewrite map、`recomputeFeature(true)` lifecycle、ElementMap / NamedShape lifecycle，并能区分 `BindCopyOnChange=Enabled/Mutated`、single support gate、`PartialLoad` 与 `Cache_*` optimization boundary。
2. Product contract approved：批准字段只表达前端可持久化的 DocumentObject graph / `documentObjectUpdates`，不引入 backend session、temporary document handle、native pointer、full BREP / TopoDS、persistent `NamedShape` / `ElementMap` cache 或 post-request `_tmp_binder` / `_CopiedObjs` session state。
3. Current mismatch confirmed：current `cad-core` retained diagnostic 与 approved DTO / product contract 在同一 request-local graph 下存在真实 mismatch，且不能由 known gap wording 或 product-boundary rejection 解释。

任一项不成立，本包只能发布 `no_code_retained_diagnostic`、`oracle_blocked_retained` 或 `product_contract_required`，不得授权 C++。

## FreeCAD / CAD Core 依据

| 语义 | 依据 | C12-M10 用法 |
| --- | --- | --- |
| SubShapeBinder CopyOnChange 入口 | `src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::setupCopyOnChange()`，`BindCopyOnChange.getValue() == 0 || support.size() != 1` | S1/S2 必须覆盖 mode matrix 与 single-support gate。 |
| Mutated copied graph lifecycle | `src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::update()`，`newDocument("_tmp_binder")`、`copyObject({obj}, true, true)`、`_CopiedObjs.emplace_back`、`recomputeFeature(true)`、`_CopiedLink` | S2 native artifact 的核心字段。 |
| Hidden state / properties | `src/Mod/PartDesign/App/ShapeBinder.h`，`PartialLoad`、`BindCopyOnChange`、`_CopiedLink`、`_CopiedObjs` | S3 区分 input-only、deferred 和 rejected DTO 字段。 |
| copied dependency graph | `src/App/Document.cpp::Document::copyObject()`、`Document::recomputeFeature()` | S2 必须证明 copied object identity、dependency order、support rewrite 与 recompute lifecycle。 |
| App::Link reference vocabulary | `src/App/Link.cpp::LinkBaseExtension::*CopyOnChange*`、`cad-core/src/app/copy_on_change.cpp`、`cad-core/include/cad_core/app/copy_on_change.h` | 只作为 `documentObjectUpdates` 词汇参考，不能单独证明 SubShapeBinder 支持。 |
| current retained diagnostic | `cad-core/src/part_design/feature_shape_binder.cpp`、`cad-core/tests/test_c8_shapebinder.py`、`cad-core/src/runtime/capability_contract.cpp` | S4 比较 current retained diagnostic 与 approved DTO / product contract。 |

## 工作步骤

- 入口：确认 C12-M10 包结构、矩阵和队列入口。
- S0：live 基线、C12-M1..M9 关闭口径、capability snapshot 与 C12-M8/C12-M9 继承口径冻结（已完成，下一步从 S1 继续）。
- S1：FreeCAD source、current diagnostic、old artifacts 和 native probe schema 复核。
- S2：native copied graph oracle collection / evidence gate。
- S3：request-local DTO / product contract boundary 裁决。
- S4：current mismatch 与 implementation candidate gate。
- S5：implementation package authorization、oracle/product-contract 分流或 no-code retained 裁决。
- S6：发布闸门、root README 更新和后续分流。

## 入口

- 总入口：`7-1-02-48-C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次总入口.md`
- 方案：`7-1-02-48-C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 非目标

- 不直接实现 CopyOnChange full temporary document cache。
- 不在 adapter 中模拟 FreeCAD CopyOnChange。
- 不把 `_tmp_binder`、`_CopiedObjs`、temporary document handle 或 native object pointer 作为 backend / frontend 持久状态。
- 不传输 full BREP / TopoDS，也不建立 persistent `NamedShape` / `ElementMap` cache。
- 不用 property/session 状态、shape count、bbox、label、object name 或 `_CopiedLink` target 单独证明 copied graph 支持。
- 不重开 Groove、RuledSurface、ProjectOnSurface、Sweep、Filling、GeomPlate、Loft 或 Assembly 行。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次 docs/CADCore12.0/README.md
git diff --check
```
