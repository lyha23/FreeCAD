# 【已实现】C7-M1 S1 FreeCAD 源码与 oracle 候选矩阵

## 目标

按 FreeCAD 源码复核 Hole ModelThread、标准孔表 head cut、profile source 和 history 调用链；批量列出本轮 oracle / fixture 候选。S1 仍不改 C++、fixtures、expected 或 tests。

## 必读

- `src/Mod/PartDesign/App/FeatureHole.cpp`
- `cad-core/src/part_design/feature_hole.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_p7_features.py`
- `cad-core/fixtures/p7/hole-*.json`
- `cad-core/fixtures/p7/expected/hole-*.freecad.json`
- `docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/*.tsv`

## 动作

1. 记录 FreeCAD 调用链：`Hole::Hole()`、`readCutDefinitions()`、`updateHoleCutParams()`、`determineDiameter()`、`getThreadPitch()`、`getThreadClassClearance()`、`execute()`、`makeThread()`、`findHoles()`。
2. 对照 cad-core `feature_hole.cpp`：thread table、standard head cut lookup、ModelThread pipe-shell tool、compound tool、history freeze、metadata 输出。
3. 列出 supported native oracle fixtures、legacy pending expected rows、ModelThread + head cut rows、point/circle/arc profile source rows。
4. 更新 source、scope、input contract、oracle fixture、backend gap 矩阵；只写证据和候选，不做 route 结论。
5. 如果需要重新采集 FreeCAD expected，写清 collector 命令和本机 FreeCAD/LibPack/OCCT 基线；不要在 sandbox Qt 失败时直接判定实现失败。

