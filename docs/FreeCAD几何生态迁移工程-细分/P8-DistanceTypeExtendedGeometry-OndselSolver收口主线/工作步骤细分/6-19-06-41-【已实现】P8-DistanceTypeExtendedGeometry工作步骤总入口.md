# P8 DistanceTypeExtendedGeometry 工作步骤总入口

## 目标

建立 P8 DistanceType extended geometry 的 S0-S6 可执行队列。它不是只补一个 radius fixture，而是按 FreeCAD `getDistanceType()` -> `getEdgeRadius()` / `getFaceRadius()` -> `makeMbdJointDistance()` 同一调用链，把所有剩余 DistanceType 纳入同一套矩阵和发布边界。

索引关闭口径：S0-S6 均已按各自步骤文件关闭；本文件作为工作步骤索引、矩阵文件名和轻量验收命令的入口保留。

## 步骤索引

| 步骤 | 文件 | 当前状态 | 解决的问题 |
| --- | --- | --- | --- |
| S0 | `6-19-06-42-【已实现】P8-DistanceTypeExtendedGeometry-S0-声明口径与live基线复核.md` | 已实现 | 已冻结当前 supported basic subset、MarkerPlacement subset、剩余 DistanceType 范围和非目标 |
| S1 | `6-19-06-43-【已实现】P8-DistanceTypeExtendedGeometry-S1-FreeCAD源码候选矩阵.md` | 已实现 | 已复核 DistanceType enum、classification、radius helper、ASMT switch、cad-core landing |
| S2 | `6-19-06-44-【已实现】P8-DistanceTypeExtendedGeometry-S2-范围准入与blocker矩阵.md` | 已实现 | 已冻结 remaining cases 的 implementation batch、oracle-first、default/TODO boundary、nonGoal 和 blocker ownership |
| S3 | `6-19-06-45-【已实现】P8-DistanceTypeExtendedGeometry-S3-RadiusPrimitive证据专项复审.md` | 已实现 | 已补 DTO / JSON radius evidence、primitive resolver、scalar correction 和 boundary evidence |
| S4 | `6-19-06-46-【已实现】P8-DistanceTypeExtendedGeometry-S4-OndselDistanceJoint扩展映射专项复审.md` | 已实现 | 已裁决显式 extended cases 的 ASMT class、`distanceIJ` / `offset`，并保持 default branch unmapped |
| S5 | `6-19-06-47-【已实现】P8-DistanceTypeExtendedGeometry-S5-NativeOracle与代表fixture专项复审.md` | 已实现 | 已批量采集 FreeCADCmd expected，并把 supported-candidate / diagnostic / nonGoal 固化 |
| S6 | `6-19-06-48-【已实现】P8-DistanceTypeExtendedGeometry-S6-实现与发布闸门.md` | 已实现 | 已落 C++、fixtures、focused tests、capability/docs 和矩阵回写 |

## 执行顺序

1. S0 只复核 live baseline，不写 C++，不采 oracle。
2. S1 补全 source candidates，不把候选直接写成 supported。
3. S2 按最小完整语义批次分类；显式 switch cases 原则上同批实现，default / TODO cases 必须有边界结论。
4. S3 先补 radius / primitive evidence 设计；没有 DTO 和 JSON evidence 不进入 native expected 判断。
5. S4 已裁决 `ASMTRevCylJoint`、`ASMTCylSphJoint`、`ASMTPlanarJoint`、`ASMTPointInPlaneJoint`、`ASMTLineInPlaneJoint`、`ASMTSphSphJoint` 等映射。
6. S5 批量采集 representative native expected；oracle 不稳定的 case 保留 notCollected 或 nonGoal，不写 C++ 猜测。
7. S6 已做实现和发布，并已按仓库规则改名为 `【已实现】`；后续只在 product decision 重开 PointCurve 或 default/TODO branch 时另开专包。

## 当前矩阵闸门

| 矩阵 | 当前用途 | 当前结论 |
| --- | --- | --- |
| `p8_distance_type_extended_geometry_source_candidates.tsv` | FreeCAD / cad-core source authority | S6 已消费 source authority 并发布 13-case supported subset |
| `p8_distance_type_extended_geometry_scope_review_matrix.tsv` | 范围分类 | S6 已关闭 supported / diagnostic / nonGoal 边界 |
| `p8_distance_type_extended_geometry_blocker_queue.tsv` | 可执行 blocker | DTE-BLOCK-001..008 均已关闭为 consumed、supported、diagnostic/nonGoal 或 supported-with-diagnostics |
| `p8_distance_type_extended_geometry_backend_gap_classification.tsv` | gap 聚合 | supported subset 不再保留 backendGap；PointCurve/default/TODO 仍是 diagnostic/nonGoal |
| `p8_distance_type_extended_geometry_non_goal_registry.tsv` | 非目标和 reopen 条件 | PointCurve/default/TODO、GUI/session、persistent solver state、unsupported default claim 等仍不得发布 |

## 通用验收

```bash
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线/矩阵/*.tsv
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线/工作步骤细分 --format markdown
```

S6 最终验收命令见 `6-19-06-48-【已实现】P8-DistanceTypeExtendedGeometry-S6-实现与发布闸门.md`。

## 非目标

- 不把本包压缩成单 fixture 实现。
- 不重新采集或修改已通过的 basic DistanceType expected。
- 不发布 FreeCAD TODO / default branch 为 full support。
- 不实现 GUI/session、persistent solver state 或完整 Assembly transaction。
