# 【已实现】C12-M14 S2 dedicated native helper probe schema 与采集

## 目标

为 `BRepOffsetAPI_MakePipeShellPy` 未采证方法建立稳定 native helper probe，或者明确记录 native instability blocker。

## live 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD`：`0251a16d10`。
- `git log -1 --oneline`：`0251a16d10 文档：关闭 C12-M14 S1 source landing 复核`。
- `git -c core.quotepath=false status --short -uall`：无输出。
- S2 开始前队列：S2-S5 pending，第一项为本步骤。

## artifact

- Schema：`docs/temp/7-4-12-15-c12m14-helper-lifecycle-native-probe-schema.md`。
- Runner：`docs/temp/7-4-12-15-c12m14-helper-lifecycle-native-probe.py`。
- Output：`docs/temp/7-4-12-15-c12m14-helper-lifecycle-native-probe-output.json`。
- Version：`docs/temp/7-4-12-15-c12m14-helper-lifecycle-freecadcmd-version.txt`。

Runner 使用 one-FreeCADCmd-process-per-case 模式，子进程命令为 file-backed `-c exec(compile(open(...)))`。每个 case 记录 command argv、returncode、shell exit、stdout/stderr、stdout/stderr tail、timeout、method sequence、return summary、diagnostics 和 `can_enter_s4=false`。

## native baseline

- `FreeCADCmd`：`/Users/li/.cargo/bin/freecadcmd`。
- FreeCAD：`1.2.0 revision 20260519`。
- OCCT：`7.8.1`。
- App home：`/Applications/FreeCAD.app/Contents/Resources//`。
- Probe top-level conclusion：`native_instability_blocker`。
- Probe top-level `can_enter_s3=true`，`can_enter_s4=false`。S2 只产出临时证据；S3 才能裁决 product-contract / current mismatch；S4 未解锁。

## coverage summary

| oracle | status | 结论 |
| --- | --- | --- |
| `C12M14-ORACLE-001` | `stable_native_payload` | baseline subset `add/isReady/getStatus/build/shape/makeSolid` 成功；`isReady=true`，build 前后 `getStatus=0`，`shape()` 返回 Shell，`makeSolid()` 返回 `true`。 |
| `C12M14-ORACLE-101` | `stable_native_diagnostic` | `remove` before add / after add before build 给出 stable `PipeShell` diagnostic；after build 与 remove/readd ordering 为 stable payload。 |
| `C12M14-ORACLE-102` | `stable_native_diagnostic` | `firstShape/lastShape` unbuilt 与 failed build 给出 stable null-shape/PipeShell diagnostics；build success 返回 stable shapes。 |
| `C12M14-ORACLE-103` | `stable_native_payload` | `generated(profile)` before build、after build、unknown profile 均返回 stable list payload。 |
| `C12M14-ORACLE-104` | `stable_native_diagnostic` | `simulate(2)` pre/post build 与 `simulate(0)` 返回 stable list payload；unready `simulate(2)` 给出 stable `PipeShell` diagnostic。 |
| `C12M14-ORACLE-105` | `native_instability_blocker` | `remove/readd/simulate/build` 中 `simulate(2)` 返回 list，但后续 `build()` 与 `shape()` 均抛 `NCollection_Sequence::ChangeValue`，记录为 native instability blocker。 |

## 矩阵回写

- `c12m14_helper_lifecycle_oracle_matrix.tsv`：`ORACLE-101..104` 进入 S3 gate pending；`ORACLE-105` 记录为 `native_instability_blocker`。
- `c12m14_helper_lifecycle_scope_matrix.tsv`：remove / first-last / generated / simulate scope 均指向 S2 artifact；simulate scope 明确 combination instability。
- `c12m14_helper_lifecycle_blocker_queue.tsv`：`C12M14-BLOCKER-301` 关闭；新增保留 `C12M14-BLOCKER-302` 记录 `NCollection_Sequence::ChangeValue`。
- `c12m14_helper_lifecycle_validation_matrix.tsv`：`C12M14-VAL-201` 记录实际 probe 命令、输出路径与结论。

## 结论

- S2 已完成 schema、script、version 与 output artifact。
- 没有新增 `cad-core` fixture / expected / tests。
- 当前不授权 C++ helper lifecycle 实现；`ORACLE-105` 的 native instability 必须由 S3 作为 product-contract/current-mismatch gate 输入处理。

## 非目标

- 不实现 helper lifecycle。
- 不把 crash 当 supported。
- 不修改 existing expected。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 docs/temp/7-4-12-15-c12m14-helper-lifecycle-native-probe.py
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次 docs/temp docs/CADCore12.0/README.md
git diff --check
```
