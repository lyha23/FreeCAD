# C8-M2 SubShapeBinder CopyOnChange 下游同步与 DTO 准入方案

## 背景

C8-M1 已完成 `ShapeBinder` / `SubShapeBinder` 后端支持，但它明确把 `BindCopyOnChange` 的 full FreeCAD temporary-document copied-object cache 保持为 `known_gap` / `oracle_blocked`。这个状态不能直接写成 supported，也不能通过后端 session 绕过无状态 CAD Core 边界。

C8-M2 选择先做准入和同步，而不是直接写 C++：

- 下游需要知道 C8-M1 的 type ids、fixtures、capability、diagnostics 和 ElementMap / NamedShape 结果合同。
- CopyOnChange 只有在 FreeCAD native evidence 能证明 request-local DTO 子集时，才值得进入 C++ implementation gate。

## 原则

- 不重开 C8-M1 已关闭 executor / ElementMap 主路径。
- 不把 `copy_on_change_full_temporary_document_cache` 写成 supported。
- 不引入跨请求 backend session、temporary FreeCAD document、BREP、TopoDS_Shape、NamedShape 或 ElementMap cache。
- 下游同步方案可以在 FreeCAD repo 写源头合同，但 Rust 实现必须另在 `opencascade-rs` 执行。

## 最小完整语义批次

| 批次 | 范围 | 预期处理 |
| --- | --- | --- |
| C8-M1 capability sync | ShapeBinder / SubShapeBinder status、covered、remaining_gaps、diagnostics | 下游同步合同 |
| Fixture / expected sync | `cad-core/fixtures/c8m1` 12 个输入和 expected | 下游 parity / blackbox 种子 |
| CopyOnChange property state | Disabled / Enabled / Mutated / PartialLoad Python-visible state | S3 已用 native probe 复核 |
| CopyOnChange copied cache | temporary document / copied object cache / mutation lifecycle | S3 观察到 `_tmp_binder` / `_CopiedLink`，但 full cache 仍 retained `oracle_blocked` |
| Request-local DTO | 仅传递 product-approved graph updates / diagnostics | S6 可实现候选 |
| GUI / backend session | TaskPanel、ViewProvider、persistent session | non-goal |

## S0 live 基线与范围冻结

已完成。S0 冻结 C8-M1 已关闭状态、当前 capability 输出、C8-M2 的禁止声明和状态词典：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=dc93b0d3af`（`dc93b0d3af chore: 完成 C8-M1 S6 发布闸门`）。C8-M1 队列为空；`part_design.shape_binder.remaining_gaps=[]`；`part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]` 且 `known_gap.status=known_gap_diagnostic`、`route=oracle_blocked`。

S0 不采 oracle，不改 C++，不声明 CopyOnChange full temporary-document cache supported，不修改 Rust 下游。S0 之后只有下游同步行是 `sync_required`，CopyOnChange request-local DTO 仍是 `backend_gap_candidate` / `oracle_candidate`，full temporary-document cache 仍是 `known_gap_retained`。

## S1 FreeCAD source 与 C8-M1 能力复核

已完成。S1 live 基线为 `HEAD=e7e07663d9`（`e7e07663d9 docs: 完成 C8-M2 S0 live 基线冻结`），开始工作区干净。S1 复核了 `ShapeBinder.cpp` 的 `setupCopyOnChange()`、`checkCopyOnChange()`、`onChanged()`、`update()` 中 CopyOnChange / PartialLoad / Mutated 行为，以及 `LinkBaseExtension::setupCopyOnChange()` 对照、C8-M1 capability/tests/fixtures 和 current diagnostic 边界。

S1 结论：FreeCAD `Mutated` full path 依赖 `_CopiedObjs`、`"_tmp_binder"` temporary document、`copyObject()` 和 `recomputeFeature()`；cad-core 当前只发布 request-local recompute + `copy_on_change_full_temporary_document_cache_not_supported` diagnostic。C8-M2 仍不采 oracle、不改 C++，`copy_on_change_full_temporary_document_cache` 继续是 known_gap。

## S2 DTO 准入与 oracle 候选矩阵

已完成。S2 live 基线为 `HEAD=73a5acf8a8`（`73a5acf8a8 docs: 完成 C8-M2 S1 源码与能力复核`），开始工作区干净。S2 把 S1 的 source / current evidence 转成 `oracle_candidate`、`sync_required`、`known_gap_retained`、`backend_gap_candidate` 和 `diagnostic_non_goal`，并关闭 `C8M2-BLOCKER-201`。

S2 结论：C8-M1 ShapeBinder / SubShapeBinder capability、fixtures、expected 和 diagnostics 是下游同步合同；CopyOnChange Disabled / Enabled / Mutated property-state 与 PartialLoad allow-partial 进入 S3 native probe；full temporary-document copied-object cache 继续保持 `known_gap_retained`；request-local DTO 只是 `backend_gap_candidate`，不是 S6 implementation gate；GUI/session/persistent cache/Rust 下游属于 `diagnostic_non_goal`。S2 不采 oracle、不新增 fixture/expected/tests/collector、不改 C++ 或 Rust。

## S3 native CopyOnChange 生命周期探针

已完成。S3 新增 `cad-core/tools/probe_c8m2_subshapebinder_copyonchange.py`、`cad-core/fixtures/c8m2/subshape-binder-copyonchange-lifecycle-probe.json` 和 `cad-core/fixtures/c8m2/expected/subshape-binder-copyonchange-lifecycle-probe.freecad.json`。FreeCADCmd 采集 `freecad_version=1.2.0 revision 20260519`；Disabled / Enabled / Mutated、动态 CopyOnChange 属性和 Enabled -> Mutated 触发均可 Python-visible；`PartialLoad=True` 与 `Support` 的 `AllowPartial` / `ReadOnly` 状态可见；但 `_CopiedObjs` private vector、`copyObject` dependency order 和 `recomputeFeature(true)` 生命周期不可作为稳定 request-local DTO 导出，所以 full temporary-document cache 继续 retained `oracle_blocked`。

## S4 下游 opencascade-rs 同步契约

形成下游同步清单：TypeId registry、document graph parser、feature executor / DTO、capability snapshot、fixture blackbox、diagnostics 和 known_gap vocabulary。S4 在本 repo 只写合同，不改 Rust。

## S5 capability 协议与前端接入边界

同步 FreeCAD `cad-core` capability 发布口径，明确哪些是 supported、known_gap、diagnostic_non_goal 和下游 product contract。

## S6 实现准入与发布闸门

若 S6 明确接受 S3 property-state 子集为 request-local CopyOnChange DTO，S6 指定 C++ landing 和 focused tests；若不能证明 product-approved DTO，则关闭为 no-code release gate，并给出下游同步或下一轮 oracle probe 任务。full temporary-document copied-object cache 仍不得写成 supported。

## 验收分层

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/矩阵/*.tsv
git diff --check
```

实现短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics
```

阶段回归只在 S6 打开 C++ implementation gate、修改 `feature_shape_binder.cpp`、`copy_on_change.cpp`、capability schema 或 shared reference/update path 时执行。
