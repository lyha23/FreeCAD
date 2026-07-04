# C12-M14 Part Sweep helper mutable lifecycle 证据解锁批次总入口

## 目标

承接 C12-M13 `blocked_partial_helper_oracle`，为 Part Workbench `BRepOffsetAPI_MakePipeShellPy` 的 `remove/firstShape/lastShape/generated/simulate` 建立可执行的 native probe / product contract / implementation gate 队列。

## 必读文件

- `README.md`
- `7-4-11-54-C12-M14-PartSweepHelperMutableLifecycle证据解锁批次方案.md`
- `矩阵/c12m14_helper_lifecycle_source_matrix.tsv`
- `矩阵/c12m14_helper_lifecycle_scope_matrix.tsv`
- `矩阵/c12m14_helper_lifecycle_oracle_matrix.tsv`
- `矩阵/c12m14_helper_lifecycle_blocker_queue.tsv`
- `矩阵/c12m14_helper_lifecycle_validation_matrix.tsv`
- `矩阵/c12m14_helper_lifecycle_non_goal_registry.tsv`
- `docs/CADCore12.0/README.md`

## 队列顺序

1. `7-4-11-55-【已实现】C12-M14工作步骤总入口.md`
2. `7-4-11-56-【已实现】C12-M14-S0-live基线与C12-M13继承冻结.md`
3. `7-4-11-57-【已实现】C12-M14-S1-source与current-helper-landing复核.md`
4. `7-4-11-58-【已实现】C12-M14-S2-dedicated-native-helper-probe-schema与采集.md`
5. `7-4-11-59-【已实现】C12-M14-S3-product-contract与current-mismatch准入裁决.md`
6. `7-4-12-00-【已实现】C12-M14-S4-helper-lifecycle实现或no-code收口.md`
7. `7-4-12-01-【已实现】C12-M14-S5发布闸门.md`

## 当前状态

- 工作步骤总入口已关闭：包结构、矩阵字段和 S0-S5 队列文件已创建。
- S0 live 基线与 C12-M13 继承冻结已关闭：`HEAD=09e2f66c73`（`09e2f66c73 文档：新增 C12-M14 helper 生命周期证据方案`），起点 worktree clean。
- S1 source 与 current helper landing 复核已关闭：FreeCAD helper binding、plain `Sweep::execute()` wrapper no-mix、cad-core current response 字段、shared builder 内部 `Simulate(2)` 和 C12-M13 focused subset 已记录；`C12M14-BLOCKER-201` 已关闭。
- C12-M13 队列为空，最终状态为 `partial_implementation_with_named_followups`；`ORACLE-301` collected subset 继承为 current-supported，未采证 helper methods 已进入 S2/S3。
- S2 dedicated native helper probe schema 与采集已关闭：`docs/temp/7-4-12-15-c12m14-helper-lifecycle-native-probe-output.json` 覆盖 baseline subset、remove、firstShape/lastShape、generated、simulate 和 remove/readd/simulate/build 组合；FreeCAD `1.2.0 revision 20260519` / OCCT `7.8.1`。组合 case 记录 `NCollection_Sequence::ChangeValue` 为 `native_instability_blocker`，`can_enter_s4=false`。
- S3 product contract 与 current mismatch 准入裁决已关闭：`ORACLE-101..104` 因 stable native evidence + source/current audit mismatch 标为 `implementation_authorized`；`ORACLE-105` 因 native instability 标为 `product_contract_only`，产品路径见 `7-4-13-26-C12-M14-helper-lifecycle-request-local产品契约.md`，不得称为 FreeCAD native parity。
- S4 helper lifecycle 实现收口已关闭：`cad-core/src/part/part_sweep.cpp` 新增 opt-in `HelperLifecycle` request DTO、per-operation response、C12-M14 fixture/expected 和 focused P8 test；plain `Part::Sweep` wrapper no-mix guard 保持不变。
- `ORACLE-105` 仍是 request-local product contract only：实现输出 `native_parity=false` 与 `contract_provenance=cad_core_product_contract_non_parity`，不得称为 FreeCAD native parity。
- S5 发布闸门已关闭：最终状态为 `implementation_unlocked_helper_lifecycle` + `product_contract_published_helper_lifecycle` 混合发布。`ORACLE-101..104` 是 source-backed helper lifecycle current-supported rows；`ORACLE-105` 只按 CAD Core request-local product contract 关闭，不能称为 FreeCAD native parity。Capability / adapter public wording 已发布 `HelperLifecycle` request-local DTO、C12-M14 fixture 和 ORACLE-105 non-parity product contract。
- C12-M14 队列关闭后应只输出表头。

## 执行规则

- 每次只处理队列中的第一个未实现步骤，完成后刷新队列。
- S2 不稳定 probe 只能进入 blocker，不得写成 expected parity。
- S3 未授权时不得修改 `cad-core/src/part/part_sweep.cpp` 或 `topo_shape_expansion.cpp`。
- wrapper path、helper product contract、PartDesign Pipe 三者不得混线。
- `ORACLE-001` 用户失败复现不属于本包。

## 关闭条件

- 队列、矩阵、README 与根 `docs/CADCore12.0/README.md` 均指向 C12-M14。
- `step_goal_queue.py` 可读出 S0-S5 均已 `【已实现】`，最终队列为空。
- TSV 字段数检查通过。
- whitespace 与 `git diff --check` 通过。

## 非目标

- 不在总入口执行代码实现。
- 不重开 C12-M13 PartDesign Pipe 已关闭项。
- 不等待 ORACLE-001 才推进 helper probe。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次 docs/temp docs/CADCore12.0/README.md
git diff --check
```
