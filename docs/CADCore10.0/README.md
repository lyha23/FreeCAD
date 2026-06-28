# CADCore10.0

CADCore10.0 承接 CADCore9.0 队列全部关闭后的下一轮 CAD Core 收口工作。C9-M1 到 C9-M5 已关闭 Assembly request-local solver / DistanceType default rows / SubShapeBinder CopyOnChange DTO 准入复审；当前 live capability 中唯一非空 `remaining_gaps` 仍是 `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`，但 C9-M5 已裁定为 `no_code_retained_known_gap_release_gate`，不是 CADCore10.0 默认实现入口。

C10-M1 转向 P5b Sketch open-wire / WireJoiner / InternalFace stable selector。目标不是重开 Sketcher solver，也不是继续做 CopyOnChange 准入，而是沿 FreeCAD `SketchObject::buildInternals()` 同一调用链，把近切线、重合边、复杂 open-wire 与 `Profile.StableSubList=InternalFaceN` without `ReferenceShadow` 的 request-local 证据、ElementMap 和发布边界拆清。

## 入口

- C10-M1 总入口：`C10-M1-SketchOpenWireInternalFaceStableSelector批次/6-28-17-33-C10-M1-SketchOpenWireInternalFaceStableSelector批次总入口.md`
- C10-M1 方案：`C10-M1-SketchOpenWireInternalFaceStableSelector批次/6-28-17-33-C10-M1-SketchOpenWireInternalFaceStableSelector批次方案.md`
- C10-M1 工作步骤：`C10-M1-SketchOpenWireInternalFaceStableSelector批次/工作步骤细分/`
- C10-M1 矩阵：`C10-M1-SketchOpenWireInternalFaceStableSelector批次/矩阵/`

## 当前状态

- S0 live 基线已冻结：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=382539f170`（`382539f170 docs: 关闭 C9-M5 S6 发布闸门`）。S0 起始 `git -c core.quotepath=false status --short -uall` 仅显示 C10-M1 seed 文档、矩阵和 `docs/CADCore10.0/README.md` 未跟踪；无 `cad-core/src`、fixtures、expected 或 tests 改动。
- C9-M5 队列复核为空；`copy_on_change_full_temporary_document_cache` 仍由 live capability 发布为 `known_gap_diagnostic` / `oracle_blocked` 和 `remaining_gaps=["copy_on_change_full_temporary_document_cache"]`，只作为 retained known gap，不进入 C10-M1。
- `docs/CADCore方案/细化方案/12-P5b-Sketch-open-wire-WireJoiner完整迁移方案.md` 已把下一步定为：用 FreeCAD oracle 固定近切线、重合边、复杂 open-wire case；不可证明的一对多 open-wire history 保持 stable diagnostic；若支持 `Profile.StableSubList=InternalFaceN` without `ReferenceShadow`，必须先补 FreeCAD evidence 和 ElementMap-backed stable selector。
- current `cad-core` 已有 `profile_resolver.cpp` 的 explicit `InternalFaceN` profile selection、ReferenceShadow-backed recovery、open profile diagnostic，以及 `wire_joiner.cpp` / `internal_shape_history_publisher.cpp` 的 recoverable WireJoiner child-wire / MapperHistory / ElementMap 子集；C10-M1 只在 S3-S5 证据闭环后扩大这些正式路径。
- 禁止声明：CopyOnChange、full Sketcher solver、GUI、cross-request cache、raw `FaceN` alias、source index / split order / bbox / 面积 / 输出排序 stable selector 均不是 C10-M1 supported 范围。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0
git diff --check
```
