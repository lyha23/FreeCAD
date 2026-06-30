# C12-M8 S3 request-local DTO 产品边界裁决【已实现】

## 目标

裁决 S2 native copied graph evidence 中哪些字段可以进入 CAD Core request-local 产品契约，哪些字段必须保留为 forbidden backend/session state。

## 必读文件

- `../矩阵/c12m8_copy_on_change_dto_contract_fields.tsv`
- `../矩阵/c12m8_copy_on_change_non_goal_registry.tsv`
- `../矩阵/c12m8_copy_on_change_backend_gap_classification.tsv`
- `docs/接口规定/01-cad-recompute全量输入输出接口.md`
- `cad-core/include/cad_core/app/copy_on_change.h`
- `cad-core/src/app/copy_on_change.cpp`
- `cad-core/src/adapters`

## 操作

1. 若 S2 未输出 `native_copied_graph_evidence_ready`，S3 直接关闭为 `dto_not_reviewed_due_to_native_blocker`。
2. 若 S2 成立，只允许 request graph / `documentObjectUpdates` 表达 copied object 创建、属性写回、链接重写和前端持久化 graph mutation。
3. 禁止字段包括 temporary document handle、native object pointer、TopoDS、BREP、full object shape snapshot、persistent `NamedShape`、persistent `ElementMap`、backend cache key、post-request `_tmp_binder` 或 `_CopiedObjs` session state。
4. 将每个 DTO 字段标为 `approved`、`rejected`、`needs_product_decision` 或 `deferred_to_implementation_package`。
5. 更新 backend_gap_classification：只有 DTO 批准后才允许进入 current mismatch gate。

## S3 结论

- 本轮 baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`git rev-parse --short HEAD=557c5be617`，`git log -1 --oneline=557c5be617 文档：关闭 C12-M8 S2 native evidence gate`，起点 `git -c core.quotepath=false status --short -uall` 无输出。
- S3 执行前队列确认：`6-30-21-29-C12-M8-S3-request-local-DTO产品边界裁决.md` 是第一条 pending，后续为 S4-S6。
- S2 输出是 `native_evidence_retained_blocker`，不是 `native_copied_graph_evidence_ready`，因此 S3 按步骤规则直接关闭为 `dto_not_reviewed_due_to_native_blocker`。
- `docs/接口规定/01-cad-recompute全量输入输出接口.md` 继续固定边界：`Objects[]` 是唯一持久输入，`documentObjectUpdates[]` 只是前端可选择应用到 `Objects[]` 的对象级 graph 写回建议，不表示后端保存 graph/session。
- `TopoDS_Shape`、full BREP、mesh、subshape map、`NamedShape`、`ElementMap` 都是单次 recompute 运行态产物；除既有 `ReferenceShadow.brep` 单 subshape 例外外，不得进入 CopyOnChange 请求或响应 DTO。
- `cad-core/include/cad_core/app/copy_on_change.h` 与 `cad-core/src/app/copy_on_change.cpp` 只提供 App::Link `documentObjectUpdates` vocabulary：copied object create/update、copied-subtree link rewrite、group sync、historyPreserve 和 link writeback；它是 S3 字段命名参考，不等同 SubShapeBinder `_tmp_binder` / `_CopiedObjs` 支持。
- `cad-core/src/adapters/cli/cli.cpp`、`main.cpp` 与 `cad-core/src/adapters/c_api/c_api.cpp` 仍只暴露 request-local recompute / export adapter 边界，不承接 CopyOnChange、Link rewrite、NamedShape、ElementMap 或 backend session 业务语义；S3 不改 adapter 协议。

## DTO 字段裁决

- `C12M8-DTO-001..004`：copied object create、copied property writeback、copied link rewrite、copied support sublist 都属于未来可能允许的 frontend-persisted DocumentObject graph / `documentObjectUpdates` 方向，但本轮因 S2 缺 stable copied identity、dependency order、support rewrite map 和 ElementMap lifecycle evidence，统一裁决为 `deferred`。
- `C12M8-DTO-005`：`BindCopyOnChange` enum 只裁决为 `approved` input-only。它可以作为前端持久化 `Objects[]` 里的可见输入属性存在，但不批准 `Enabled/Mutated` 执行支持，不打开 implementation gate。
- `C12M8-DTO-006`：`PartialLoad` 仍裁决为 `deferred`。该字段可见，但 `PartialLoad=True` 的 native copied graph / support lifecycle evidence 不足，current diagnostic 继续保留。
- `C12M8-DTO-007..012`：temporary document handle、native object pointer、full BREP / TopoDS、persistent `NamedShape` / `ElementMap` cache、post-request `_tmp_binder` / `_CopiedObjs` session state、backend `Cache_*` 均裁决为 `rejected`。
- `C12M8-BLOCKER-301` 已关闭为 `closed_s3_dto_not_reviewed_due_to_native_blocker`。

## 后续限制

- S4 只能继承 `native_evidence_retained_blocker` 与 `dto_not_reviewed_due_to_native_blocker`，做 retained diagnostic / wording drift 判断；不得把 App::Link transport、property/session 状态、label、bbox、shape count、temporary document name 或 `_CopiedLink` target 当作 implementation approval 证据。
- S5 不能授权 CopyOnChange full implementation package，除非先用更强 native copied graph artifact 重开并通过 S2，再重新执行 S3 DTO approval。
- 删除 retained diagnostic 的条件仍是：FreeCADCmd artifact 稳定暴露 `_CopiedObjs` identity、copied dependency order、support rewrite map、`recomputeFeature(true)` lifecycle、ElementMap / NamedShape lifecycle，并且这些 evidence 能转成前端持久化 graph / `documentObjectUpdates`。

## 关闭条件

- `C12M8-DTO-001..012` 均有裁决：已完成。
- `C12M8-BLOCKER-301` 关闭：已完成。
- S3 输出：`dto_not_reviewed_due_to_native_blocker`。

## 非目标

- 不为了通过 DTO 审核而新增 backend session。
- 不把 full BREP / TopoDS / NamedShape / ElementMap cache 放进请求或响应。
- 不改 adapter 协议。
- 不改 `cad-core/src`、`include`、fixtures、expected、tests、adapters 或 capability source。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次/矩阵/*.tsv
git diff --check
```