## S1 执行基线

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=669974037a`（`669974037a 文档：冻结 C7-M1 S0 Hole live 基线`），`git status --short -uall` 无输出。
- 队列状态：S1 执行前 C7-M1 队列为 S1-S5 pending；本文件标记为 `【已实现】` 后，队列应推进到 S2。
- 本步只更新 C7-M1 文档包和 TSV 矩阵；未改 C++、fixtures、expected 或 tests。

## FreeCAD 调用链证据

| function | evidence | S1 记录用途 |
| --- | --- | --- |
| `Hole::Hole()` | `readCutDefinitions()` 在构造开始调用；属性注册覆盖 `Threaded`、`ModelThread`、`ThreadType`、`ThreadSize`、`ThreadClass`、`ThreadFit`、`HoleCutType`、`HoleCutDiameter`、`HoleCutDepth`、`ThreadDepthType`、`UseCustomThreadClearance`、`CustomThreadClearance`、`BaseProfileType`。 | 输入契约边界，约束 cad-core DTO 字段。 |
| `readCutDefinitions()` | 搜索 `Mod/PartDesign/Resources/Hole` 与用户目录 `PartDesign/Hole`，读取 `*.json` 并经 `CutDimensionSet` / `addCutType()` 注册动态 head cut。 | 标准孔表来源，约束 oracle 采集基线和资源文件清单。 |
| `updateHoleCutParams()` | metric `Counterbore` 默认读 `ISO 4762`，metric `Countersink` / `Counterdrill` 默认读 `ISO 10642`；动态 `HoleCutTypeMap` 命中时写入 normed `HoleCutDiameter` / `HoleCutDepth` / angle，否则回退 `calculateAndSetCounterbore()` / `calculateAndSetCountersink()` 或非 metric rule-of-thumb。 | head cut rows 必须区分标准表命中、动态资源表和 fallback。 |
| `getThreadPitch()` | 直接返回 `threadDescription[threadType][threadSize].pitch`。 | thread table expected 必须冻结 pitch 来源。 |
| `getThreadClassClearance()` | 只有 `ThreadClass` 第二字符为 `G` 时按 `ThreadClass_ISOmetric_data` 和 pitch 取牙隙，否则返回 `0.0`。 | ModelThread class/custom clearance rows 的输入契约。 |
| `determineDiameter()` | `Threaded && ModelThread` 时先取 custom clearance 或 class clearance；tap drill 可用时用 `TapDrill + clearance`，否则 BSP/BSW/BSF、NPT、generic pitch fallback；非 threaded clearance 走 metric/UTS/fallback clearance table。 | diameter_source rows 和 ModelThread major/tap-drill 组合。 |
| `execute()` | 先按 depth/head cut/drill/taper 构造 revolve `protoHole`；`Threaded && ModelThread` 时调用 `makeThread()`，再把 `protoHole` 与 `protoThread` 放进 `holeWithThread` compound；随后调用 `findHoles()`。 | ModelThread + head cut 代表场景必须覆盖 compound tool。 |
| `makeThread()` | 读取 `ThreadDirection`、pitch、class/custom clearance；构造 thread profile、`makeLongHelix()`，用 `BRepOffsetAPI_MakePipeShell`，模拟端面、`FaceMakerCheese` 封口、sewing、solidify，并按 solid classifier 可能反向。 | pipe-shell tool 与 ModelThread geometry 证据。 |
| `findHoles()` | 对 circle/arc profile edge 用 `GeomAbs_Circle`、`adaptor.IsClosed()` 和 `circle.Axis().Location()` 找中心；point profile 走 `getSubTopoShapes(TopAbs_VERTEX, TopAbs_EDGE)` 避免曲线端点；每个 center 用 `makeShapeWithElementMap(protoHole, mapper, {baseshape})` 后 `makeElementTransform()`。 | point/circle/arc profile source rows 和 history freeze 边界。 |

## cad-core 对照证据

- `cad-core/src/part_design/feature_hole.cpp` 已有 thread tables 和 `threadTableFor()`，`resolveThreadDiameter()` 输出 `diameter_source`、`thread_diameter`、`thread_pitch`、`thread_fit`，并复刻 tap drill、Whitworth/NPT/pitch fallback 和 clearance table。
- `threadClassClearanceFor()` 与 `resolveThreadModelParameters()` 对齐 FreeCAD `getThreadClassClearance()` / `makeThread()`，输出 `thread_class`、`thread_direction`、`thread_clearance`、`thread_radius_clearance`、custom clearance fields。
- `readHoleCutDefinitionFile()` / `resourceHoleCutDefinitions()` 从 `cad-core/resources/hole/*.json` 加载与 FreeCAD `src/Mod/PartDesign/Resources/Hole/*.json` 同名资源；`standardCounterboreFor()` / `standardCountersinkFor()` 与 `normalizeHoleToolOptions()` 写入 `hole_cut_standard`、`hole_cut_definition_source`、`hole_cut_diameter`、`hole_cut_depth`、angle。
- `buildModelThreadAtCenter()` / `buildModelThreadTool()` 用 profile wire + helix + `BRepOffsetAPI_MakePipeShell` + end caps + sewing + solid；`combineHoleAndThreadTools()` 组合 tap-drill tool 与 thread tool compound。
- `holeCentersFromCircularProfile()` / `holeCentersFromPointProfile()` 保留 EdgeN/VertexN source；`namedShapeForHoleToolHistory()` 和 `holeHistoryFreezeJson()` 输出 `hole_find_holes:profile_source`、`hole_cut_history:element_map_freeze`、`hole_model_thread:pipe_shell_tool_history`、`model_thread_compound_tool_shape`、`threaded_model_thread_head_cut_native_oracle` 等冻结证据。
- `cad-core/src/runtime/capability_contract.cpp` 发布 `part_design.hole.model_thread.status=done_first_slice`、`geometry=pipe_shell`、`history.status=element_map_freeze_first_slice`、`native_oracle_fixtures` 和空 `remaining_gaps`；S1 只记录该事实，不做 S2 route 结论。

## oracle / fixture 候选

| group | rows | S1 classification |
| --- | --- | --- |
| supported native oracle fixtures | `hole-supported-threaded-dynamic-iso2009`、`hole-supported-threaded-dynamic-din7984`、`hole-supported-model-thread-metric`、`hole-point-profile`、`hole-supported-point-counterbore`、`hole-supported-model-thread-counterbore` | 已有 expected 与 capability publication；S2 复核是否直接关闭或只需发布同步。 |
| legacy pending expected rows | `hole-threaded-standard-counterbore`、`hole-threaded-standard-countersink`、`hole-threaded-dynamic-iso2009`、`hole-threaded-dynamic-din7984`、`hole-model-thread-metric`、`hole-thread-class-clearance`、`hole-thread-custom-clearance`、`hole-thread-depth-dimension-clamped`、`hole-thread-depth-din76` | expected 仍写 `hole_thread_geometry_oracle_pending`；S2 裁决 collect/migrate/legacy non-active。 |
| ModelThread + head cut | `hole-supported-model-thread-counterbore`、`hole-supported-model-thread-metric`、legacy `hole-model-thread-metric` | 覆盖 `Threaded && ModelThread`、class clearance、pipe-shell、compound tool、head cut 与 history freeze。 |
| point profile source | `hole-point-profile`、`hole-supported-point-profile`、`hole-supported-point-counterbore` | 覆盖 `BaseProfileType=OnPoints` 和 Vertex source history。 |
| circle / arc profile source | `BaseProfileType=6` 的 supported threaded/head-cut/model-thread rows，如 `hole-supported-threaded-dynamic-iso2009`、`hole-supported-model-thread-metric`、`hole-supported-model-thread-counterbore` | 覆盖 circle/arc edge source discovery、Edge source history 和 local transform。 |

## collector / baseline 要求

如 S2 决定重新采集或补充 expected，只记录命令和基线，不在 S1 运行原生 FreeCAD：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
FREECADCMD=/path/to/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/p7/hole-supported-model-thread-counterbore.json
```

正式 expected 必须使用与本阶段 oracle 一致的 FreeCAD / LibPack / OCCT 基线；若只能在另一套 OCCT 上 smoke test，结论只能写成兼容性探测。Codex sandbox 中 `FreeCADCmd` / Qt 启动失败不能判定 cad-core 实现失败。

## S1 输出

- `c7m1_hole_modelthread_source_candidates.tsv`：补充 FreeCAD `getThreadPitch()`、`getThreadClassClearance()`、`readCutDefinitions()`、`execute()`、`makeThread()`、`findHoles()` 与 cad-core 落点证据。
- `c7m1_hole_modelthread_input_contract_matrix.tsv`：把 thread table、ModelThread、head cut、profile source、capability publication 的输入/输出契约推进到 S2 待裁决。
- `c7m1_hole_modelthread_oracle_fixture_matrix.tsv`：批量列出 supported native oracle、legacy pending expected、ModelThread + head cut、point/circle/arc profile source rows。
- `c7m1_hole_modelthread_backend_gap_classification.tsv`：只记录 S1 候选与证据，不裁决 route；所有需判断项交给 S2。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/工作步骤细分 --format markdown
rg -n 'ModelThread|updateHoleCutParams|determineDiameter|getThreadPitch|getThreadClassClearance|makeThread|findHoles|readCutDefinitions' src/Mod/PartDesign/App/FeatureHole.cpp cad-core/src/part_design/feature_hole.cpp
find cad-core/fixtures/p7 -maxdepth 2 -type f \( -name '*hole*json' -o -name '*Hole*json' \) | sort
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线
git diff --check
```

## 通过条件

- oracle fixture matrix 覆盖 supported native oracle、legacy pending rows、ModelThread + head cut、profile source。
- source matrix 写清 FreeCAD 源文件、函数和 cad-core 落点。
- S1 文件名和标题标记为 `【已实现】` 后，队列推进到 S2。
