# 【已实现】P8 Assembly Reference / JCS MarkerPlacement S2 范围准入与 blocker 矩阵

## 目标

消费 S0 / S1 的 live source authority，把本包 14 个 scope 拆成 `supportedBaseline`、`backendGap`、`notCollected`、`releaseGate` 和 `nonGoal`，并确认 `MP-BLOCK-001..010`、`MP-BG-001..012`、`MP-NG-001..006` 的路由一致。

S2 不写 C++，不采 expected，不改 collector，不发布 capability。

## live baseline

```text
pwd
/Users/li/Chili3DProject/FreeCAD

git rev-parse --short HEAD
b68e4818bb

git log -1 --oneline
b68e4818bb docs: 完成 P8 MarkerPlacement S1 源码候选复核

git -c core.quotepath=false status --short -uall
<clean>
```

## 输入复核

- `6-18-22-11-P8-Assembly-Reference-JCS-MarkerPlacement收口主线总入口.md`
- `工作步骤细分/6-18-22-11-P8-Assembly-Reference-JCS-MarkerPlacement工作步骤总入口.md`
- `工作步骤细分/6-18-22-12-【已实现】P8-Assembly-Reference-JCS-MarkerPlacement-S0-声明口径与live基线复核.md`
- `工作步骤细分/6-18-22-13-【已实现】P8-Assembly-Reference-JCS-MarkerPlacement-S1-FreeCAD源码候选矩阵.md`
- `矩阵/p8_marker_placement_scope_review_matrix.tsv`
- `矩阵/p8_marker_placement_blocker_queue.tsv`
- `矩阵/p8_marker_placement_backend_gap_classification.tsv`
- `矩阵/p8_marker_placement_non_goal_registry.tsv`

## 范围分类

S2 复核后，`p8_marker_placement_scope_review_matrix.tsv` 保持 14 个 scope，状态分布为：`supportedBaseline=1`、`backendGap=4`、`notCollected=5`、`releaseGate=3`、`nonGoal=1`。

| scope | 状态 | S2 裁决 |
| --- | --- | --- |
| `MP-SCOPE-001` object-level native placement baseline | `supportedBaseline` | 只作为已有 P8 request-local object-level placement 回归基线，不等于 subshape marker placement parity |
| `MP-SCOPE-002` ordinary dispatch / validation | `backendGap` | 进入 `MP-BLOCK-001`；S3 设计 stable diagnostics，S5 才能用实现和 focused parity 关闭 |
| `MP-SCOPE-003` object/subshape global transform | `backendGap` | 进入 `MP-BLOCK-002`；当前 `markerPlacement = connectorPlacement` 不能发布 |
| `MP-SCOPE-004` containing part local transform / `offsetPlc` | `backendGap` | 进入 `MP-BLOCK-003`；必须实现 moving-part-local marker 或显式 diagnostic |
| `MP-SCOPE-005` Vertex marker parity | `notCollected` | 进入 `MP-BLOCK-004`；S4 与 Edge / Face 同批采集 native expected |
| `MP-SCOPE-006` Edge marker parity | `notCollected` | 进入 `MP-BLOCK-004`；S4 与 Vertex / Face 同批采集 native expected |
| `MP-SCOPE-007` Face marker parity | `notCollected` | 进入 `MP-BLOCK-004`；S4 与 Vertex / Edge 同批采集 native expected |
| `MP-SCOPE-008` mixed reference + request-local swap sync | `backendGap` | 进入 `MP-BLOCK-005`；S3/S5 同步 Reference / Placement / connector / marker / DTO |
| `MP-SCOPE-009` current value from JCS placements | `notCollected` | 进入 `MP-BLOCK-005`；S4 采 current value evidence 或写清 case 外边界 |
| `MP-SCOPE-010` RackPinion / Screw special regression | `releaseGate` | 进入 `MP-BLOCK-007`；只作为统一 resolver 切换后的 regression gate |
| `MP-SCOPE-011` real Ondsel marker consumption | `releaseGate` | 进入 `MP-BLOCK-006`；必须证明 `addConstraintToOndselAssembly()` 消费 resolver 输出 |
| `MP-SCOPE-012` native oracle batch completeness | `notCollected` | 进入 `MP-BLOCK-004`；阻止只采单个 Vertex / Edge / Face fixture |
| `MP-SCOPE-013` capability / docs publication | `releaseGate` | 进入 `MP-BLOCK-008`；S6 只能发布 S4/S5 已证明的 representative subset |
| `MP-SCOPE-014` radius / curve / GUI/session / connector-only boundary | `nonGoal` | 进入 `MP-BLOCK-009`；reopen condition 留在 non-goal registry |

