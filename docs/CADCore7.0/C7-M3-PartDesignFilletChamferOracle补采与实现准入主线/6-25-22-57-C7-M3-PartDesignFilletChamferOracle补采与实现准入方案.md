# C7-M3 PartDesign Fillet Chamfer Oracle 补采与实现准入方案

## 背景

C7-M2 已完成 release gate。它没有打开 C++ 实现闸门，而是把 Fillet / Chamfer 残余口径拆为 inherited expected-backed、oracle pending 和 diagnostic non-goal。现在真正阻塞发布的是缺 FreeCAD oracle 的 3 个 rows：Fillet multi-edge / `UseAllEdges`、Chamfer `FlipDirection=true`、DressUp chain stale `ReferenceShadow` / Base recovery。

C7-M3 的方案是 oracle-first：先设计和采集 representative fixtures，再用当前 `cad-core` 跑 parity，最后裁决是否需要实现。不能从 fixture 输出倒推业务逻辑，也不能在未采 oracle 的情况下把 code path 存在等同于 supported。

## 目标

- 为 3 个 C7-M2 `oracle_pending_collect` rows 设计最小完整 oracle 批次。
- 采集或记录 FreeCAD native oracle blocker，明确 expected 来源。
- 用当前 `cad-core` 对新 oracle fixtures 做 parity 分类。
- 若出现 active backend gap，明确 S4 修改文件、FreeCAD 依据、fixture/test 范围和验证命令。
- 若 parity 已闭合，更新 docs/capability 发布口径，不扩大 C++ scope。

## 最小完整语义批次

| 批次 | 代表项 | 判定方式 |
| --- | --- | --- |
| Fillet 选择 | selected multi-edge、`UseAllEdges=true` | FreeCAD oracle + cad-core parity |
| Chamfer 方向 | `FlipDirection=true`，Equal distance true-side + Two distances true-side + Distance and Angle true-side | FreeCAD oracle + cad-core parity |
| DressUp recovery | stale `StableSubList` + `ShadowSub` + `ReferenceShadow` + current graph | 先证明 FreeCAD 可成功恢复；不能采集则 blocker，不实现 |

## 实施纪律

- S0-S3 默认不改 C++；S3 只有裁出 `backend_gap_requires_implementation` 才打开 S4 code edit gate。
- expected 文件只能来自 FreeCAD oracle 或明确 diagnostic blocker，不得用当前 `cad-core` 输出反写 expected。
- `ReferenceShadow` recovery 必须有 `StableSubList`、`ShadowSub`、`ReferenceShadow` 和当前 graph 共同支撑；不得做宽松 fallback。
- 若需要 topo/history 实现，优先补 `cad-core/src/topo/` 和正式 reference recovery 账本，再接入 `feature_dress_up.cpp`；不得在 adapter 或输出 JSON 修剪。
- 若本机 `FreeCADCmd` 在 sandbox 内报 Qt/processor 错误，只记录环境 blocker，不能把它当作 FreeCAD 语义失败。

## 步骤

### S0 live baseline

已完成。S0 冻结的 live 基线为 `pwd=/Users/li/Chili3DProject/FreeCAD`、`HEAD=d678462e20`（`d678462e20 文档：完成 C7-M2 S5 发布闸门`）；开始状态只有目标文档改动（root README modified、C7-M3 包 untracked），没有无关源码、fixture、expected 或 test 改动。C7-M1/C7-M2 队列为空，C7-M3 队列推进到 S1。

S0 冻结的 C7-M2 到 C7-M3 route 映射：

| C7-M2 row | C7-M3 row | route | 后续 |
| --- | --- | --- | --- |
| `C7M2-GAP-101` | `C7M3-SCOPE-101` | `oracle_pending_collect` | Fillet multi-edge / `UseAllEdges` oracle fixture 设计 |
| `C7M2-GAP-203` | `C7M3-SCOPE-102` | `oracle_pending_collect` | Chamfer `FlipDirection=true` true-side oracle fixture 设计 |
| `C7M2-GAP-301` | `C7M3-SCOPE-103` | `oracle_pending_collect` | stale `ReferenceShadow` / Base recovery oracle fixture 设计 |

### S1 oracle fixture 设计

