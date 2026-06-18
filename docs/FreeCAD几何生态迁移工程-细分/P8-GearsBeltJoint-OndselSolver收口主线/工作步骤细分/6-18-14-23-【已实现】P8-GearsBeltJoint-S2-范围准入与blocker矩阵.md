# P8 GearsBeltJoint S2 范围准入与 blocker 矩阵

## 目标

把 S1 候选路由成明确的 scope、blocker、nonGoal 和 releaseGate，确保本包只实现 Gears / Belt。

当前状态：已完成。S2 只关闭范围准入和 blocker 队列路由，不关闭任何 blocker，不声明 Gears / Belt supported。

## 分类规则

| 状态 | 准入条件 | 本包动作 |
| --- | --- | --- |
| `unsupportedImplementable` | 当前发布为 unsupported，但有直接 FreeCAD source、cad-core 落点和可建 fixture/oracle | 进入 S3-S6 |
| `notCollected` | 需要 native oracle 或 expected | S4 采集或验证后再进入 supported |
| `releaseGate` | 实现后需同步 capability / docs / tests | S5-S6 关闭 |
| `unsupported` | FreeCAD 有语义但本包没有完整 DTO/oracle/test | 保持 diagnostic-only |
| `nonGoal` | 与 stateless CAD Core 边界冲突 | 公开排除 |

## scope 路由

| scope | 当前状态 | 理由 |
| --- | --- | --- |
| `GBJ-SCOPE-001` | supportedBaseline | P8 real Ondsel、placement writeback、Cylindrical、Parallel、Perpendicular 已作为上游基线 |
| `GBJ-SCOPE-002` | unsupportedImplementable | Gears 是直接 `ASMTGearJoint` 映射，缺 `Distance2` DTO 和 cad-core conversion |
| `GBJ-SCOPE-003` | unsupportedImplementable | Belt 是直接 `ASMTGearJoint` 映射，差异是 `radiusJ=-Distance2` |
| `GBJ-SCOPE-004` | notCollected | Gears / Belt native expected 尚未入库 |
| `GBJ-SCOPE-005` | releaseGate | capabilities、focused tests、P8 docs / TSV 需要同步 |
| `GBJ-SCOPE-006` | unsupported | RackPinion / Screw 需要 special marker 或 sliding part 语义 |
| `GBJ-SCOPE-007` | notCollected | complex Distance geometry 不由本包实现 |
| `GBJ-SCOPE-008` | nonGoal | GUI / persistent solver session 不属于本包 |

## blocker 路由

- `GBJ-BLOCK-001`：`Distance2` DTO 和 request parser。
- `GBJ-BLOCK-002`：Gears / Belt `ASMTGearJoint` mapping 和 supported predicate。
- `GBJ-BLOCK-003`：Gears native expected / fixture / focused assertion。
- `GBJ-BLOCK-004`：Belt native expected / fixture / focused assertion。
- `GBJ-BLOCK-005`：capabilities 和 unsupported matrix 同步。
- `GBJ-BLOCK-006`：remaining RackPinion / Screw / complex Distance 边界保护。

## 关闭结论

- `GBJ-SCOPE-002` / `GBJ-SCOPE-003` 继续保持 `unsupportedImplementable`，只进入 S3-S6 的 DTO、adapter、oracle 和发布闸门。
- `GBJ-SCOPE-004` 继续保持 `notCollected`，Gears / Belt native expected 必须由 S4 采集或验证后才能支持声明。
- `GBJ-SCOPE-005` 继续保持 `releaseGate`，capability、focused tests、P8 docs / TSV 必须由 S5-S6 同步。
- `GBJ-SCOPE-006`、`GBJ-SCOPE-007`、`GBJ-SCOPE-008` 分别保持 `unsupported`、`notCollected`、`nonGoal`；RackPinion / Screw、complex Distance 和 GUI/session 不进入本包实现。
- `GBJ-BLOCK-001` 到 `GBJ-BLOCK-006` 均指向有效 scope，并保留 `next_step` 与 `close_condition`；S2 不尝试关闭这些 blocker。

## 验收标准

- `p8_gears_belt_joint_scope_review_matrix.tsv` 每个 `scope_id` 都有合法状态和 `next_step`。
- `p8_gears_belt_joint_blocker_queue.tsv` 每个 blocker 都指向一个 scope。
- `p8_gears_belt_joint_backend_gap_classification.tsv` 不得把 RackPinion / Screw 混入本轮 implementation。
- 检查命令：

```bash
for f in docs/FreeCAD几何生态迁移工程-细分/P8-GearsBeltJoint-OndselSolver收口主线/矩阵/*.tsv; do
  awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' "$f"
done
rg -n "GBJ-SCOPE-00[1-8]|GBJ-BLOCK-00[1-6]" docs/FreeCAD几何生态迁移工程-细分/P8-GearsBeltJoint-OndselSolver收口主线/矩阵
```

## 非目标

- S2 不修改源代码。
- S2 不关闭 blocker，只建立可执行队列。
- S2 不扩大到完整 Joint constraints、RackPinion marker rewrite、Screw sliding part 或 Link lifecycle。
