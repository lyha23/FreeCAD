# C12-M10 S2 native copied graph oracle 采集

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

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M10-SubShapeBinderCopyOnChangeCopiedGraphOracle产品契约解锁批次/矩阵/*.tsv
git diff --check
```
