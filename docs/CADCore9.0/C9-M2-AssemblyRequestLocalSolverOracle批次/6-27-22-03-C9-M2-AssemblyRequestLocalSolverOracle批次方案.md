# C9-M2 Assembly request-local solver oracle 批次方案

## 目标

C9-M2 不再把 C9-M1 后续工作拆成单个 oracle case。它沿同一条 Assembly request-local solver 调用链，批量处理三个仍有价值的证据面：

- fixed-joint bundle 产生 non-identity `objectPartMap.offsetPlc` 时的 marker placement 与 solver writeback。
- 已存在的 `assembly-marker-custom-placement-chain-real-solver` expected 接入 focused tests。
- zero Angle fallback 到 `ASMTParallelAxesJoint` 的 native expected 与 current parity。

## 范围

纳入本包：

- FreeCAD `AssemblyObject::getMbDData()`、`handleOneSideOfJoint()`、`validateNewPlacements()`、`setNewPlacements()`、`makeMbdJointOfType()`。
- cad-core `joint_solver.cpp` marker resolver、Angle joint mapping、real Ondsel adapter、unsupported diagnostics。
- cad-core `assembly_utils.cpp` 的 request-local `documentObjectUpdates`。
- `cad-core/fixtures/c3m6` Assembly fixture / expected 与 `tests/test_p8_features.py` focused tests。
- `capability_contract.cpp` 与 `tests/test_adapters.py` 的 publication contract。

排除本包：

- 完整 Assembly session、cross-request solver state、backend placement cache。
- 完整 FreeCAD Link ledger、ShowElement 持久写回事务、GUI / ViewProvider / TaskPanel。
- non-AssemblyLink primitive frame generalization，除非产品边界另批 DTO 和 native oracle。
- 用 fixture 名、bbox、几何排序、角度容差或 adapter 字符串掩盖 runtime 差异。

## 最小完整语义批次

本包必须至少把以下代表场景作为同一轮处理：

| 场景 | 为什么同批 | S6 可能结果 |
| --- | --- | --- |
| bundled `offsetPlc` object marker | `getMbDData()` 与 `handleOneSideOfJoint()` 的同一 offset 应用链 | expected-backed covered 或 backendGap |
| bundled `offsetPlc` subshape marker | 同一 marker chain，防止只覆盖 object-level shortcut | expected-backed covered 或 backendGap |
| bundled `offsetPlc` writeback | 同一 `runPreDrag()` / `setNewPlacements()` lifecycle | expected-backed covered 或 backendGap |
| custom placement-chain expected activation | C9-M1 已确认 expected 存在但测试未直接断言 | releaseGate / focused test |
| zero Angle fallback | 同一 `makeMbdJointOfType()` real solver DTO 边界 | expected-backed covered 或 knownGap retained |
| unsupported diagnostics guard | 扩面后不能把 unsupported silent success | releaseGate |

## 步骤

| 步骤 | 任务 | 关闭条件 |
| --- | --- | --- |
| S0 | live 基线与声明口径冻结 | C9-M1 queue empty、capability/non-goal、C9-M2 批次边界写清。 |
| S1 | FreeCAD 源码与 oracle 候选矩阵 | source candidates 覆盖 FreeCAD / cad-core / tests / fixtures。 |
| S2 | 范围准入与 blocker 矩阵 | 所有 scope 都有 owner、route、close condition。 |
| S3 | bundled `offsetPlc` oracle 批量采集 | object marker、subshape marker、writeback 三类 native expected 或明确 blocker。 |
| S4 | custom placement-chain 测试激活 | 已有 expected 被 focused tests 直接断言或记录不可激活原因。 |
| S5 | zero Angle fallback 与 diagnostics 复审 | zero Angle expected / tests 或 known-gap 保留理由；unsupported diagnostics 保持。 |
| S6 | Oracle 实现与发布闸门 | 只消费 source-backed mismatch；更新 capability/docs/tests；队列清空。 |

## 代码落点规则

只有 S3-S5 证明存在 FreeCAD native oracle + current cad-core mismatch 时，S6 才允许代码修改。候选落点限于：

- `cad-core/src/assembly/joint_solver.cpp`
- `cad-core/src/assembly/assembly_utils.cpp`
- `cad-core/src/assembly/assembly_object.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_adapters.py`
- `cad-core/fixtures/c3m6/*`

若 oracle 证明 current 已匹配，S6 应补 focused tests / capability docs 并关闭为 expected-backed release gate。若 oracle 无法采集，保持 `oracle_candidate` / `notCollected`，不得把缺证据写成 backendGap。

## 验收

文档短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次 docs/CADCore9.0/README.md
git diff --check
```

实现闸门：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_adapters
./cad-core capabilities > /tmp/c9m2-capabilities.json
```
