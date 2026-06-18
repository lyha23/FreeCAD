# P8 Assembly Reference / JCS MarkerPlacement S2 范围准入与 blocker 矩阵

## 目标

消费 S0 / S1 的候选，把本包拆成 supported baseline、backendGap、notCollected、releaseGate 和 nonGoal。S2 不写 C++，不采 expected。

## 范围分类

| scope | 预期状态 | 说明 |
| --- | --- | --- |
| object-level baseline | `supportedBaseline` | 既有 P8 native placement expected 已支持，作为回归保护 |
| ordinary dispatch / validation | `backendGap` | moving part、object、ReferenceN 失败要有稳定 diagnostic |
| object or subshape global transform | `backendGap` | 当前 cad-core 仍用 `PlacementN` 直接当 marker placement |
| containing part local transform / offsetPlc | `backendGap` | FreeCAD 先转 global 再转 moving part local，最后处理 bundled part offset |
| Vertex / Edge / Face representative parity | `notCollected` -> S4/S5 | 需要 checked-in FreeCAD expected 和 focused tests，三类 primitive 必须同批次 |
| mixed reference + swapJCS + current value | `backendGap` / `notCollected` -> S4/S5 | request-local swap 必须同步 reference 和 placement；current value 要保留 JCS global evidence |
| real Ondsel marker path | `releaseGate` | resolver 输出必须被 `addConstraintToOndselAssembly()` 消费 |
| RackPinion / Screw regression | `releaseGate` | 已支持，但本包统一 resolver 不能破坏 special rewrite、sliding-side swap 或 scalar routes |
| capability/docs publication | `releaseGate` | 只有 S5 parity 后才能发布 |
| radius / curve / GUI/session / connector-only shortcut | `nonGoal` | 不进入本包支持声明 |

## blocker 队列

S2 需要确认 `MP-BLOCK-001..010` 足以覆盖完整批次：

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

## 验收

```bash
rg -n "MP-SCOPE|MP-BLOCK|MP-BG|MP-NG" docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵/*.tsv
```

## 非目标

- S2 不新增 fixtures。
- S2 不修改 collector。
- S2 不发布 capability。
