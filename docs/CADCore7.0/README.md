# CADCore7.0

CADCore7.0 承接 C6-M9 之后的下一轮 CAD Core 收口工作。C6-M9 已把 `PartDesign::Groove Type=UpToFirst/UpToFace` 裁决为 FreeCAD native `BRepFeat_MakeRevol` 稳定失败证据，`part_design.revolution_groove.remaining_gaps=[]`，不再作为实现缺口推进。

本轮不打开 C6-M10 Conic 小包：当前 conic 方向剩余项主要是 GUI conic edit、full sketch solver conic constraints、DistanceType default/todo，这些不是当前无状态 CAD Core 后端实现批次。C7.0 的第一包转向 PartDesign Hole，但不重开泛 Hole 支持；C7-M1 已确认 Hole ModelThread、标准孔表驱动头部尺寸、点/圆/弧 profile source 和 history/capability 发布边界形成 expected-backed 闭环，队列为空。

C7-M2 接在 C7-M1 之后，转向 `PartDesign::Fillet` / `PartDesign::Chamfer` 的复杂参数组合与复杂引用变更后的稳定恢复。P7 live 文档已说明基础 Edge / Face Base、连续边过滤、OCCT maker、replacement solid、DressUp AddSubShape cache、`SupportTransform=true` 和链式 DressUp 已有覆盖；本包不重开泛 Fillet / Chamfer。S4 已把旧 P7 残余口径拆成 inherited expected-backed、oracle pending 和 non-goal / publication-only 三类发布口径，避免把缺 oracle 的候选能力写成 supported。

C7-M3 承接 C7-M2 的 3 个 `oracle_pending_collect` rows，不直接实现 C++。本包先为 Fillet multi-edge / `UseAllEdges`、Chamfer `FlipDirection=true`、DressUp chain stale `ReferenceShadow` / Base recovery 设计并采集 FreeCAD oracle，再通过 cad-core parity 打开或关闭 implementation gate。

## 入口

- C7-M1 总入口：`C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/6-25-14-03-C7-M1-PartDesignHoleModelThread标准孔表边界收口主线总入口.md`
- C7-M1 方案：`C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/6-25-14-03-C7-M1-PartDesignHoleModelThread标准孔表边界收口方案.md`
- C7-M1 工作步骤：`C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/工作步骤细分/`
- C7-M1 矩阵：`C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/`
- C7-M2 总入口：`C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/6-25-16-20-C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线总入口.md`
- C7-M2 方案：`C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/6-25-16-20-C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口方案.md`
- C7-M2 工作步骤：`C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/工作步骤细分/`
- C7-M2 矩阵：`C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/矩阵/`
- C7-M3 总入口：`C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/6-25-22-57-C7-M3-PartDesignFilletChamferOracle补采与实现准入主线总入口.md`
- C7-M3 方案：`C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/6-25-22-57-C7-M3-PartDesignFilletChamferOracle补采与实现准入方案.md`
- C7-M3 工作步骤：`C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/工作步骤细分/`
- C7-M3 矩阵：`C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/矩阵/`

## 当前状态

- 方案创建基线：`HEAD=8fdf9b71c5`（`8fdf9b71c5 docs: 完成 C6-M9 S5 发布闸门`）。
- S0 live 基线已冻结：`HEAD=1624050685`（`1624050685 文档：新增 CADCore7.0 Hole 边界收口方案`），`git status --short -uall` 无输出；C6-M1 到 C6-M9 的 `工作步骤细分` 队列均为空。
- S1-S3 已完成：FreeCAD Hole 源码链、cad-core capability/tests/fixtures 和 oracle rows 已被矩阵化；S2 确认没有 `backend_gap_requires_implementation`，S3 完成 no-code publication closure。
- S4 已完成：没有修改 C++、fixtures、expected 或 tests；focused unittest 验证 `part_design.hole` capability、ModelThread pipe-shell、standard/dynamic head cut、native oracle fixtures、history covered/remaining 和 adapter assertions 全部一致。
- S5 已完成：live 起点 `HEAD=dd2b919e46`（`dd2b919e46 文档：完成 C7-M1 S4 发布同步`），`git status --short -uall` 无输出；release gate `cmake --build build` 通过，S4/S5 focused unittest 5 tests OK。本包没有代码、fixtures、expected、test、topo/history 或 adapter schema 改动，未触发重型阶段回归。
- 当前 capability/test 发布口径：`part_design.hole.model_thread.status=done_first_slice`、`geometry=pipe_shell`，`history.status=element_map_freeze_first_slice`，`history.remaining=[]`，`native_oracle_known_gap_fixtures=[]`，`remaining_gaps=[]`；adapter tests 断言这些字段和 supported native oracle fixtures。
- expected-backed rows 的 expected 文件记录 `FreeCADCmd oracle from ...`、`freecad_version=1.2.0 revision 20260519`、topology/volume，不是从当前 `cad-core` 输出倒推；legacy `hole-threaded-standard-*`、`hole-threaded-dynamic-*`、`hole-model-thread-metric`、thread clearance/depth pending stubs 只保留为 historical/non-active diagnostic。
- C7-M1 不声明 full FreeCAD Hole parity，不声明 GUI Hole dialog，不声明 full topo naming / full MapperHistory；当前队列为空。
- C7-M2 S0-S5 已完成：创建前 live 起点 `HEAD=6ba500ea32`（`6ba500ea32 文档：完成 C7-M1 S5 发布闸门`），S5 live 起点 `HEAD=5446576356`（`5446576356 文档：完成 C7-M2 S4 发布口径同步`），开始时 `git status --short -uall` 无输出；S2/S3 没有产生 `backend_gap_requires_implementation`，S4/S5 没有新增或修改 C++、fixtures、expected、tests、topo/history 或 adapter schema。
- C7-M2 release gate 已通过：C7-M2 队列清空，矩阵 TSV 列数检查、trailing whitespace 检查、`git diff --check` 和 route / 发布口径检查通过；本包只改文档/矩阵，未触发 `cad-core` build、focused unittest 或 P7 stage regression。
- C7-M2 最终发布口径：Chamfer Two distances、Chamfer Distance and Angle、SupportTransform mirrored / chained DressUp regression 是 inherited `already_closed_expected_backed`；Fillet multi-edge / `UseAllEdges`、Chamfer `FlipDirection=true`、stale `ReferenceShadow` / Base recovery 是 `oracle_pending_collect`，不能写成 supported；GUI、full DressUp universe、full MapperHistory 和 output-side guessing 是 `diagnostic_non_goal`。
- C7-M3 方案已创建：创建前 live 起点 `HEAD=d678462e20`（`d678462e20 文档：完成 C7-M2 S5 发布闸门`），`git status --short -uall` 无输出。C7-M3 初始队列为 S0-S5 pending；S0/S1 只允许文档和矩阵，S2 才能按设计新增 oracle fixtures/expected，S3 才能裁决 implementation gate。
- C7-M3 S0 已完成：执行 live 基线时 `pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=d678462e20`（`d678462e20 文档：完成 C7-M2 S5 发布闸门`）；开始状态已有目标文档改动（root README modified、C7-M3 包 untracked），未发现无关 dirty 文件。C7-M1/C7-M2 队列为空，C7-M3 队列推进到 S1；3 个 C7-M2 `oracle_pending_collect` rows 已冻结到 C7-M3：`C7M2-GAP-101 -> C7M3-SCOPE-101`、`C7M2-GAP-203 -> C7M3-SCOPE-102`、`C7M2-GAP-301 -> C7M3-SCOPE-103`。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0
```
