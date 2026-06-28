# CADCore10.0

CADCore10.0 承接 CADCore9.0 队列全部关闭后的下一轮 CAD Core 收口工作。C9-M1 到 C9-M5 已关闭 Assembly request-local solver / DistanceType default rows / SubShapeBinder CopyOnChange DTO 准入复审；当前 live capability 中唯一非空 `remaining_gaps` 仍是 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`，但 C9-M5 已裁定为 `no_code_retained_known_gap_release_gate`，不是 CADCore10.0 默认实现入口。

C10-M1 转向 P5b Sketch open-wire / WireJoiner / InternalFace stable selector。目标不是重开 Sketcher solver，也不是继续做 CopyOnChange 准入，而是沿 FreeCAD `SketchObject::buildInternals()` 同一调用链，把近切线、重合边、复杂 open-wire 与 `Profile.StableSubList=InternalFaceN` without `ReferenceShadow` 的 request-local 证据、ElementMap 和发布边界拆清。

C10-M2 转向 PartDesign DressUp / Hole topo history 第二阶段。current capability 已显示 DressUp / Hole producer matrix 为 first slice 且无 active `remaining`，所以本包不是从 remaining gap 硬开 C++，而是复核 FreeCAD `DressUp::getAddSubShape()`、Fillet / Chamfer / Draft / Thickness、`Hole::findHoles()` / `makeThread()` 与 current cad-core `NamedShape` / `ElementMap` / MapperHistory 是否还有 source-backed mismatch。只有 S3-S5 证明 mismatch 时，S6 才打开实现；否则发布 no-code release gate。

## 入口

- C10-M1 总入口：`C10-M1-SketchOpenWireInternalFaceStableSelector批次/6-28-17-33-C10-M1-SketchOpenWireInternalFaceStableSelector批次总入口.md`
- C10-M1 方案：`C10-M1-SketchOpenWireInternalFaceStableSelector批次/6-28-17-33-C10-M1-SketchOpenWireInternalFaceStableSelector批次方案.md`
- C10-M1 工作步骤：`C10-M1-SketchOpenWireInternalFaceStableSelector批次/工作步骤细分/`
- C10-M1 矩阵：`C10-M1-SketchOpenWireInternalFaceStableSelector批次/矩阵/`
- C10-M2 总入口：`C10-M2-PartDesignDressUpHoleTopoHistory批次/6-28-22-53-C10-M2-PartDesignDressUpHoleTopoHistory批次总入口.md`
- C10-M2 方案：`C10-M2-PartDesignDressUpHoleTopoHistory批次/6-28-22-53-C10-M2-PartDesignDressUpHoleTopoHistory批次方案.md`
- C10-M2 工作步骤：`C10-M2-PartDesignDressUpHoleTopoHistory批次/工作步骤细分/`
- C10-M2 矩阵：`C10-M2-PartDesignDressUpHoleTopoHistory批次/矩阵/`

## 当前状态

- S0 live 基线已冻结：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=382539f170`（`382539f170 docs: 关闭 C9-M5 S6 发布闸门`）。S0 起始 `git -c core.quotepath=false status --short -uall` 仅显示 C10-M1 seed 文档、矩阵和 `docs/CADCore10.0/README.md` 未跟踪；无 `cad-core/src`、fixtures、expected 或 tests 改动。
- C9-M5 队列复核为空；`copy_on_change_full_temporary_document_cache` 仍由 live capability 发布为 `known_gap_diagnostic` / `oracle_blocked` 和 `remaining_gaps=["copy_on_change_full_temporary_document_cache"]`，只作为 retained known gap，不进入 C10-M1。
- C10-M1 S2 scope 准入已完成：执行基线为 `HEAD=b53dd572ad`（`b53dd572ad docs: 完成 C10-M1 S1 源码覆盖矩阵审计`），`C10M1-BLOCKER-201` 已关闭为 `closed_s2`；S3/S4/S5/S6 owner step 已在矩阵中明确，S2 未采 oracle、未改 C++。
- C10-M1 S6 发布闸门已完成：执行基线为 `HEAD=e409342850`（`e409342850 docs: 完成 C10-M1 S5 stable selector 复审`），裁决为 existing-code no-code release gate；S3/S4 不重开 FaceMaker / WireJoiner C++ gate，S5 的 without-ReferenceShadow `StableSubList=InternalFaceN` 只发布 request-local `Sketch.InternalShape` `NamedShape` / `ElementMap` 唯一解析路径；`C10M1-BLOCKER-601` 已关闭为 `closed_s6`，C10-M1 队列为空。
- `docs/CADCore方案/细化方案/12-P5b-Sketch-open-wire-WireJoiner完整迁移方案.md` 已把下一步定为：用 FreeCAD oracle 固定近切线、重合边、复杂 open-wire case；不可证明的一对多 open-wire history 保持 stable diagnostic；若支持 `Profile.StableSubList=InternalFaceN` without `ReferenceShadow`，必须先补 FreeCAD evidence 和 ElementMap-backed stable selector。
- current `cad-core` 已有 `profile_resolver.cpp` 的 explicit `InternalFaceN` profile selection、ReferenceShadow-backed recovery、open profile diagnostic，以及 `wire_joiner.cpp` / `internal_shape_history_publisher.cpp` 的 recoverable WireJoiner child-wire / MapperHistory / ElementMap 子集；C10-M1 只在 S3-S5 证据闭环后扩大这些正式路径。
- 禁止声明：CopyOnChange、full Sketcher solver、GUI、cross-request cache、raw `FaceN` alias、source index / split order / bbox / 面积 / 输出排序 stable selector 均不是 C10-M1 supported 范围。
- C10-M2 S0 live 基线已冻结：起点 HEAD=`2420e3b842`（`docs: 完成 C10-M1 S6 发布闸门`），起点 dirty boundary 仅包含 C10-M2 docs / matrices 和 `docs/CADCore10.0/README.md` 的 in-scope 变更；`C10M2-BLOCKER-000=closed_s0`，`C10M2-SCOPE-001=baseline_frozen_s0` 仅作为 S6 复核的 docs-only release baseline。C10-M2 S1 已复核 `C10M2-SRC-101..204` 的 live FreeCAD / current cad-core/tests path、symbol 和 concise evidence，`C10M2-BLOCKER-101=closed_s1`。C10-M2 S2 已完成 scope 准入，`C10M2-BLOCKER-201=closed_s2`，所有 scope 均有合法 `current_status`、owner `next_step` 和 `close_condition`，且没有 `backend_gap_requires_implementation`。C10-M2 S3 已完成 DressUp producer-history 复审，`C10M2-SCOPE-101=expected_backed_no_gap`，`C10M2-SCOPE-102=no_gap`，`C10M2-BLOCKER-301=closed_s3`，`C10M2-CAT-101=no_gap`；S3 未发现 expected-backed current mismatch，未改 C++、tests、fixtures、expected 或 capability。C10-M2 S4 已完成 Hole producer-history 复审，`C10M2-SCOPE-201=expected_backed_no_gap`，`C10M2-SCOPE-202=expected_backed_no_gap`，`C10M2-BLOCKER-401=closed_s4`，`C10M2-CAT-102=no_gap`；S4 未发现 expected-backed current mismatch，未改 C++、tests、fixtures、expected 或 capability。C10-M2 S5 已完成 cross-feature old-reference / diagnostic boundary 复审，`C10M2-SCOPE-301=diagnostic_retained`，`C10M2-BLOCKER-501=closed_s5`，`C10M2-CAT-103=diagnostic_retained`；S5 确认 split / deleted / merge 经 Body boolean、transformed copy、Link retag 只发布 retained diagnostics，stale `ReferenceShadow` / Base recovery 仍缺 native observable `ShadowSub` / `ReferenceShadow` evidence。current capability 显示 `part_design.hole.history.status=element_map_freeze_first_slice`、`topo_history.producer_matrix.dressup.status=done_first_slice`、`topo_history.producer_matrix.hole.status=done_first_slice`；C10-M2 没有新的 evidence-backed implementation row。
- C10-M2 S6 发布闸门已完成：执行基线为 `HEAD=49bb78adf3`（`docs: 完成 C10-M2 S5 旧引用诊断复审`），裁决为 docs-only no-code release gate；`C10M2-SCOPE-401=release_closed`、`C10M2-BLOCKER-601=closed_s6`、`C10M2-CAT-104=release_closed`。C10-M2 队列为空；未修改 `cad-core/src`、tests、fixtures、expected 或 capability；stale `ReferenceShadow` / Base recovery 继续保持 `diagnostic_retained` / oracle-blocked，不声明 supported。
- C10-M2 禁止声明：raw `FaceN`、bbox、面积、顺序、source index、fixture 名称、adapter 层修剪或输出排序不能用来选 face / edge；stale `ReferenceShadow` / Base recovery 继续保持 oracle-blocked / diagnostic，不能发布为 supported。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore10.0/C10-M2-PartDesignDressUpHoleTopoHistory批次/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M2-PartDesignDressUpHoleTopoHistory批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0
git diff --check
```
