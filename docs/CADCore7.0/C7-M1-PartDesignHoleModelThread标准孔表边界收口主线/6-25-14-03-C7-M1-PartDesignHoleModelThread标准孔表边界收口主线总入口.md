# C7-M1 PartDesign Hole ModelThread 标准孔表边界收口主线总入口

## 结论

C7-M1 选择 `PartDesign::Hole` 的 ModelThread + 标准孔表边界闭环作为 CADCore7.0 第一包，但不是“重新实现 Hole”。当前 `cad-core` 已有 Hole 常用孔、thread tables、标准头部尺寸、ModelThread pipe-shell tool、history freeze 和 capability 发布；本包要把同一批代表场景重新按 oracle、DTO/API、fixtures、focused tests、capability/docs 和 release gate 串起来。

S2 已证明全部代表场景已经 expected-backed 或 publication-backed：supported native rows 走 `already_closed_expected_backed`，capability/docs 漂移走 `publication_closure_only`，legacy pending expected rows 保留为 `historical_or_native_blocked` 的 diagnostic historical / non-active legacy。S3 已完成 no-code publication closure，没有改 `cad-core/src/part_design/feature_hole.cpp`、fixtures、expected 或 tests；S4 已同步 fixtures/tests/capability/docs 发布口径并关闭 capability/docs drift blocker。

