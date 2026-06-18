# P8 DistanceTypeExtendedGeometry S2 范围准入与 blocker 矩阵【已实现】

## 目标

按最小完整语义批次冻结 scope / blocker / backend / nonGoal 矩阵。S2 只做文档和矩阵收口，不写 C++，不采 oracle，不修改 fixtures / expected。

## live 基线

- 复核仓库：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=e94b5e51a1`，最新提交为 `e94b5e51a1 docs: 完成P8扩展DistanceType S1源码矩阵复核`。
- 复核开始时工作区仅见 unrelated `AGENTS.md` dirty；本步骤未编辑或暂存 `AGENTS.md`。
- S0 已冻结 basic / marker baseline 和本包非目标；S1 已用 `AssemblyUtils.h::DistanceType`、`AssemblyUtils.cpp::getDistanceType()` / `getEdgeRadius()` / `getFaceRadius()`、`AssemblyObject.cpp::makeMbdJointDistance()` 证明 FreeCAD 37 个 enum 中 6 个 basic baseline 之外的 31 个值全部进入 `DTE-SCOPE-004..009`。

## live 结论

- `DTE-BLOCK-001` 在 S2 被标记为 closed/consumed：S0 确认现有 basic support 不重做，S1 确认 remaining enum 没有漏项，S2 只保留该项作为后续 grep / queue 守门。
- `DTE-SCOPE-004..007` 是后续最小完整 implementation batch，不允许缩成单个 radius fixture：edge-circle、face-radius、torus/sphere explicit switch、`PointCurve` 必须按同一 FreeCAD 调用链经历 S3 DTO evidence、S4 ASMT mapping、S5 native oracle，再由 S6 决定 supported / diagnostic / nonGoal。
- `DTE-SCOPE-008..009` 保持 default / TODO boundary：cone、line-surface、curve-face 和 `Other` 仍纳入审计，但因为 FreeCAD 当前走 default 或 TODO-like 语义，必须先有 native oracle 和产品决策；若本包 S6 不实现，只能以 diagnostic / nonGoal 留边界，并写明 reopen 条件。
- `DTE-BG-001..008` 不声明 backendGap。当前分类只允许 `releaseGate`、`notCollected`、`oracleFirst`、`defaultBoundary` 或 `publicationGate`；只有 S5 产生 checked-in native oracle 且 S6/cad-core 实测出现 mismatch 后，才能把具体 case 改成 backendGap。
- `DTE-BLOCK-002..008` 保持 open，分别归属 S3、S4、S5、S6；S2 不关闭 S3-S6 blocker，不删除 scope，不发布 capability。

## batching 纪律

1. 后续实现不得从 `LineCircle` 或任一单 fixture 倒推 FreeCAD 语义；必须先补 request-local primitive / radius evidence，再补 mapping 和 expected。
2. 显式 switch cases 若因 native oracle 不稳定需要拆分，拆分只能发生在 S5 之后，并且必须保留下一批次范围、reopen 条件和 capability 不发布声明。
3. default / TODO cases 允许作为 diagnostic 或 nonGoal 暂停，但不得从矩阵消失，也不得出现在 `distance_type_extended_geometry.supported` 中。

## 输出

- 已更新 scope / blocker / backend / nonGoal 矩阵：冻结 `DTE-SCOPE-004..007` 主批次，保留 `DTE-SCOPE-008..009` default/TODO 边界，补齐 blocker ownership 和 close condition，并明确 backendGap 创建前置条件。
- 已更新总入口和步骤索引，只把 S2 标记为 `【已实现】`；S3-S6 仍为 pending。

## 验收

```bash
rg -n 'DTE-SCOPE-00[1-9]|DTE-SCOPE-010|DTE-SCOPE-011|DTE-BLOCK-00[1-8]|DTE-BG-00[1-8]' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线/矩阵/*.tsv
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线/工作步骤细分 --format markdown
```

## 非目标

- 不通过删 scope 缩小批次。
- 不把 oracle 缺失改写成 C++ backendGap。
- 不关闭 S3-S6 blocker。
- 不发布 supported capability。
