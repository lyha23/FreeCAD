# C12-M12 FreeCAD Sweep / Pipe Parity 迁移批次总入口

## 目标

把 FreeCAD 原生 Sweep / Pipe 语义迁移到 `/Users/li/Chili3DProject/FreeCAD/cad-core`，并把后续执行拆成可由 `goal-step-runner` 顺序推进的工作步骤。

C12-M12 的核心判断是：先证明 FreeCAD source、当前 `cad-core` 行为和用户失败样例之间的真实差异，再修改实现。任何没有经过 source authority 与 oracle red loop 的修补都不进入本包代码 gate。

## 必读文件

- `README.md`
- `7-3-20-16-C12-M12-FreeCADSweepPipeParity迁移批次方案.md`
- `矩阵/c12m12_sweep_source_matrix.tsv`
- `矩阵/c12m12_sweep_drift_audit.tsv`
- `矩阵/c12m12_sweep_oracle_matrix.tsv`
- `矩阵/c12m12_sweep_blocker_queue.tsv`
- `矩阵/c12m12_sweep_validation_matrix.tsv`
- `矩阵/c12m12_sweep_non_goal_registry.tsv`
- `docs/CADCore12.0/README.md`

## 队列顺序

1. `7-3-20-17-【已实现】C12-M12工作步骤总入口.md`
2. `7-3-20-18-【已实现】C12-M12-S0-live基线与dirty边界冻结.md`
3. `7-3-20-19-【已实现】C12-M12-S1-FreeCAD-source-authority复核.md`
4. `7-3-20-20-【已实现】C12-M12-S2-cad-core-drift审计.md`
5. `7-3-20-21-【已实现】C12-M12-S3-oracle-fixture与红灯闭环.md`
6. `7-3-20-22-【已实现】C12-M12-S4-PartDesignPipe主路径迁移.md`
7. `7-3-20-23-【已实现】C12-M12-S5-PartSweep-wrapper与response收口.md`
8. `7-3-20-24-C12-M12-S6-发布闸门.md`

## 当前状态

- 工作步骤总入口已关闭，已确认包结构、S0-S6 队列顺序、矩阵入口和 TSV 字段数。
- S0 `live 基线与 dirty 边界冻结` 已关闭：当前 live baseline 为 `HEAD=2677f140ed`，baseline status clean，已记录 dirty boundary、现有 sweep/pipe fixture/test surface 和本轮允许写入范围。
- S1 `FreeCAD source authority 复核` 已关闭：source matrix 中 C12M12-SRC-001..005 已标记 reviewed，C12M12-BLOCKER-201 已关闭，VAL-101 已记录通过。
- S2 `cad-core drift 审计` 已关闭：source matrix 中 C12M12-SRC-006..010 已标记 reviewed，DRIFT-001..008 已写入 status、owner_step、required_evidence 和 close_condition；C12M12-BLOCKER-301 已关闭，VAL-201 已记录通过，后续队列从 S3 `oracle fixture 与红灯闭环` 继续。
- S3 `oracle fixture 与红灯闭环` 已关闭：ORACLE-001 未找到用户失败 input/output，已保留为 `blocked_missing_user_input`；ORACLE-003 复用 checked-in FreeCADCmd expected `c51m4/partdesign-pipe-fixed-round-body` 形成 pre-refresh red evidence，expected Body volume `0.72` / edges `20` / vertices `12`，pre-refresh Body volume `0.336` / edges `28` / vertices `15`。C12M12-BLOCKER-501 曾授权 S4 复核 PartDesign Pipe fixed/round selected-spine cap/sewing mismatch；S3 未授权 S5 实现，后续 S5 只能做 wrapper / response 回归收口。
- S4 `PartDesign Pipe 主路径迁移` 已关闭：S3 红灯复现后定位到本地 `cad-core/build` CMake cache 错绑 `cad-web-background/cad-core`；`cmake --fresh -S . -B build` 重新绑定当前源码后，`partdesign-pipe-fixed-round-body` focused case 通过，current Body volume `0.7199999999999999`、edges `20`、vertices `12` 与 FreeCAD expected 一致。本步无 C++ / tests / fixtures / expected 改动；C12M12-BLOCKER-501 关闭为 `closed_current_supported_after_build_refresh`。ORACLE-001 继续等待用户 repro，S5 仍未授权实现。
- S5 `Part Sweep wrapper 与 response 收口` 已关闭：S4 没有 shared-builder source delta，S5 仅执行 no-code regression closeout。指定 8 个 Part Sweep focused controls 通过（`Ran 8 tests in 1.109s OK`），完整 P8 通过（`Ran 219 tests in 44.917s OK`）。`spine`、`sections`、mode / `frenet`、`transition`、`solid`、`advanced`、history、`subshapes` 与 `mesh` response 已由既有 P8 controls 覆盖；`mesh` 只作为 response quality gate。本步未改 C++、fixtures、expected、response schema 或 capability wording，C12M12-BLOCKER-601 关闭为 `closed_no_code_regression_closeout`。

## 执行规则

- 每次只处理队列中的第一个未实现步骤，完成后刷新队列。
- 如果 S1/S2/S3 任一闸门无法证明 source-backed current mismatch，停止在 blocker，不进入 S4/S5 代码实现；当前仅 S4 曾由 ORACLE-003 授权，S5 未授权实现且已按回归收口关闭。
- S4 已关闭；若未来再次出现 fixed/round selected-spine mismatch，应先确认 `cad-core/build` 是否绑定当前源码树。
- S5 已关闭为 no-code regression closeout；没有新的 Part Sweep current mismatch 前不得补实现或修改 response schema。
- S6 必须同时更新 README、矩阵、focused tests 和 release wording。

## 关闭条件

- 队列、矩阵、README 与根 `docs/CADCore12.0/README.md` 均指向 C12-M12。
- `step_goal_queue.py` 能读出入口与 S0-S6。
- TSV 字段数检查通过。
- whitespace 与 `git diff --check` 通过。

## 非目标

- 不在总入口执行实现。
- 不改 `cad-core/src`、fixtures、expected、tests 或 adapter。
- 不把 old `chili3d` sweep 行为当成 FreeCAD parity。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M12-FreeCADSweepPipeParity迁移批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M12-FreeCADSweepPipeParity迁移批次/矩阵/*.tsv
```