已完成。S1 的 live 起点为 `pwd=/Users/li/Chili3DProject/FreeCAD`、`HEAD=a0a9799608`（`a0a9799608 文档：完成 C7-M3 S0 基线冻结`），开始状态干净。S1 只更新文档和矩阵，没有新增 fixture/expected/tests，没有运行 FreeCAD oracle、cad-core parity 或 C++ build。

S1 确定的 fixture / collector 计划：

| row | fixture | payload 要点 | collector |
| --- | --- | --- | --- |
| `C7M3-SCOPE-101` | `p7/fillet-pad-multi-edge` | `Base=Pad`，`SubList=[Edge1,Edge2]`，`Radius=0.35`，`UseAllEdges=false` | `cd cad-core && FREECADCMD=/path/to/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/p7/fillet-pad-multi-edge.json --out fixtures/p7/expected/fillet-pad-multi-edge.freecad.json` |
| `C7M3-SCOPE-101` | `p7/fillet-pad-use-all-edges` | `Base=Pad`，`SubList=[Edge1]`，`Radius=0.2`，`UseAllEdges=true` | `cd cad-core && FREECADCMD=/path/to/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/p7/fillet-pad-use-all-edges.json --out fixtures/p7/expected/fillet-pad-use-all-edges.freecad.json` |
| `C7M3-SCOPE-102` | `p7/chamfer-pad-edge-flip-true` | `ChamferType=Equal distance`，`Size=0.5`，`FlipDirection=true` | `cd cad-core && FREECADCMD=/path/to/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/p7/chamfer-pad-edge-flip-true.json --out fixtures/p7/expected/chamfer-pad-edge-flip-true.freecad.json` |
| `C7M3-SCOPE-102` | `c3m5/chamfer-two-distances-edge-flip-true` | `ChamferType=Two distances`，`Size=0.3`，`Size2=0.8`，`FlipDirection=true` | `cd cad-core && FREECADCMD=/path/to/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/c3m5/chamfer-two-distances-edge-flip-true.json --out fixtures/c3m5/expected/chamfer-two-distances-edge-flip-true.freecad.json` |
| `C7M3-SCOPE-102` | `c3m5/chamfer-distance-angle-edge-flip-true` | `ChamferType=Distance and Angle`，`Size=0.5`，`Angle=45`，`FlipDirection=true` | `cd cad-core && FREECADCMD=/path/to/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/c3m5/chamfer-distance-angle-edge-flip-true.json --out fixtures/c3m5/expected/chamfer-distance-angle-edge-flip-true.freecad.json` |
| `C7M3-SCOPE-103` | `c3m5/dressup-reference-shadow-base-recovery` | `Chamfer.Base.value=Fillet`，stale `SubList=[OldFilletEdge1]`，`StableSubList=[Edge1]`，`ShadowSub=[{newName=Edge1,oldName=OldFilletEdge1}]`，`ReferenceShadow` 指向 `Fillet.Shape` old edge evidence | geometry-only command is `cd cad-core && FREECADCMD=/path/to/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/c3m5/dressup-reference-shadow-base-recovery.json --out fixtures/c3m5/expected/dressup-reference-shadow-base-recovery.freecad.json`; S2 must add reference evidence or record blocker |

Expected 字段按当前 collector 复核为 `schema_version`、`reference`、`freecad_version` 和目标对象的 `shape_summary.{bbox,volume,topology_counts}`。Body group 中的 DressUp member 会和 Body 一起进入 `objects` expected。S3 focused test 入口从当前 `test_p7_features.py` 复核为 `test_p7_fillet_replaces_body_tip_shape`、`test_p7_chamfer_replaces_body_tip_shape`、`test_c3m5_chamfer_parameter_variants_build`、`test_c3m5_chained_dressup_pattern_history_keeps_support_transform_slot` 等同族用例，具体新增或扩展测试由 S3 在 fixtures/expected 存在后处理。

`ReferenceShadow` recovery 的 S1 结论必须单独保留：当前 `collect_freecad_expected.py::link_sub_value()` 会优先用 `StableSubList` 收集 post-resolution 几何，但不会把 `ShadowSub` / `ReferenceShadow` 原生喂给 FreeCAD。S2 若无法补足 recovered `SubList`、`StableSubList`、`ShadowSub`、`ReferenceShadow` 更新证据，就只能把 `C7M3-SCOPE-103` 记为 collector blocker / `oracle_blocked`，不得宽松 fallback。

