# P8 GearsBeltJoint 工作步骤总入口

## 目标

把 Gears / Belt 两个 `ASMTGearJoint` 族 JointType 从 remaining unsupported 队列推进到 real Ondsel request-local supported 子集，并保持 RackPinion / Screw 和复杂 Distance geometry 不被误发布。

当前收口状态：S0 已完成 live baseline 复核，S1 已完成 FreeCAD 源码候选矩阵复核，S2 已完成范围准入与 blocker 路由，S3 已完成 `Distance2` DTO 与 `ASMTGearJoint` runtime 映射，S4 已完成 native oracle、focused tests 与半径符号复核，S5 已完成 capability 与 unsupported 矩阵发布同步，S6 已完成最终 oracle / publication gate。整个 Gears / Belt 包已支持发布。

索引关闭口径：本文件表示工作步骤索引、矩阵文件名和轻量验收命令已经复核；S0-S6 均已由各自文件关闭，队列应为空。

## 步骤索引

| 步骤 | 文件 | 当前状态 | 解决的问题 |
| --- | --- | --- | --- |
| S0 | `6-18-14-21-【已实现】P8-GearsBeltJoint-S0-声明口径与live基线复核.md` | 已实现 | 冻结支持声明、禁止声明和 current unsupported 基线 |
| S1 | `6-18-14-22-【已实现】P8-GearsBeltJoint-S1-FreeCAD源码候选矩阵.md` | 已实现 | 建立 FreeCAD / cad-core source candidates |
| S2 | `6-18-14-23-【已实现】P8-GearsBeltJoint-S2-范围准入与blocker矩阵.md` | 已实现 | 将候选路由到 unsupportedImplementable、notCollected、releaseGate、nonGoal |
| S3 | `6-18-14-24-【已实现】P8-GearsBeltJoint-S3-Distance2与ASMTGearJoint映射专项复审.md` | 已实现 | 收口 `Distance2` DTO、Gears/Belt ASMT 映射和 supported predicate |
| S4 | `6-18-14-25-【已实现】P8-GearsBeltJoint-S4-NativeOracle与半径符号专项复审.md` | 已实现 | 对齐 FreeCADCmd expected、`radiusJ` 符号和 request-local solver output |
| S5 | `6-18-14-26-【已实现】P8-GearsBeltJoint-S5-Capability与unsupported矩阵专项复审.md` | 已实现 | 同步 capabilities、focused tests、P8 docs / TSV |
| S6 | `6-18-14-27-【已实现】P8-GearsBeltJoint-S6-Oracle实现与发布闸门.md` | 已实现 | 已消费 blockers 并完成包级 oracle / publication gate |

## 执行顺序

1. 先执行 S0，确认当前 `supported_joint_matrix` / `unsupported_joint_matrix`、`JointConstraint` 和工作区状态。
2. 执行 S1 / S2，锁定 FreeCAD source authority，并把 Gears / Belt 与 RackPinion / Screw 分开。
3. 执行 S3，落地 `Distance2` DTO、`ASMTGearJoint` include / conversion 和 supported predicate。
4. 执行 S4，增加 Gears / Belt c3m6 fixtures、FreeCADCmd expected 和 focused expected parity。
5. 执行 S5，同步 capabilities、focused tests 和既有 P8 AssemblySolver 矩阵。
6. S6 已执行，blocker 与发布闸门均已关闭；S6 文件已按规则改名为 `【已实现】`。

## 当前矩阵闸门

| 矩阵 | 当前用途 | 当前结论 |
| --- | --- | --- |
| `p8_gears_belt_joint_source_candidates.tsv` | FreeCAD / cad-core 证据入口 | 已记录 S4 native expected 与 focused test 关闭，S5 已完成 supported / unsupported publication |
| `p8_gears_belt_joint_scope_review_matrix.tsv` | 当前能力路由 | GBJ-SCOPE-002 / 003 / 004 / 005 已转为 supported；GBJ-SCOPE-006 / 007 / 008 保持边界 |
| `p8_gears_belt_joint_blocker_queue.tsv` | 发布前 blocker | GBJ-BLOCK-001 / 002 已由 S3 关闭，GBJ-BLOCK-003 / 004 已由 S4 关闭，GBJ-BLOCK-005 / 006 已由 S5 关闭 |
| `p8_gears_belt_joint_non_goal_registry.tsv` | 非目标边界 | RackPinion / Screw、复杂 Distance、GUI/session 不进入本包 |
| `p8_gears_belt_joint_backend_gap_classification.tsv` | 分类与优先级 | 本包只推进 Gears / Belt |

## 状态纪律

- `supported` 只能在 build、focused tests、expected parity、capability/docs 同步全部通过后使用。
- 当前 Gears / Belt 是 request-local real Ondsel supported subset，不是完整 Assembly transaction lifecycle。
- `RackPinion` / `Screw` 保持 unsupported；复杂 Distance geometry 保持 notCollected。
- 不允许用 `ASMTGearJoint` 存在来跳过 FreeCADCmd expected。
- 不允许恢复 representative fallback 或 adapter 层业务补丁。

## 通用验收

```bash
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-GearsBeltJoint-OndselSolver收口主线
for f in docs/FreeCAD几何生态迁移工程-细分/P8-GearsBeltJoint-OndselSolver收口主线/矩阵/*.tsv; do
  awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' "$f"
done
```

代码阶段最小验收由 S6 指定。

## 非目标

- 不实现 RackPinion / Screw。
- 不实现完整 Distance geometry matrix。
- 不实现 GUI drag / postDrag 或跨请求 solver session。
- 不修改 unrelated P5/P6/P7 packages。
