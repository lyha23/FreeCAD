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
| Chamfer 方向 | `FlipDirection=true`，优先 Equal distance，按 S1 决定是否补 Two distances / Distance and Angle true-side | FreeCAD oracle + cad-core parity |
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

为 3 个 rows 写清 fixture payload、FreeCAD source authority、collector path、expected output fields、focused test plan 和失败时的 blocker 分类。S1 可以新增计划文档和矩阵行，但不新增 fixture/expected。

### S2 oracle 采集

按 S1 设计新增 fixtures 并采集 FreeCAD expected。若 collector 缺能力，先修 collector 或记录 native oracle blocker；不得把 `cad-core` 当前输出写入 expected。

### S3 parity gate

运行当前 `cad-core` 对 S2 expected 的 focused parity。每行裁为 `already_closed_expected_backed`、`backend_gap_requires_implementation`、`oracle_blocked` 或 `diagnostic_non_goal`。S3 是唯一 code edit gate。

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

S1 必须从当前 `cad-core/tools/collect_freecad_expected.py` 和 `cad-core/tests/test_p7_features.py` 复核真实命令与 test names 后写入；不要在方案里硬编码未复核的旧 filter。

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
