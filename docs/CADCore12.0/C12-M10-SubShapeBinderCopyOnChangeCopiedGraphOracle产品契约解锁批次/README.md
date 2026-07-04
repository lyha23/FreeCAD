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

## S1 source 与 native oracle schema 复核

- S1 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=acf501ab6b`（`acf501ab6b 文档：冻结 C12-M10 S0 live 基线`），起点 worktree clean。
- FreeCAD `SubShapeBinder::setupCopyOnChange()` 在 `BindCopyOnChange.getValue() == 0 || support.size() != 1` 时清理 CopyOnChange dynamic property 并返回；只有 exactly one support 且非 Disabled 时才接入 `LinkBaseExtension::setupCopyOnChange()`。
- FreeCAD `SubShapeBinder::update()` 的 Mutated 路径会创建 temporary `_tmp_binder`，执行 `copyObject({obj}, true, true)`，填充 `_CopiedObjs`，先 `recomputeFeature(true)` 生成 geometry element map，再 copy property、必要时再次 recompute，并写 `_CopiedLink`。
- `ShapeBinder.h` 明确定义 `PartialLoad`、`BindCopyOnChange`、`_CopiedLink`、`_CopiedObjs`；S3 必须区分可持久化输入字段和禁止跨 request 保存的 hidden/session state。
- `Document::copyObject()` 通过 dependency list、exportObjects/importObjects 复制对象；`Document::recomputeFeature(feature, true)` 递归 recompute 并返回有效性。
- `LinkBaseExtension` 与 cad-core `app/copy_on_change` 只是 App::Link / `documentObjectUpdates` 参考词汇，不证明 SubShapeBinder `_tmp_binder` / `_CopiedObjs` 已支持。
- current cad-core 对 `BindCopyOnChange=Enabled|Mutated` 或 `PartialLoad=True` 仍保留 `copy_on_change_full_temporary_document_cache_not_supported`；S4 只有在 S2/S3 给出 approved request-local DTO 后才比较 mismatch。
- S1 已关闭 `C12M10-BLOCKER-101`，`C12M10-VAL-101=passed_s1_source_schema_fixed`；S2 继续采集 native copied graph oracle，不能把 schema fixed 当作 native evidence ready。

## S2 native copied graph oracle 采集

- S2 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=965915be73`（`965915be73 文档：关闭 C12-M10 S1 source schema 复核`），起点 worktree clean。
- 本轮使用 `/Users/li/.cargo/bin/freecadcmd` 运行 file-backed probe；FreeCAD / OCCT baseline 为 FreeCAD `1.2.0 revision 20260519`、OCCT `7.8.1`。
- 新增 native probe 脚本：`docs/temp/c12m10-subshapebinder-copy-on-change-native-copied-graph-probe.py`。
- Raw artifact：`docs/temp/c12m10-subshapebinder-copy-on-change-native-copied-graph.raw.freecad.json`；gate artifact：`docs/temp/c12m10-subshapebinder-copy-on-change-native-copied-graph-evidence-gate.json`，schema 均为 `c12m10.copy-on-change-native-copied-graph.v1`。
- Artifact 覆盖 `C12M10-PROBE-001..011`：baseline、mode matrix、single support gate、tmp binder lifecycle、`_CopiedObjs` identity 尝试、copyObject dependency order 尝试、support rewrite / `_CopiedLink`、recompute lifecycle 尝试、ElementMap / NamedShape lifecycle 尝试、`PartialLoad` 与 `Cache_*` boundary。
- S2 裁决：`native_oracle_blocked_retained`。Artifact 可观察 `_tmp_binder` 文档/object order、`_CopiedLink` target/subvalues、`PartialLoad` property 和 `Cache_*` dynamic matrix cache，但仍不能稳定导出 `_CopiedObjs` stored identity/order、`Document::copyObject()` dependency list 与 source-to-import mapping、first/second `recomputeFeature(true)` lifecycle、ElementMap / NamedShape per-stage lifecycle。
- `C12M10-BLOCKER-201` 已关闭为 retained blocker；`C12M10-VAL-201=passed_s2_native_oracle_blocked_retained`。S3 必须继承该 blocker，不得从 property/session 状态、temporary document name、label/bbox/shape count 或 `_CopiedLink` target 单独批准 DTO / implementation。

## S3 DTO 与产品契约边界裁决

- S3 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=acb2457753`（`acb2457753 文档：关闭 C12-M10 S2 native oracle`），起点 worktree clean。
- S3 继承 S2 裁决：`native_oracle_blocked_retained`。S2 artifact 覆盖 `C12M10-PROBE-001..011`，但 `_CopiedObjs` stored identity/order、`copyObject()` dependency order/mapping、内部 `recomputeFeature(true)` lifecycle、ElementMap / NamedShape 分阶段 lifecycle 仍未稳定导出。
- `C12M10-DTO-001..004` 和 `C12M10-DTO-006` 关闭为 `deferred_native_oracle_blocked`：copied object create、property writeback、link rewrite、support sublist rewrite 与 `PartialLoad` 不能从 property/session evidence 或 `_CopiedLink` target 单独批准为执行 DTO。
- `C12M10-DTO-005` 只批准为 `allowed_input_only_no_execution_support`：`BindCopyOnChange` 可保留为 input-only request field，但不表示 CopyOnChange execution、temporary document 或 copied graph 支持。
- `C12M10-DTO-007..012` 关闭为 `rejected_product_boundary`：temporary document handle、native pointer、full BREP / TopoDS、persistent `NamedShape` / `ElementMap` cache、post-request `_tmp_binder` / `_CopiedObjs` state、backend `Cache_*` persistence 均不得进入产品契约。
- `C12M10-CONTRACT-001..003` 保持 deferred / output-suggestions-only，`C12M10-CONTRACT-004` 保留 current diagnostic，`C12M10-CONTRACT-005` 仅作 App::Link `documentObjectUpdates` reference vocabulary，`C12M10-CONTRACT-006` 保持 forbidden session/geometry state rejection。
- `C12M10-CAT-002` 已关闭为 `dto_not_reviewed_due_to_native_blocker`，current mismatch 仍 `not comparable`；`C12M10-BLOCKER-301` 已关闭，`C12M10-VAL-301=passed_s3_dto_not_reviewed_due_to_native_blocker`。S4 不得在没有 approved execution DTO 的情况下认定 current mismatch。

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
- S1：FreeCAD source、current diagnostic、old artifacts 和 native probe schema 复核（已完成，下一步从 S2 继续）。
- S2：native copied graph oracle collection / evidence gate（已完成，`native_oracle_blocked_retained`）。
- S3：request-local DTO / product contract boundary 裁决（已完成，`dto_not_reviewed_due_to_native_blocker`）。
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
