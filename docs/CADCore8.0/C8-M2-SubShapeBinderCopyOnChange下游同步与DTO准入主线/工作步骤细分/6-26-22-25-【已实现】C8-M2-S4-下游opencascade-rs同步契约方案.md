# 【已实现】C8-M2-S4 下游 opencascade-rs 同步契约方案

## 目标

把 C8-M1 / C8-M2 的 FreeCAD 侧能力、fixtures、diagnostics、known_gap 和 protocol vocabulary 转成下游 `opencascade-rs` / 前端可消费的源头合同。S4 在本仓库只写文档和矩阵，不修改 Rust，不修改 C++。

## live 基线

- `pwd=/home/user/Chili3DProject/FreeCAD`
- `HEAD=7b4eec93fe`（`7b4eec93fe docs: 完成 C8-M2 S3 native 生命周期探针`）
- S4 开始时 `git -c core.quotepath=false status --short -uall` 为空，未发现无关 dirty 文件。
- 队列首项是本 S4 文件；S5/S6 仍 pending。

## 下游合同产物

- 新增 `矩阵/c8m2_downstream_sync_contract.tsv`：记录 `C8M2-SYNC-101..103` 的下游 owner、协议面、必同步内容、FreeCAD repo 边界和验证依据。
- 回写 `c8m2_copyonchange_oracle_plan.tsv`：`C8M2-SYNC-101..103` 均关闭为 `closed_S4_downstream_sync_contract`。
- 回写 `c8m2_copyonchange_blocker_queue.tsv`：`C8M2-BLOCKER-401` 关闭为 `closed_S4_downstream_sync_contract_has_owner_paths_validation_and_no_FreeCAD_Cpp_or_Rust_change`。
- 回写 `c8m2_copyonchange_non_goal_registry.tsv`：`C8M2-NG-005` 明确 Rust 实现属于另一个 `opencascade-rs` 包，本仓库只保留源头合同。

## 合同边界

| 合同 | 下游必须同步 | 仍不支持 |
| --- | --- | --- |
| `C8M2-SYNC-101` | `PartDesign::ShapeBinder`、`PartDesign::SubShapeBinder`、`PartDesign::SubShapeBinderPython` TypeIds；`/cad/capabilities` 中 `part_design.shape_binder.status=supported_c8m1_expected_backed_request_local`、`remaining_gaps=[]`；`part_design.sub_shape_binder.status=supported_c8m1_expected_backed_request_local_with_copy_on_change_known_gap`、`remaining_gaps=["copy_on_change_full_temporary_document_cache"]`；两者的 `covered` 列表。 | 不把 `copy_on_change_full_temporary_document_cache` 从 remaining gap 移除。 |
| `C8M2-SYNC-102` | `copy_on_change_full_temporary_document_cache_not_supported`、`known_gap_diagnostic`、`oracle_blocked`、`delete_condition`、`reopen_condition`；C8-M2 native probe 只能作为 property-state 和可见 `_tmp_binder` / `_CopiedLink` 证据。 | 不把 `_CopiedObjs`、`copyObject` dependency order、`recomputeFeature(true)` ElementMap lifecycle 当成稳定 request-local DTO。 |
| `C8M2-SYNC-103` | C8-M1 12 个 input fixture 与 12 个 FreeCAD expected；其中包括 CopyOnChange Disabled / Enabled / Mutated / PartialLoad property-state seed；`ElementMap` / `NamedShape` / maker history / full subname 输出作为前端 picking、reselect 和 reference update 的 request-local 合同。 | 不引入前端持久 BREP、TopoDS_Shape、NamedShape、ElementMap 或 temporary-document cache。 |

## C8-M1 blackbox 种子

下游 blackbox 同步必须成对消费 `cad-core/fixtures/c8m1/*.json` 与 `cad-core/fixtures/c8m1/expected/*.freecad.json`：

- `shape-binder-whole-box-cross-body`
- `shape-binder-face-edge-vertex-multi-subshape`
- `shape-binder-trace-support-placement`
- `shape-binder-datum-fallback-line-plane-point`
- `shape-binder-subshape-binder-element-map-namedshape-body-replay`
- `subshape-binder-basic-support-whole-face-edge-list`
- `subshape-binder-makeface-offset-fuse-refine`
- `subshape-binder-setlinks-normalization-diagnostics`
- `subshape-binder-relative-context-nested-route`
- `subshape-binder-profile-consumer-before-after-pad`
- `subshape-binder-bindmode-synchronized-frozen-detached`
- `subshape-binder-copy-on-change-disabled-enabled-mutated-partialload`

## C8-M2 native probe 使用边界

`cad-core/fixtures/c8m2/expected/subshape-binder-copyonchange-lifecycle-probe.freecad.json` 可以作为下游诊断和 product discussion 证据：它证明 Disabled / Enabled / Mutated、动态 CopyOnChange 属性、Enabled -> Mutated 触发、`PartialLoad=True`、`Support` 可见值以及 `_tmp_binder` / `_CopiedLink` 名称可被 native probe 观察。

它不能证明 full temporary-document cache 支持。`_CopiedObjs` private vector、`_tmp_binder copyObject` 对象图与依赖顺序、`copyObject` dependency mapping、`recomputeFeature(true)` 内部 copied-object ElementMap 生命周期都仍不可导出为稳定 request-local DTO。full temporary-document copied-object cache 继续是 `known_gap_diagnostic` / `oracle_blocked`。

## 前端输出合同

- `subshapes` 必须保留对象内完整 stable subname，供 picking 和 reselect 使用。
- `named_shapes[object].element_map`、`topo_history.maker_history=shapebinder/subshapebinder` 和 `producer_matrix.shapebinder.covered` 是 request-local reference update 证据。
- `documentObjectUpdates` 只能表达 request-local graph writeback，例如 `BindMode=Detached` 清 `Support`；不得要求前端或后端持久化 hidden FreeCAD temporary document、BREP、TopoDS_Shape、NamedShape 或 ElementMap cache。

## 下一步

进入 S5：发布 capability 协议与前端接入边界。S5 可以复核 `/cad/capabilities` wording 与 focused tests，但不能把 S4 下游同步合同改写成 FreeCAD `cad-core` 新 supported，也不能关闭 S6 implementation gate。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'C8M2-SYNC|opencascade-rs|/cad/capabilities|SubShapeBinderPython|copy_on_change_full_temporary_document_cache|ElementMap' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线 docs/CADCore8.0/README.md
git diff --check
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/工作步骤细分 --format markdown
git status --short -uall
```

本文件已按原时间前缀重命名为 `6-26-22-25-【已实现】C8-M2-S4-下游opencascade-rs同步契约方案.md`，索引链接同步更新。

## 非目标

- 不在 FreeCAD repo 修改 `opencascade-rs`。
- 不把下游同步状态写成 FreeCAD `cad-core` 新 supported。
- 不扩大到 GUI、Worker、WASM 或前端持久状态。
- 不改 C++。
- 不关闭 S5/S6 blocker。
