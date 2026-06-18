# P8 ScrewRackPinionJoint S2 范围准入与 blocker 矩阵

## 目标

把 S1 候选路由成明确的 scope、blocker、nonGoal 和 releaseGate，确保本包只实现 Screw / RackPinion 及其 shared sliding 前置。

## 分类规则

| 状态 | 准入条件 | 本包动作 |
| --- | --- | --- |
| `unsupportedImplementable` | 当前发布为 unsupported，但有直接 FreeCAD source、cad-core 落点和可建 fixture/oracle | 进入 S3-S6 |
| `notCollected` | 需要 native oracle 或 expected | S5 采集或验证后再进入 supported |
| `releaseGate` | 实现后需同步 capability / docs / tests | S5-S6 关闭 |
| `unsupported` | FreeCAD 有语义但本包没有完整 DTO/oracle/test | 保持 diagnostic-only |
| `nonGoal` | 与 stateless CAD Core 边界冲突 | 公开排除 |

## scope 路由

| scope | 当前状态 | 理由 |
| --- | --- | --- |
| `SRJ-SCOPE-001` | supportedBaseline | P8 real Ondsel、placement writeback、Gears/Belt 等已作为上游基线 |
| `SRJ-SCOPE-002` | unsupportedImplementable | shared `slidingPartIndex()` / `swapJCS()` 是 Screw / RackPinion 共同前置 |
| `SRJ-SCOPE-003` | unsupportedImplementable | Screw 是直接 `ASMTScrewJoint(pitch=Distance)`，缺 shared sliding 前置和 adapter conversion |
| `SRJ-SCOPE-004` | unsupportedImplementable | RackPinion 是 `ASMTRackPinionJoint(pitchRadius=Distance)`，缺 marker rewrite 和 adapter conversion |
| `SRJ-SCOPE-005` | notCollected | Screw / RackPinion native expected 尚未入库 |
| `SRJ-SCOPE-006` | releaseGate | capabilities、focused tests、P8 docs / TSV 需要同步 |
| `SRJ-SCOPE-007` | notCollected | complex Distance geometry 不由本包实现 |
| `SRJ-SCOPE-008` | nonGoal | GUI / persistent solver session / full Assembly transaction 不属于本包 |

## blocker 路由

- `SRJ-BLOCK-001`：`slidingPartIndex()` / `swapJCS()` request-local 前置。
- `SRJ-BLOCK-002`：Screw `ASMTScrewJoint` mapping、pitch field、supported predicate。
- `SRJ-BLOCK-003`：RackPinion `ASMTRackPinionJoint` mapping、pitchRadius field。
- `SRJ-BLOCK-004`：RackPinion marker side detection 与 rack marker rotation rewrite。
- `SRJ-BLOCK-005`：Screw / RackPinion native expected、fixtures、focused runtime assertions。
- `SRJ-BLOCK-006`：capabilities、unsupported matrix 和 P8 upstream matrix 同步。
- `SRJ-BLOCK-007`：complex Distance、GUI/session、full transaction 边界保护。

## 验收标准

- `p8_screw_rackpinion_joint_scope_review_matrix.tsv` 每个 `scope_id` 都有合法状态和 `next_step`。
- `p8_screw_rackpinion_joint_blocker_queue.tsv` 每个 blocker 都指向一个 scope。
- `p8_screw_rackpinion_joint_backend_gap_classification.tsv` 不得把 complex Distance 混入本轮 implementation。
- 检查命令：

```bash
for f in docs/FreeCAD几何生态迁移工程-细分/P8-ScrewRackPinionJoint-OndselSolver收口主线/矩阵/*.tsv; do
  awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' "$f"
done
rg -n "SRJ-SCOPE-00[1-8]|SRJ-BLOCK-00[1-7]|unsupportedImplementable|notCollected|releaseGate|unsupported|nonGoal" docs/FreeCAD几何生态迁移工程-细分/P8-ScrewRackPinionJoint-OndselSolver收口主线/矩阵
```

## 非目标

- S2 不修改源代码。
- S2 不关闭 blocker，只建立可执行队列。
- S2 不扩大到完整 Distance geometry、GUI drag / postDrag、Reverse UI 或 Link lifecycle。
