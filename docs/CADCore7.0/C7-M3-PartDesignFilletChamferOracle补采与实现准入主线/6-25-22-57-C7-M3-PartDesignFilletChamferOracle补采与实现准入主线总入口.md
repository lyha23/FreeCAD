# C7-M3 PartDesign Fillet Chamfer Oracle 补采与实现准入主线总入口

## 结论

C7-M3 release gate 已完成，没有打开 C++ implementation gate。C7-M2 release gate 留下的 Fillet multi-edge / `UseAllEdges` 与 Chamfer `FlipDirection=true` rows 已用 S2 FreeCADCmd expected 证明当前 `cad-core` parity 一致，并在 S4/S5 发布为 `already_closed_expected_backed`；DressUp stale `ReferenceShadow` / Base recovery 仍缺 native FreeCAD recovery oracle，保持 `oracle_blocked`。本包没有改 executor、runtime、topo、adapter 或 capability supported 口径，C7-M3 队列为空。

## 上游状态

- C7-M2 队列为空，最终提交为 `d678462e20 文档：完成 C7-M2 S5 发布闸门`。
- C7-M2 没有 `backend_gap_requires_implementation`，未改 C++、fixtures、expected、tests、topo/history 或 adapter schema。
- S0 已冻结当前 live 基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=d678462e20`；开始状态只有目标文档改动（root README modified、C7-M3 包 untracked），没有无关源码、fixture、expected 或 test 改动。
- S1 已完成：live 起点 `HEAD=a0a9799608`（`a0a9799608 文档：完成 C7-M3 S0 基线冻结`），开始状态干净；只改文档和矩阵，未新增 fixture/expected/tests，未运行 FreeCAD oracle 或 cad-core parity。
- S2 已完成：live 起点 `HEAD=ad03c44cfe`（`ad03c44cfe 文档：完成 C7-M3 S1 oracle fixture 设计`），开始状态干净；新增 6 个 fixture JSON、5 个 FreeCADCmd expected JSON 和 1 个 ReferenceShadow native oracle blocker JSON，未改 C++、tests、adapter、runtime、topo 或 capability。
- S3 已完成：live 起点 `HEAD=ac831f3ba7`（`ac831f3ba7 文档：完成 C7-M3 S2 oracle expected 固化`），开始状态干净；新增 focused parity tests，未改 C++、adapter、runtime、topo 或 capability。
- C7-M1/C7-M2 队列为空，C7-M3 队列在 S5 后为空。
- C7-M2 留下的 3 个 oracle pending rows：
  - `C7M2-GAP-101 -> C7M3-SCOPE-101`：Fillet multi-edge / `UseAllEdges`。
  - `C7M2-GAP-203 -> C7M3-SCOPE-102`：Chamfer `FlipDirection=true`。
  - `C7M2-GAP-301 -> C7M3-SCOPE-103`：DressUp chain stale `ReferenceShadow` / Base recovery。

## FreeCAD 调用链

- Fillet：`src/Mod/PartDesign/App/FeatureFillet.cpp::Fillet::execute()` 读取 `Radius` / `UseAllEdges`，`UseAllEdges=true` 时取 `getSubTopoShapes(TopAbs_EDGE)`，否则走 `DressUp::getContinuousEdges(baseShape)`，最后调用 `shape.makeElementFillet(baseShape, edges, Radius, Radius)`。
- Chamfer：`src/Mod/PartDesign/App/FeatureChamfer.cpp::Chamfer::execute()` 读取 `ChamferType` / `Size` / `Size2` / `Angle` / `FlipDirection` / `UseAllEdges`，再调用 `shape.makeElementChamfer(...)`；`migrateFlippedProperties()` 会调整旧文件 `FlipDirection`。
- DressUp recovery：`src/Mod/PartDesign/App/FeatureDressUp.cpp::getBaseObject()`、`getContinuousEdges()`、`getAddSubShape()` 共同决定 Base LinkSub、连续边、AddSubShape cache 和 SupportTransform source base。

## cad-core 落点

- `cad-core/tools/collect_freecad_expected.py`：oracle 采集入口；S1 已复核单 fixture command、`--out` 和 Body member DressUp expected 路径。当前 collector 会把 `StableSubList` 喂给 FreeCAD `PropertyLinkSub`，但不会原生验证 `ShadowSub` / `ReferenceShadow`，S2 必须补证据或记录 blocker。
- `cad-core/fixtures/p7/`：C7-M3 Fillet 与 Chamfer Equal true-side fixture 优先落这里；expected 必须来自 FreeCAD oracle。
- `cad-core/fixtures/c3m5/`：C7-M3 Chamfer non-equal true-side 和 stale DressUp recovery fixture 落这里，复用既有复杂参数 / chained DressUp 语义族。
- `cad-core/src/part_design/feature_fillet.cpp`：S3 parity 已闭合，S4 不修改。
- `cad-core/src/part_design/feature_chamfer.cpp`：S3 parity 已闭合，S4 不修改。
- `cad-core/src/part_design/feature_dress_up.cpp` 与 `cad-core/src/topo/`：`ReferenceShadow` recovery 仍是 `oracle_blocked`，S4 不实现宽松 fallback。

## 步骤队列

1. S0【已实现】：冻结 C7-M3 live baseline 与 C7-M2 oracle pending rows。
2. S1【已实现】：设计 oracle fixtures 与 FreeCAD collector route。
3. S2【已实现】：采集 FreeCAD oracle 或记录 native oracle blocker。
4. S3【已实现】：运行 cad-core parity 并裁决 implementation gate，未打开 C++ gate。
5. S4【已实现】：no-code publication closure。
6. S5【已实现】：release gate，清空队列并同步 README / 矩阵。

## S1 fixture 设计摘要

- Fillet：`p7/fillet-pad-multi-edge` 使用 `Base=Pad`、`SubList=[Edge1,Edge2]`、`Radius=0.35`、`UseAllEdges=false`；`p7/fillet-pad-use-all-edges` 使用 `Base=Pad`、`SubList=[Edge1]`、`Radius=0.2`、`UseAllEdges=true`，证明 Base selection 被 all-edge path 覆盖。
- Chamfer：`p7/chamfer-pad-edge-flip-true` 是 Equal distance true-side smoke；S1 判定 `c3m5/chamfer-two-distances-edge-flip-true` 与 `c3m5/chamfer-distance-angle-edge-flip-true` 也需要采集，才能发布非等距 `FlipDirection=true` 支持。
- DressUp recovery：`c3m5/dressup-reference-shadow-base-recovery` 以 `SketchPad -> Pad -> Fillet -> Chamfer -> Body` 为 current graph；`Chamfer.Base` 保留 stale `SubList`、`StableSubList`、`ShadowSub` 和 `ReferenceShadow`。当前 collector 的 geometry-only 成功不足以证明恢复，S2 无完整证据时必须写 blocker，不能 fallback。

## S2 oracle 结果

- `p7/fillet-pad-multi-edge`、`p7/fillet-pad-use-all-edges`、`p7/chamfer-pad-edge-flip-true`、`c3m5/chamfer-two-distances-edge-flip-true`、`c3m5/chamfer-distance-angle-edge-flip-true` 均已有 FreeCADCmd oracle expected，`freecad_version=1.2.0 revision 20260519`。
- `c3m5/dressup-reference-shadow-base-recovery` 有 fixture 和 blocker expected；`known_gap.kind=dressup_reference_shadow_base_recovery_native_oracle_blocked`。S3 必须把它裁为 `oracle_blocked`，不能把 `/tmp/c7m3-dressup-reference-shadow-base-recovery.geometry-only.freecad.json` 的 StableSubList 几何输出当成恢复支持。

## S3 parity 结果

- `C7M3-SCOPE-101`：`p7/fillet-pad-multi-edge`、`p7/fillet-pad-use-all-edges` 当前 cad-core 输出匹配 S2 expected，route=`already_closed_expected_backed`。
- `C7M3-SCOPE-102`：`p7/chamfer-pad-edge-flip-true`、`c3m5/chamfer-two-distances-edge-flip-true`、`c3m5/chamfer-distance-angle-edge-flip-true` 当前 cad-core 输出匹配 S2 expected，route=`already_closed_expected_backed`。
- `C7M3-SCOPE-103`：`dressup-reference-shadow-base-recovery` expected 仍是 known_gap native oracle blocker，route=`oracle_blocked`。
- 没有 `backend_gap_requires_implementation`，S4 不改 C++。

## S4 no-code 发布结果

- S4 live 起点为 `HEAD=364ae7a093`（`364ae7a093 测试：完成 C7-M3 S3 parity gate`），开始状态干净。
- `p7/fillet-pad-multi-edge`、`p7/fillet-pad-use-all-edges`、`p7/chamfer-pad-edge-flip-true`、`c3m5/chamfer-two-distances-edge-flip-true`、`c3m5/chamfer-distance-angle-edge-flip-true` 已发布为 expected-backed。
- `c3m5/dressup-reference-shadow-base-recovery` 保持 `oracle_blocked` / not supported；geometry-only StableSubList 成功不作为 `ShadowSub` / `ReferenceShadow` 原生恢复证据。
- S4 只同步 docs/矩阵/P7 细化口径，没有修改 C++ executor/runtime/topo/adapter/capability_contract、fixtures/expected/tests；队列推进到 S5。

## S5 release gate 结果

- S5 live 起点为 `HEAD=5e7b76261c`（`5e7b76261c 文档：完成 C7-M3 S4 no-code 发布收口`），开始状态干净。
- focused unittest 3 tests OK：`test_c7m3_fillet_oracle_rows_match_expected`、`test_c7m3_chamfer_flip_direction_oracle_rows_match_expected`、`test_c7m3_reference_shadow_recovery_oracle_remains_blocked`。
- C7-M3 矩阵 TSV 列数检查、trailing whitespace 检查、`git diff --check` 和队列检查通过。
- S5 只更新 root README、本包 README/总入口/方案、步骤文件和矩阵；没有 C++、fixture、expected、test、topo/history、adapter 或 capability schema 改动，因此不触发 `cad-core` build 或全量 FreeCAD build。
- 最终 route：`C7M3-SCOPE-101` / `C7M3-SCOPE-102` expected-backed，`C7M3-SCOPE-103` oracle_blocked，`backend_gap_requires_implementation` 为 0；C7-M3 队列为空。

## 验收入口

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线 docs/CADCore7.0/README.md docs/CADCore方案/细化方案/10-P7-PartDesign常用生态.md
git diff --check
```
