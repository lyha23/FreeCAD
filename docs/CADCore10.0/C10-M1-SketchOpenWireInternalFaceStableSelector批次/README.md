# C10-M1 Sketch OpenWire InternalFace StableSelector 批次

本目录承接 C9-M5 queue-empty 后的 live 状态。C10-M1 不重开 SubShapeBinder CopyOnChange，也不把 Sketcher constraint solver 纳入范围；它聚焦 P5b Sketch `InternalShape` 的高风险剩余边界：近切线 / 重合边 / 复杂 open-wire FreeCAD oracle、WireJoiner history ledger、以及 `Profile.StableSubList=InternalFaceN` without `ReferenceShadow` 的 request-local stable selector。

## 入口

- 主线总入口：`6-28-17-33-C10-M1-SketchOpenWireInternalFaceStableSelector批次总入口.md`
- 方案：`6-28-17-33-C10-M1-SketchOpenWireInternalFaceStableSelector批次方案.md`
- 工作步骤总入口索引：`工作步骤细分/6-28-17-34-【已实现】C10-M1工作步骤总入口.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- S0 live 基线已冻结：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=382539f170`（`382539f170 docs: 关闭 C9-M5 S6 发布闸门`）。起始 dirty state 仅为 C10-M1 seed 文档 / 矩阵与 `docs/CADCore10.0/README.md` 未跟踪；本轮不改 `cad-core/src`、fixtures、expected 或 tests。
- S1 source/current 审计已完成：执行基线为 `HEAD=3493d948f5`（`3493d948f5 docs: 修正 C10-M1 S1 cad-core 路径口径`），起始工作区干净，队列下一项已从 S1 推进到 S2。
- S2 scope 准入已完成：执行基线为 `HEAD=b53dd572ad`（`b53dd572ad docs: 完成 C10-M1 S1 源码覆盖矩阵审计`），起始工作区干净，`C10M1-BLOCKER-201` 已关闭为 `closed_s2`。
- S3 near-tangent / coincident-edge oracle 复审已完成：执行基线为 `HEAD=918c09ef8e`（`918c09ef8e docs: 完成 C10-M1 S2 范围准入矩阵`），起始工作区干净；新增 `cad-core/fixtures/c10m1` 四个 input / expected，FreeCAD `freecad_version=1.2.0 revision 20260519`，current public `InternalFace` / `InternalEdge` / `InternalVertex` counts 全部匹配；`C10M1-BLOCKER-301` 已关闭为 `closed_s3`，队列下一项应为 S4。
- C9-M5 队列为空，`copy_on_change_full_temporary_document_cache` 已被保留为 `known_gap_diagnostic` / `oracle_blocked`，不是本批实现入口。
- P5b 当前已支持 bounded split、FaceMaker concrete producer evidence、WireJoiner EdgeInfo / WireInfo 子集、InternalFace profile selection、ReferenceShadow-backed recovery 和多类 P5 fixtures。
- S1 矩阵已把 FreeCAD authority 拆到 `C10M1-SRC-101..108`，把 current cad-core coverage 拆到 `C10M1-SRC-201..208`；`C10M1-BLOCKER-101` 已关闭。
- S0 允许声明的待准入范围仅限近切线、重合边、复杂 open-wire、非平面 / 复杂投影，以及 `Profile.StableSubList=InternalFaceN` without `ReferenceShadow` 的 request-local evidence 边界；S1 未把这些候选升级为 supported。
- S3 route 分布：near-tangent / coincident 已为 `native_expected_collected`，FaceMaker count-level row 为 `no_gap`，`C10M1-CAT-101` 为 `native_expected_collected_no_gap`；complex open-wire 仍保持 `native_oracle_required` / `notCollected` 出口，without-ReferenceShadow stable selector 保持 `backend_gap_candidate`，ambiguous one-to-many open-wire history 保持 `diagnostic_retained`；full solver、GUI、cache、raw FaceN alias、geometry guessing 与 downstream Rust 保持 `diagnostic_non_goal`。
- S0 禁止声明 CopyOnChange、full Sketcher solver、GUI、cross-request cache、raw `FaceN` alias、source index / split order / bbox / 面积 / 输出排序 stable selector 为 supported。

## 收口边界

- C10-M1 只处理 Sketch internal geometry / PartDesign profile consumer / topo history 的 request-local 语义。
- 不实现完整 Sketcher solver、GUI、跨请求 Sketch 状态、raw BREP 持久缓存或输出端修剪。
- 不用 fixture 名称、source index、split 顺序、bbox、面积或输出排序猜 InternalFace / InternalEdge ownership。
- 只有 S3-S5 产出 FreeCAD oracle、current mismatch 和产品边界后，S6 才允许打开 C++ implementation gate。

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
