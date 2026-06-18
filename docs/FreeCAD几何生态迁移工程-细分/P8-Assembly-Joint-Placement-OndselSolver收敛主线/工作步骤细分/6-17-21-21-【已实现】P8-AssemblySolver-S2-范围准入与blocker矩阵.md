# 【已实现】P8 AssemblySolver S2 范围准入与 blocker 矩阵

## 目标

消费 S1 候选，把 Assembly solver 主线分类为 `releaseGate`、`notCollected`、`unsupported`、`backendGap` 或 `nonGoal`。S2 不写 C++，不采 FreeCAD oracle，只形成 S3-S6 可执行队列。

## 后续修正

2026-06-18 后续实现已删除 representative fallback 和 optional unlinked build 路径；下文关于 fallback / linked-unlinked 的裁决只保留为当时分流依据，不代表当前发布状态。native solver placement expected 已在 S6 之后入库，并已在 S7 修复到 supported；当前状态以 S7、矩阵、C++ 和 C ABI capabilities 的 real-only 口径为准。

## 本轮 live 基线

| 项 | 结果 |
| --- | --- |
| `pwd` | `/home/user/Chili3DProject/FreeCAD` |
| `git rev-parse --short HEAD` | `6d35327fcb` |
| `git log -1 --oneline` | `6d35327fcb fix: 收敛 P7 transformed 拓扑 oracle` |
| `git status --short -uall` | 工作区已有非 S2 改动和 P8 seed 未跟踪文件；本轮只编辑 P8 AssemblySolver S2 文档、P8 总入口、步骤总入口和四个 S2 分类矩阵，不 reset、不 revert、不清理其它文件。 |

## 读取依据

| 依据面 | 关键结论 |
| --- | --- |
| S0 | 当前 C++、C ABI capabilities 和 focused tests 已暴露 request-local solver 子集，但正式 P8 文档仍有 `solve=not_migrated` 旧口径。 |
| S1 | `P8ASM-CAND-001..018` 已覆盖 FreeCAD `AssemblyObject::solve()`、`validateNewPlacements()`、`setNewPlacements()`、`makeMbdJointOfType()`、`JointObject.py`、`GroundedJoint`、`JointGroup::getJoints()` 以及 cad-core DTO / adapter / tests。 |
| FreeCAD source | FreeCAD solve 顺序为 `syncGroundedJoints()`、`fixGroundedParts()`、`jointParts()`、`runPreDrag()`、`setNewPlacements()`；grounded 移动时 `validateNewPlacements()` 拒绝 bad solve。 |
| cad-core source | `AssemblySolveRequest`、`solveAssemblyWithOndselAdapter()`、representative fallback、`assembly_set_placement` 和 unsupported diagnostics 已存在，但仍需 S3-S5 复核边界。 |
| fixtures/tests | P8 expected 主要覆盖 Assembly metadata；C3M6 focused tests 覆盖 real solver、representative fallback、writeback 和 unsupported diagnostics；没有 checked-in FreeCAD native solver placement oracle。 |

## S2 分类结论

| scope | 状态 | 准入理由 | 后续路由 |
| --- | --- | --- | --- |
| `P8ASM-SCOPE-001` | `releaseGate` | docs/capabilities/tests 存在 publication drift。 | S6 |
| `P8ASM-SCOPE-002` | `releaseGate` | Joint / GroundedJoint DTO 有 source 和 focused coverage，但只能发布 request-local 输入契约。 | S4/S6 |
| `P8ASM-SCOPE-003` | `releaseGate` | real Ondsel grounded path 有代码和 focused tests，仍缺 native parity 与 build-mode 发布闭环。 | S3/S6 |
| `P8ASM-SCOPE-004` | `releaseGate` | representative fallback 只代表 transport / DTO 行为，不等价 FreeCAD solver。 | S3/S6 |
| `P8ASM-SCOPE-005` | `releaseGate` | placement writeback focused tests 存在，仍需验证无状态生命周期和发布口径。 | S4/S6 |
| `P8ASM-SCOPE-006` | `notCollected` | 缺 checked-in FreeCAD native solver placement oracle，不能转 backendGap。 | S6 |
| `P8ASM-SCOPE-007` | `unsupported` | FreeCAD 支持更多 JointType，cad-core 当前只诊断或只覆盖子集。 | S5/S6 |
| `P8ASM-SCOPE-008` | `releaseGate` | Ondsel linked/unlinked build mode影响 capability 声明。 | S3/S6 |
| `P8ASM-SCOPE-009` | `nonGoal` | GUI、持久 session、完整 transaction lifecycle 超出 CAD Core 无状态边界。 | 保持排除 |

