# C12-M2 Part Workbench Native Oracle Probe 批次总入口

## 包目标

C12-M2 单独承接 C12-M1 S6 的重开条件：Part Workbench historical rows 必须先获得 stable native / request-local expected，并证明 current cad-core 与 expected 不一致，才允许后续 implementation。C12-M2 本身是 oracle collection / native probe 包，不是代码实现包。

S0 live 基线为 `HEAD=4d245a9c11`（`4d245a9c11 docs: 新增 C12-M2 native oracle probe 开包`），`pwd=/Users/li/Chili3DProject/FreeCAD`，起点 dirty boundary 为 `<clean>`。C12-M1 队列已空；C12-M2 队列在 S0 执行前从 S0-S6 开始。FreeCADCmd 可发现路径为 `/Users/li/.cargo/bin/freecadcmd`，S0 不启动 FreeCAD，版本 / OCCT / LibPack 和 runtime 分类由 S3 采集。

C12-M1 已确认 Sweep / Filling / GeomPlate / Loft / ProjectOnSurface 只有历史证据，缺少 stable expected/current mismatch；S6 最终发布 `no_code_backlog_gate`，不授权 C++、fixtures、expected、oracle 采集、capability wording 或 adapter/test 改动。C12-M2 单独打开 native oracle probe，但不推翻该 no-code 结论；它的任务是把这些历史证据逐行复开为可采集、可阻断、可发布的 native oracle 结果。

## 最小完整语义批次

C12-M2 覆盖同一个发布问题：Part Workbench retained rows 是否能升级为 implementation candidate。五个 family 的 FreeCAD 调用链不同，因此本包不把它们混成一个代码落点，而是共用同一套 oracle 准入矩阵和 native probe 出口：

- S0/S1 统一冻结 live baseline、source authority 和既有 probe 证据。
- S2 统一判断 request-local/product boundary、helper/native-hidden blocker 和当前 cad-core 可比较性。
- S3 建立 probe harness / FreeCADCmd / artifact schema 的共同准入；当前 schema 为 `c12m2.native-probe-artifact.v1`，FreeCADCmd baseline 为 `1.2.0 revision 20260519` / OCCT `7.8.1`，runtime artifact 位于 `docs/temp/6-29-20-12-c12m2-freecadcmd-baseline-native-probe.json`。
- S4 已处理更接近 DocumentObject / native Part feature 的 Sweep 与 Loft：Sweep Location overload 为 `native_probe_blocked`，no-location auxiliary / binormal / tolerance controls 为 current-covered context；Loft selected subelement 为 `native_hidden`。
- S5 已处理 helper / wrapper / mapper 证据更重的 Filling、GeomPlate 与 ProjectOnSurface：Filling 为 `helper_blocked`；GeomPlate 只有 projected curve2d + initial surface 进入 S6 comparison candidate，G1 curve-on-surface 保持 `native_hidden`；ProjectOnSurface provenance 为 `native_hidden`。
- S6 已发布 oracle 结果和后续授权，不在本包写代码；唯一允许比较 current cad-core 的 GeomPlate projected curve2d + initial surface 已确认为 `current_covered`。

## 工作步骤

| step | title | output |
| --- | --- | --- |
| S0 | live 基线与 oracle 声明口径冻结 | 冻结 HEAD、C12-M1 S6 结论、FreeCADCmd 可用性和本包禁止项。 |
| S1 | FreeCAD 源码与 probe 候选矩阵 | 回填 source authority、既有 expected/probe artifacts 和候选 native 行。 |
| S2 | 范围准入与 blocker 矩阵 | 判断 request-local 边界、helper/native-hidden 阻塞和可比较性。 |
| S3 | 通用 NativeProbe harness 与 FreeCADCmd 基线 | 定义可复用 probe schema、artifact 命名、失败分类和运行口径。 |
| S4 | Sweep / Loft 原生 DocumentObject probe 复审 | 已关闭：Sweep Location native_probe_blocked，Sweep no-location controls current-covered context，Loft native_hidden。 |
| S5 | Filling / GeomPlate / ProjectOnSurface helper-mapper probe 复审 | 已关闭：Filling helper_blocked，GeomPlate projected curve2d + initial surface 进入 S6 comparison candidate / G1 native_hidden，ProjectOnSurface provenance native_hidden。 |
| S6 | Oracle 收集与发布闸门 | 已关闭：发布 `no_code_oracle_blocked_gate`，GeomPlate projected curve2d + initial surface 为 current_covered，无 implementation 包授权。 |

## 发布闸门

S6 只有在同一 row 同时满足以下条件时，才允许写出后续 implementation package 建议：

1. FreeCAD source authority 可追溯到 `src/Mod/Part/App` 或相关原生 binding / helper。
2. native probe 产出稳定 expected artifact，且 artifact 不是 crash、timeout、TypeError、notCollected 或 helper lifecycle 噪声。
3. 行为属于 CAD Core request-local/product boundary，不依赖 GUI session、跨请求 native 文档状态或完整 BREP 传输。
4. current cad-core 输出与 expected 存在稳定 mismatch，并能落到明确 C++ module/API boundary。

任何一项缺失都保持 no-code：可以记录 blocker，可以建议下一包 probe，但不能授权 C++。

S6 最终裁决：没有任何 row 同时满足四项条件。Sweep Location 为 `native_probe_blocked`，Filling 为 `helper_blocked`，Loft 与 ProjectOnSurface 为 `native_hidden`，GeomPlate projected curve2d + initial surface 虽有 expected-backed path 但 current cad-core 已覆盖。`C12M2-BLOCKER-004` 已关闭为 `closed_s6_current_covered_geomplate_only`；后续只允许另开更窄 native probe 包解除 blocker，不授权 C++ implementation 包。

本包禁止在 S6 前把 crash、timeout、TypeError、notCollected、native-hidden、helper blocker 或 probe-only evidence 写成 implementation row；禁止修改 `cad-core/src`、`cad-core/include`、fixtures、expected、tests 或 adapters；禁止用当前机器系统 OCCT 差异替代正式 FreeCAD / LibPack oracle 基线；禁止把 GUI session、跨请求 BREP / TopoDS / NamedShape / ElementMap cache 纳入 CAD Core request-local 产品边界。

## 主要交付物

- `矩阵/c12m2_partworkbench_native_oracle_source_candidates.tsv`
- `矩阵/c12m2_partworkbench_native_oracle_scope_review_matrix.tsv`
- `矩阵/c12m2_partworkbench_native_oracle_blocker_queue.tsv`
- `矩阵/c12m2_partworkbench_native_oracle_non_goal_registry.tsv`
- `矩阵/c12m2_partworkbench_native_oracle_backend_gap_classification.tsv`
- `矩阵/c12m2_partworkbench_native_oracle_probe_matrix.tsv`
- `矩阵/c12m2_partworkbench_native_oracle_validation_matrix.tsv`

## 验收命令

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次 docs/CADCore12.0/README.md
git diff --check
```