## blocker 队列

S2 确认 `MP-BLOCK-001..010` 足以覆盖完整批次，且每个 blocker 均已指向后续 S3-S6 的可关闭条件：

- `MP-BLOCK-001`：ordinary dispatch and reference validation。
- `MP-BLOCK-002`：object/subshape global transform。
- `MP-BLOCK-003`：containing part local transform and offsetPlc。
- `MP-BLOCK-004`：Vertex / Edge / Face primitive native oracle batch。
- `MP-BLOCK-005`：mixed swap and current value oracle。
- `MP-BLOCK-006`：focused parity implementation through real Ondsel marker path。
- `MP-BLOCK-007`：special rewrite regression。
- `MP-BLOCK-008`：capability/docs publication。
- `MP-BLOCK-009`：radius / curve / GUI/session / connector-only boundary protection。
- `MP-BLOCK-010`：fallback removal audit。

`MP-BLOCK-010` 是横向审计项，覆盖 `MP-SCOPE-002/003/004/011`，用于阻止 fixture-name branch、bbox heuristic、output trimming 或 failed resolver silent fallback 进入发布路径。

## backend gap / release 分类

S2 确认 `p8_marker_placement_backend_gap_classification.tsv` 的 12 类分类与 scope / blocker 一致：

| 类别 | 优先级 | 路由 |
| --- | --- | --- |
| `MP-BG-001` ordinary validation / dispatch | P0 | `MP-SCOPE-002` -> `MP-BLOCK-001` -> S3/S5 |
| `MP-BG-002` object/subshape global transform | P0 | `MP-SCOPE-003` -> `MP-BLOCK-002` -> S3/S5 |
| `MP-BG-003` containing part local / offset | P0 | `MP-SCOPE-004` -> `MP-BLOCK-003` -> S3/S5 |
| `MP-BG-004..006` Vertex / Edge / Face parity | P1 | `MP-SCOPE-005..007/012` -> `MP-BLOCK-004` -> S4/S5 |
| `MP-BG-007` mixed swap marker sync | P0 | `MP-SCOPE-008` -> `MP-BLOCK-005` -> S3/S5 |
| `MP-BG-008` current value JCS parity | P1 | `MP-SCOPE-009` -> `MP-BLOCK-005` -> S4/S5 |
| `MP-BG-009` real Ondsel consumption | P1 | `MP-SCOPE-011` -> `MP-BLOCK-006` -> S5 |
| `MP-BG-010` special rewrite regression | P1 | `MP-SCOPE-010` -> `MP-BLOCK-007` -> S5 |
| `MP-BG-011` capability publication | P1 | `MP-SCOPE-013` -> `MP-BLOCK-008` -> S6 |
| `MP-BG-012` non-goal boundary | P2 | `MP-SCOPE-014` -> `MP-BLOCK-009` -> S6 boundary audit |

## nonGoal 一致性

S2 确认 `p8_marker_placement_non_goal_registry.tsv` 的 6 条 reopen condition 与 scope / blocker 一致：

- `MP-NG-001` radius-bearing DistanceType 只能由后续 radius package reopen。
- `MP-NG-002` curve/default DistanceType 只能由后续 curve/default oracle package reopen。
- `MP-NG-003` GUI drag / postDrag / Reverse UI 只能由 GUI 或 Assembly protocol design reopen。
- `MP-NG-004` persistent solver / shape state 只能在 CAD Core 架构不再 stateless recompute 时 reopen。
- `MP-NG-005` full Link copy transaction 只能由 Link / adapter productization package reopen。
- `MP-NG-006` connector-only marker shortcut 只能作为显式 diagnostic compatibility fallback reopen，不能用于 supported subshape refs。

## S2 状态裁决

- S2 已完成：14 个 scope、10 个 blocker、12 类 backend/release/non-goal 分类和 6 条 non-goal reopen condition 已复核并回写路由。
- `supportedBaseline` 仍只限 `MP-SCOPE-001` object-level native placement baseline。
- S3-S6 仍待执行；subshape marker placement、native oracle parity、C++ resolver implementation 和 capability publication 均不得提前写成 supported。
- 本轮未修改 C++，未采 expected，未修改 collector，未发布 capability。

## 验收

```bash
rg -n "MP-SCOPE|MP-BLOCK|MP-BG|MP-NG|supportedBaseline|backendGap|notCollected|releaseGate|nonGoal" docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵/*.tsv
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线
```

## 非目标

- S2 不新增 fixtures。
- S2 不修改 collector。
- S2 不发布 capability。
