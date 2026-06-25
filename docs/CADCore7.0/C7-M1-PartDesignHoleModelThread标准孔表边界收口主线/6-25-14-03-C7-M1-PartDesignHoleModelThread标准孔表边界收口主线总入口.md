# C7-M1 PartDesign Hole ModelThread 标准孔表边界收口主线总入口

## 结论

C7-M1 选择 `PartDesign::Hole` 的 ModelThread + 标准孔表边界闭环作为 CADCore7.0 第一包，但不是“重新实现 Hole”。当前 `cad-core` 已有 Hole 常用孔、thread tables、标准头部尺寸、ModelThread pipe-shell tool、history freeze 和 capability 发布；本包要把同一批代表场景重新按 oracle、DTO/API、fixtures、focused tests、capability/docs 和 release gate 串起来。

如果 S1/S2 证明全部代表场景已经 expected-backed 或 publication-backed，S3/S4 只做发布一致性收口；如果 S2 发现 active backend gap，S3 才允许改 `cad-core/src/part_design/feature_hole.cpp`、必要的 `part/topo` / `topo` history 落点、fixtures 和 tests。

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

## 本轮代表批次

| batch | representatives | decision |
| --- | --- | --- |
| live capability | `part_design.hole` capability + adapter assertions | S0 冻结，S4/S5 发布一致性 |
| standard head cut | supported dynamic ISO2009/DIN7984 + standard counterbore/countersink rows | S1 批量 oracle；S2 裁决 pending/freezed expected |
| ModelThread | metric + counterbore ModelThread pipe-shell | S1 复核 FreeCAD `makeThread()`；S3 只在 active gap 时实现 |
| profile source | circle/arc/point profile center discovery | 约束 `findHoles()` local transform 和 source history |
| legacy pending | 旧 `hole-threaded-standard-*` pending expected | S2 裁决为补 oracle、迁移 supported、或移出 active path |

## 非目标

- 不实现 GUI conic edit、full sketch solver conic constraints、DistanceType default/todo。
- 不实现 GUI Hole dialog、property read-only UI 或 persistent backend session。
- 不声明 arbitrary thread standards、full PartDesign Hole parity、full topo naming / full MapperHistory。
- 不把 Hole internal PipeShell 混入 Part Sweep / PartDesign Pipe capability。

## 工作步骤

1. S0：【已实现】冻结 live baseline、C6 closure、Hole capability 和当前 fixture/test 状态。
2. S1：复核 FreeCAD 源码调用链，批量列出 oracle/fixture rows。
3. S2：按矩阵裁决每个 row 是否已经关闭、需要 oracle、需要实现、native blocked 或 non-goal。
4. S3：只实现 S2 接受的 backend gap；否则做 no-code publication closure。
5. S4：同步 fixtures/tests/capability/docs 和验收记录。
6. S5：运行 release gate，更新队列和状态。

## 验收入口

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线
```
