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
| CopyOnChange property state | Disabled / Enabled / Mutated / PartialLoad Python-visible state | S3 native probe 复核 |
| CopyOnChange copied cache | temporary document / copied object cache / mutation lifecycle | 默认 `oracle_blocked`，除非 S3 有新证据 |
| Request-local DTO | 仅传递 product-approved graph updates / diagnostics | S6 可实现候选 |
| GUI / backend session | TaskPanel、ViewProvider、persistent session | non-goal |

## S0 live 基线与范围冻结

已完成。S0 冻结 C8-M1 已关闭状态、当前 capability 输出、C8-M2 的禁止声明和状态词典：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=dc93b0d3af`（`dc93b0d3af chore: 完成 C8-M1 S6 发布闸门`）。C8-M1 队列为空；`part_design.shape_binder.remaining_gaps=[]`；`part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]` 且 `known_gap.status=known_gap_diagnostic`、`route=oracle_blocked`。

S0 不采 oracle，不改 C++，不声明 CopyOnChange full temporary-document cache supported，不修改 Rust 下游。S0 之后只有下游同步行是 `sync_required`，CopyOnChange request-local DTO 仍是 `backend_gap_candidate` / `oracle_candidate`，full temporary-document cache 仍是 `known_gap_retained`。

## S1 FreeCAD source 与 C8-M1 能力复核

复核 `ShapeBinder.cpp` 的 CopyOnChange / PartialLoad 调用链、`LinkBaseExtension` 对照、C8-M1 capability/tests/fixtures 和 current diagnostic 边界。S1 不采 oracle，不改 C++。

## S2 DTO 准入与 oracle 候选矩阵

把 S1 的 source / current evidence 转成 `oracle_candidate`、`sync_required`、`known_gap_retained`、`diagnostic_non_goal` 和 possible implementation gate。S2 不能直接打开 C++ implementation gate。

## S3 native CopyOnChange 生命周期探针

尝试用 FreeCADCmd / native probe 观察 `BindCopyOnChange` copied-object cache、Mutated lifecycle、PartialLoad allow-partial 行为。若不可观察，固化 blocker evidence 和 delete/reopen condition。

## S4 下游 opencascade-rs 同步契约

形成下游同步清单：TypeId registry、document graph parser、feature executor / DTO、capability snapshot、fixture blackbox、diagnostics 和 known_gap vocabulary。S4 在本 repo 只写合同，不改 Rust。

## S5 capability 协议与前端接入边界

同步 FreeCAD `cad-core` capability 发布口径，明确哪些是 supported、known_gap、diagnostic_non_goal 和下游 product contract。

## S6 实现准入与发布闸门

若 S3 证明 request-local CopyOnChange DTO 可实现，S6 指定 C++ landing 和 focused tests；若不能证明，则关闭为 no-code release gate，并给出下游同步或下一轮 oracle probe 任务。

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