## blocker 队列

`p8_assembly_solver_blocker_queue.tsv` 已回写 `P8ASM-BLOCK-001..006`：

| blocker | 路由 | 关闭条件 |
| --- | --- | --- |
| `P8ASM-BLOCK-001` | S6 | P8 docs / overview / capabilities 与验证后的子集一致。 |
| `P8ASM-BLOCK-002` | S4/S6 | Joint DTO 和 hidden reference 输入契约发布为 request-local，不声明完整 FreeCAD lifecycle。 |
| `P8ASM-BLOCK-003` | S3/S6 | real adapter、fallback 和 oracle 缺口边界分清。 |
| `P8ASM-BLOCK-004` | S4/S6 | writeback 只作为 `documentObjectUpdates` 建议，下一请求由前端 graph 应用。 |
| `P8ASM-BLOCK-005` | S5/S6 | unsupported JointType 有 diagnostics 和 reopen 条件，扩展必须有 evidence。 |
| `P8ASM-BLOCK-006` | S3/S6 | `CAD_CORE_HAS_ONDSEL_SOLVER` on/off 能力声明不越界。 |

## backendGap 裁决

S2 没有创建 evidence-backed `backendGap`。`backend_gap_classification.tsv` 只是沿用文件名作为聚合表，当前五类分别是：

- `releaseGate_publication_alignment`
- `notCollected_solver_oracle_queue`
- `releaseGate_placement_writeback_contract`
- `unsupported_joint_type_queue`
- `nonGoal_persistent_assembly_boundary`

任何后续 `backendGap` 都必须同时具备 FreeCAD authority 和 checked-in oracle / focused mismatch 证据；旧 P8 文档里的 “not migrated” 不能单独作为 backendGap。

## 非目标

`non_goal_registry.tsv` 已明确排除 GUI Workbench lifecycle、跨请求 solver session、完整 Link writeback lifecycle、Worker/WASM productization，以及无 oracle 的复杂 JointType 猜测实现。每项都写明了用户 / 协议行为和 reopen 条件。

## 回写文件

- `矩阵/p8_assembly_solver_scope_review_matrix.tsv`
- `矩阵/p8_assembly_solver_blocker_queue.tsv`
- `矩阵/p8_assembly_solver_backend_gap_classification.tsv`
- `矩阵/p8_assembly_solver_non_goal_registry.tsv`

`p8_assembly_solver_source_candidates.tsv` 只读，保持 S1 候选证据不变。

## 验收

本轮只跑 S2 指定的轻量验收：

```bash
awk -F '\t' 'FNR==1 {n=NF; next} NF!=n {print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END {exit bad}' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线/矩阵/*.tsv
python3 - <<'PY'
from pathlib import Path
root=Path('docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线/矩阵')
scopes={l.split('\t')[0] for l in (root/'p8_assembly_solver_scope_review_matrix.tsv').read_text().splitlines()[1:] if l}
cands={l.split('\t')[0] for l in (root/'p8_assembly_solver_source_candidates.tsv').read_text().splitlines()[1:] if l}
for line in (root/'p8_assembly_solver_scope_review_matrix.tsv').read_text().splitlines()[1:]:
    cols=line.split('\t'); assert cols[0]; assert all(c in cands for c in cols[1].split(';')), line
for line in (root/'p8_assembly_solver_blocker_queue.tsv').read_text().splitlines()[1:]:
    cols=line.split('\t'); assert cols[1] in scopes, line
for line in (root/'p8_assembly_solver_backend_gap_classification.tsv').read_text().splitlines()[1:]:
    cols=line.split('\t'); assert all(s in scopes for s in cols[3].split(';')), line
PY
git diff --check
```

## 非目标

- 不实现 unsupported JointType。
- 不运行 FreeCADCmd collector。
- 不把 `notCollected` 直接转 C++。
- 不把完整 Assembly persistence 和 Link writeback 生命周期纳入本主线。