## S0 live 基线冻结

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=1624050685`（`1624050685 文档：新增 CADCore7.0 Hole 边界收口方案`），`git status --short -uall` 无输出。
- 队列状态：C6-M1 到 C6-M9 的 `工作步骤细分` 队列均为空；C7-M1 初始队列为 S0-S5 pending，S0 完成后应推进到 S1。
- capability/test 基线：`part_design.hole.model_thread.status=done_first_slice`、`geometry=pipe_shell`，`history.status=element_map_freeze_first_slice`，`history.covered` 包含 profile source、point profile head cut、ModelThread compound tool 和 threaded ModelThread head-cut native oracle，`history.remaining=[]`、`remaining_gaps=[]`。
- adapter/focused test 基线：`cad-core/tests/test_adapters.py` 断言 Hole capability、producer matrix、topo history 和 known gaps；`cad-core/tests/test_p7_features.py` 覆盖 supported threaded heads、ModelThread pipe-shell tool、native oracle matrix 和 `hole-supported-model-thread-counterbore` expected。
- legacy pending expected：`cad-core/fixtures/p7/expected/hole-threaded-standard-counterbore.freecad.json` 与 `hole-threaded-standard-countersink.freecad.json` 仍写 `FreeCAD Hole threaded-standard oracle pending` / `hole_thread_geometry_oracle_pending`，S0 不裁决，交给 S1/S2。

## 为什么不是 C6-M10 Conic

Conic 方向剩余项主要是 GUI conic edit、full sketch solver conic constraints、DistanceType default/todo。它们要么是 GUI/session，要么是完整 Sketcher solver，不属于当前无状态 CAD Core 后端批量实现边界。C7-M1 选 Hole，是因为它同时命中 FreeCAD 调用链、DTO/API、expected fixture family 和 capability 发布边界，能形成较厚的一轮闭环。

## FreeCAD 调用链

- `src/Mod/PartDesign/App/FeatureHole.cpp::Hole::Hole()`：注册 `Threaded`、`ModelThread`、`ThreadType`、`ThreadSize`、`ThreadClass`、`ThreadFit`、`HoleCutType`、`HoleCutDiameter`、`HoleCutDepth`、`ThreadDepthType`、`BaseProfileType` 等属性。
- `FeatureHole.cpp::Hole::readCutDefinitions()` / `updateHoleCutParams()`：加载标准件表并解析 ISO 4762、ISO 10642、DIN 7984、ISO 2009 等 head cut 维度。
- `FeatureHole.cpp::Hole::determineDiameter()`、`getThreadPitch()`、`getThreadClassClearance()`：解析 thread table、tap drill、ModelThread major diameter 和 clearance。
- `FeatureHole.cpp::Hole::execute()`：构建 revolve hole tool；`Threaded && ModelThread` 时调用 `makeThread()` 并把 `protoHole` + `protoThread` 组成 compound。
- `FeatureHole.cpp::Hole::findHoles()`：从 circles/arcs/points 找中心，`makeShapeWithElementMap()` 后对每个 center 做 local transform，再 compound。
- `FeatureHole.cpp::Hole::makeThread()`：按 thread family 构建 profile，使用 pipe-shell 风格生成真实螺纹工具。

## cad-core 落点

- `cad-core/src/part_design/feature_hole.cpp`：Hole 属性解析、thread table、标准头部尺寸、ModelThread tool、history freeze、对象 metadata。
- `cad-core/src/runtime/capability_contract.cpp`：`part_design.hole` capability、`model_thread` 状态、native oracle fixtures、history covered/remaining。
- `cad-core/tests/test_p7_features.py`：Hole focused behavior、native oracle、thread table、ModelThread 和 standard head cut assertions。
- `cad-core/tests/test_adapters.py`：capability publication assertions。
- `cad-core/fixtures/p7/hole-*.json` 与 `cad-core/fixtures/p7/expected/hole-*.freecad.json`：本轮唯一 fixture family。

## S1 源码与 oracle 候选矩阵

- live 起点：`HEAD=669974037a`（`669974037a 文档：冻结 C7-M1 S0 Hole live 基线`），`git status --short -uall` 无输出。
- FreeCAD 证据已覆盖 `Hole::Hole()`、`readCutDefinitions()`、`updateHoleCutParams()`、`determineDiameter()`、`getThreadPitch()`、`getThreadClassClearance()`、`execute()`、`makeThread()`、`findHoles()`。
- cad-core 对照已覆盖 thread table、standard head cut lookup、ModelThread pipe-shell tool、compound tool、history freeze 和 metadata 输出。
- 矩阵已列出 supported native oracle fixtures、legacy pending expected rows、ModelThread + head cut rows、point/circle/arc profile source rows；S1 只写证据和候选，S2 才裁决 route。

## S2 准入裁决

- live 起点：`HEAD=41c62e7070`（`41c62e7070 docs: 完成 C7-M1 S1 源码与 oracle 矩阵`），`git status --short -uall` 无输出。
- supported standard/dynamic head cut、ModelThread metric、ModelThread counterbore、point/circle/arc profile source rows 均有 native expected 和 focused tests，裁决为 `already_closed_expected_backed`。
- legacy `hole-threaded-standard-*`、`hole-threaded-dynamic-*`、`hole-model-thread-metric`、thread clearance 和 thread depth pending stubs 不再代表 active backend failure；它们保留为 historical diagnostic / non-active legacy，不在 C7-M1 补 oracle。
- ModelThread + head cut 没有 geometry/topology/history active gap；`hole-supported-model-thread-counterbore` expected 和 tests 已覆盖 compound tool、pipe-shell history、topology counts、volume 与 `threaded_model_thread_head_cut_native_oracle`。
- S3 不允许改代码；S3/S4 只允许发布一致性收口、状态文字清理和 release-gate 记录。

## S3 发布收口

- live 起点：`HEAD=24b36ee45f`（`24b36ee45f 文档：完成 C7-M1 S2 准入裁决`），`git status --short -uall` 无输出。
- S3 按 S2 route 执行 publication closure：supported native rows 维持 `already_closed_expected_backed`，legacy pending rows 维持 `historical_or_native_blocked` / non-active diagnostic，`part_design.hole` capability drift 留给 S4。
- S3 未授权也未修改 C++、fixtures、expected 或 tests；未采集 oracle，未运行 `cmake --build` 或 focused unittest。
- 本步骤只同步 README、总入口、步骤索引、S3 步骤文件和 C7-M1 矩阵状态，队列推进到 S4。

## S4 fixtures tests capability 发布

- live 起点：`HEAD=d4574e4b92`（`d4574e4b92 文档：完成 C7-M1 S3 发布收口`），`git status --short -uall` 无输出。
- S4 复核 `cad-core/src/runtime/capability_contract.cpp`、`cad-core/tests/test_p7_features.py`、`cad-core/tests/test_adapters.py`、`cad-core/fixtures/p7/hole-*.json` 和 `cad-core/fixtures/p7/expected/hole-*.freecad.json`，只同步文档/矩阵，不改 C++、fixtures、expected 或 tests。
- `part_design.hole` 发布口径保持：`model_thread.status=done_first_slice`、`model_thread.geometry=pipe_shell`、`history.status=element_map_freeze_first_slice`、`history.remaining=[]`、`native_oracle_known_gap_fixtures=[]`、`remaining_gaps=[]`；adapter assertions 覆盖这些字段、native oracle fixtures 和 topo producer matrix。
- expected-backed rows 的 expected JSON 写有 `FreeCADCmd oracle from ...`、`freecad_version=1.2.0 revision 20260519`、topology/volume，不是从当前 `cad-core` 输出倒推；legacy pending rows 的 expected JSON 仍写 `hole_thread_geometry_oracle_pending` 和不要从 cad-core 输出冻结几何，S4 保持其 historical/non-active 结论。
- S4 focused unittest 通过：5 tests OK；capability/docs drift blocker 已关闭，队列推进到 S5。

## 本轮代表批次

| batch | representatives | decision |
| --- | --- | --- |
| live capability | `part_design.hole` capability + adapter assertions | S0 冻结，S4/S5 发布一致性 |
| standard head cut | supported dynamic ISO2009/DIN7984 + standard counterbore/countersink rows | S2：supported rows `already_closed_expected_backed`；legacy pending 保留 historical diagnostic |
| ModelThread | metric + counterbore ModelThread pipe-shell | S2：expected-backed，无 active backend gap |
| profile source | circle/arc/point profile center discovery | S2：expected-backed，命名顺序差异不算硬失败 |
| legacy pending | 旧 `hole-threaded-standard-*` pending expected | S2：`historical_or_native_blocked`，不补 oracle，不进 S3 实现 |

## 非目标

- 不实现 GUI conic edit、full sketch solver conic constraints、DistanceType default/todo。
- 不实现 GUI Hole dialog、property read-only UI 或 persistent backend session。
- 不声明 arbitrary thread standards、full PartDesign Hole parity、full topo naming / full MapperHistory。
- 不把 Hole internal PipeShell 混入 Part Sweep / PartDesign Pipe capability。

## 工作步骤

1. S0：【已实现】冻结 live baseline、C6 closure、Hole capability 和当前 fixture/test 状态。
2. S1：【已实现】复核 FreeCAD 源码调用链，批量列出 oracle/fixture rows。
3. S2：【已实现】按矩阵裁决每个 row；无 active backend gap，legacy pending 只保留 historical diagnostic。
4. S3：【已实现】no-code publication closure；不改 C++、fixtures、expected 或 tests。
5. S4：【已实现】同步 fixtures/tests/capability/docs 和验收记录；不改 C++、fixtures、expected 或 tests。
6. S5：运行 release gate，更新队列和状态。

## 验收入口

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线
```