### S2 oracle 采集

已完成。S2 的 live 起点为 `pwd=/Users/li/Chili3DProject/FreeCAD`、`HEAD=ad03c44cfe`（`ad03c44cfe 文档：完成 C7-M3 S1 oracle fixture 设计`），开始状态干净。S2 新增 fixtures 和 expected/blocker，不改 C++、tests、runtime、topo、adapter 或 capability。

S2 采集到 5 个 FreeCADCmd expected：

| row | fixture | expected |
| --- | --- | --- |
| `C7M3-SCOPE-101` | `p7/fillet-pad-multi-edge` | `cad-core/fixtures/p7/expected/fillet-pad-multi-edge.freecad.json` |
| `C7M3-SCOPE-101` | `p7/fillet-pad-use-all-edges` | `cad-core/fixtures/p7/expected/fillet-pad-use-all-edges.freecad.json` |
| `C7M3-SCOPE-102` | `p7/chamfer-pad-edge-flip-true` | `cad-core/fixtures/p7/expected/chamfer-pad-edge-flip-true.freecad.json` |
| `C7M3-SCOPE-102` | `c3m5/chamfer-two-distances-edge-flip-true` | `cad-core/fixtures/c3m5/expected/chamfer-two-distances-edge-flip-true.freecad.json` |
| `C7M3-SCOPE-102` | `c3m5/chamfer-distance-angle-edge-flip-true` | `cad-core/fixtures/c3m5/expected/chamfer-distance-angle-edge-flip-true.freecad.json` |

这些 expected 均由 `FREECADCMD=/Users/li/.cargo/bin/freecadcmd python3 tools/collect_freecad_expected.py <fixture> --out <expected>` 生成，`freecad_version=1.2.0 revision 20260519`。

`C7M3-SCOPE-103` 已新增 `cad-core/fixtures/c3m5/dressup-reference-shadow-base-recovery.json`，但只记录 native oracle blocker：`cad-core/fixtures/c3m5/expected/dressup-reference-shadow-base-recovery.freecad.json` 的 `known_gap.kind=dressup_reference_shadow_base_recovery_native_oracle_blocked`。本轮 geometry-only 探测写到 `/tmp/c7m3-dressup-reference-shadow-base-recovery.geometry-only.freecad.json` 且 returncode=0，但当前 collector 是把 `StableSubList` 直接喂给 FreeCAD `PropertyLinkSub`，不能证明 stale `SubList` 经 `ShadowSub` / `ReferenceShadow` 原生恢复。因此 S3 不能从该 row 打开 implementation gate，也不能发布 supported。

### S3 parity gate

运行当前 `cad-core` 对 S2 expected 的 focused parity。Fillet / Chamfer rows 裁为 `already_closed_expected_backed` 或 `backend_gap_requires_implementation`；DressUp stale `ReferenceShadow` / Base recovery 已有 S2 blocker，S3 应裁为 `oracle_blocked`。S3 是唯一 code edit gate。

### S4 实现或 no-code 发布

若 S3 打开 gate，按 FreeCAD 调用链和 cad-core 分层实现；否则只同步 docs/capability/矩阵。任何新增 public API 或 executor 主路径都要在相邻注释写明 FreeCAD 源文件、类/函数和关键短句。

### S5 release gate

清空队列，运行与实际变更匹配的验证。若改 C++、fixtures、expected 或 tests，至少运行相关 focused unittest；若只改 docs/矩阵，则记录不触发 build 的原因。

## 验收分层

### 文档短跑

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线 docs/CADCore7.0/README.md
git diff --check
```

### Oracle / parity 短跑

S1 已从当前 `cad-core/tools/collect_freecad_expected.py` 和 `cad-core/tests/test_p7_features.py` 复核真实命令与 test names。S2 按 S1 单 fixture collector commands 采集；S3 再按当前测试文件扩展或新增 focused unittest。

### 实现短跑

S4 若改 C++，至少运行：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest
```

## 收口标准

- 3 个 oracle pending rows 都有 FreeCAD oracle、native oracle blocker 或明确 non-goal 结论。
- 没有 oracle 的 row 不发布为 supported。
- 若打开 implementation gate，代码修改只覆盖 S3 指定文件和 fixtures/tests。
- C7-M3 队列为空，root README 和本包 README 记录最终发布状态。
