# C9-M1 Assembly Joint marker / offset placement request-local 方案

## 目标

C9-M1 聚焦 Assembly Joint request-local solver 的 marker 与 placement 边界。它不追求完整 Assembly solver 产品化，而是补齐当前可由 FreeCAD source、C3M6 expected 和 cad-core focused tests 约束的同一调用链：

- Joint / GroundedJoint 输入 schema。
- `handleOneSideOfJoint()` 的 marker placement 计算。
- `offsetPlc` 在 marker creation、placement validation 和 placement writeback 中的组合关系。
- real Ondsel `runPreDrag()` 的 request-local placement update。
- capability / diagnostics 对 supported、non-goal、oracle-blocked 的发布口径。

## 范围

纳入本包：

- FreeCAD `AssemblyObject.cpp`、`AssemblyUtils.cpp`、`JointObject.py` 对 Joint marker、JointType、Distance / Angle、ObjectToGround 和 placement writeback 的定义。
- cad-core `assembly/joint_solver.cpp` 中 current marker resolver、solver DTO、unsupported joint diagnostics 和 real Ondsel adapter。
- cad-core `assembly/assembly_utils.cpp` 的 `documentObjectUpdates.action=assembly_set_placement`。
- `cad-core/fixtures/c3m6` native expected 和 `cad-core/tests/test_p8_features.py` / `tests/test_adapters.py` focused tests。
- capability `assembly.ondsel_solver_adapter` 的 `subshape_marker_placement`、non-goals、remaining_gaps 和 focused evidence。

排除本包：

- GUI / ViewProvider / TaskPanel 行为。
- persistent solver state、cross-request Assembly session 或 backend placement cache。
- 完整 FreeCAD Link 账本、ShowElement 持久写回事务、cross-document hash lifecycle。
- Worker / WASM / Web bridge 产品化。
- 任何 adapter 层字符串改写、fixture 名称分支或几何排序猜测。

## 步骤

| 步骤 | 任务 | 关闭条件 |
| --- | --- | --- |
| S0 | live 基线与声明口径冻结 | HEAD、队列、capability non-goal、C8 closed state 和 forbidden claims 写清。 |
| S1 | FreeCAD 源码与 current coverage 候选 | source_candidates 覆盖 FreeCAD / cad-core / tests / fixtures。 |
| S2 | 范围准入与 blocker 矩阵 | marker、offsetPlc、writeback、capability、non-goal 均有 owner step。 |
| S3 | marker placement 与 `offsetPlc` 复审 | 决定 non-identity `offsetPlc` 和 primitive frame 是否进入 oracle / backendGap。 |
| S4 | `runPreDrag` placement writeback 复审 | 确认 placement update 是否 request-local covered 或需要实现。 |
| S5 | capability 与 diagnostics 发布准入 | 精确裁决 S6 是 C++ gate、capability patch 还是 no-code close。 |
| S6 | Oracle 实现与发布闸门 | 关闭 blocker，跑 focused tests，队列清空。 |

## 代码落点规则

只有 S3-S5 证明存在 source-backed、request-local、非跨请求缓存的实现缺口时，S6 才允许代码修改。候选落点限于：

- `cad-core/src/assembly/joint_solver.cpp`
- `cad-core/src/assembly/assembly_utils.cpp`
- `cad-core/src/assembly/assembly_object.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_adapters.py`
- `cad-core/fixtures/c3m6/*`

若缺口仅是 native oracle 不足，S6 应保持 `oracle_candidate` / `notCollected`，不得把它写成 backendGap。若缺口只是 capability 文案滞后，S6 只做 capability / docs / tests 发布收口。

## 验收

文档短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore9.0
git diff --check
```

实现闸门：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_adapters
```
