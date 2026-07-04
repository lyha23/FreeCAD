# C12-M10 S2 native copied graph oracle 采集（已实现）

## 目标

运行 file-backed FreeCADCmd oracle，采集 SubShapeBinder CopyOnChange copied graph evidence，并判断是否达到 `native_copied_graph_evidence_ready`。

## 必读文件

- `../README.md`
- `../矩阵/c12m10_copy_on_change_probe_matrix.tsv`
- `docs/CADCore12.0/C12-M8-SubShapeBinderCopyOnChangeNativeCopiedGraphDTO准入解锁批次/README.md`
- `docs/temp/c12m8-subshapebinder-copy-on-change-native-copied-graph-evidence-gate.json`
- `docs/temp/c12m8-subshapebinder-copy-on-change-native-copied-graph-probe.raw.c9m5.freecad.json`

## 操作

1. 确认本机 `FreeCADCmd` / `freecadcmd` 可用，并记录 FreeCAD / OCCT / LibPack baseline。
2. 新增或更新 C12-M10 file-backed native probe 脚本到 `docs/temp/`。
3. 运行 probe，输出 artifact 到 `docs/temp/`，schema 使用 `c12m10.copy-on-change-native-copied-graph.v1`。
4. 对 `C12M10-PROBE-001..011` 写 observed_status / decision / artifact_or_note。
5. 若 artifact 仍不能暴露 copied graph 核心证据，关闭为 `native_oracle_blocked_retained`；若暴露，进入 S3 DTO / product contract。
6. 将本 S2 step 文件重命名为带 `【已实现】` 的同名文件。

## 关闭条件

- `C12M10-BLOCKER-201` 关闭为 native evidence ready 或 retained blocker。
- Native artifact 路径、runtime baseline 和每个 evidence field 的 decision 已记录。

## 非目标

- 不刷新 checked-in expected。
- 不改 `cad-core/src`、tests、adapters 或 capability source。
- 不把 property/session 状态、label、bbox、shape count、temporary document name 或 `_CopiedLink` target 单独当作 copied graph success。

## S2 结论

- S2 baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=965915be73`（`965915be73 文档：关闭 C12-M10 S1 source schema 复核`），起点 worktree clean。
- FreeCADCmd：`/Users/li/.cargo/bin/freecadcmd`；FreeCAD baseline 为 `1.2.0 revision 20260519`，OCCT 为 `7.8.1`。
- Probe 脚本：`docs/temp/c12m10-subshapebinder-copy-on-change-native-copied-graph-probe.py`。
- Raw artifact：`docs/temp/c12m10-subshapebinder-copy-on-change-native-copied-graph.raw.freecad.json`。
- Gate artifact：`docs/temp/c12m10-subshapebinder-copy-on-change-native-copied-graph-evidence-gate.json`。
- Schema：`c12m10.copy-on-change-native-copied-graph.v1`。
- `C12M10-PROBE-001..011` 已全部写入 observed_status / decision / artifact_or_note；S2 裁决为 `native_oracle_blocked_retained`。
- 可观察证据包括 runtime baseline、mode matrix、zero/one/multi support gate、`_tmp_binder` document/object order、`_CopiedLink` target/subvalues、`PartialLoad` property state 和 `Cache_*` dynamic matrix cache creation/hit/rewrite。
- retained blocker 原因：`_CopiedObjs` stored identity/order、`Document::copyObject()` dependency list 与 source-to-imported mapping、first/second copied `recomputeFeature(true)` lifecycle、ElementMap / NamedShape per-stage lifecycle 仍未从 FreeCADCmd artifact 稳定导出。
- `C12M10-BLOCKER-201` 已关闭为 `closed_s2_native_oracle_blocked_retained`；`C12M10-VAL-201=passed_s2_native_oracle_blocked_retained`。S3 必须继承该 blocker，不得由 property/session 状态或 `_CopiedLink` target 单独批准 DTO / implementation。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次/矩阵/*.tsv
git diff --check
```
