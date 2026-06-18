# P8 CylindricalJoint 工作步骤总入口

## 目标

把 `Cylindrical` JointType 收口成一个可执行的 P8 Assembly follow-up：先确认 FreeCAD 源码和当前 live tree，再通过 S3-S6 关闭 Ondsel 子模块构建、adapter 映射、native expected、capability/test 发布闸门。

当前收口状态：S0 到 S6 均已实现。`Cylindrical` 已作为最小 request-local JointType 支持发布：cad-core real Ondsel adapter 转换 `ASMTCylindricalJoint`，native FreeCAD expected 已入库并通过 parity，capabilities / P8 docs / TSV 矩阵已同步。Parallel / Perpendicular / RackPinion / Screw / Gears / Belt、Cylindrical limits 和完整 Assembly 事务仍不属于本包。

## 步骤索引

| 步骤 | 文件 | 当前状态 | 解决的问题 |
| --- | --- | --- | --- |
| S0 | `6-18-12-22-【已实现】P8-CylindricalJoint-S0-声明口径与live基线复核.md` | 已实现 | 冻结支持声明、非目标和构建闸门 |
| S1 | `6-18-12-23-【已实现】P8-CylindricalJoint-S1-FreeCAD源码候选矩阵.md` | 已实现 | 建立 FreeCAD / cad-core source candidates |
| S2 | `6-18-12-24-【已实现】P8-CylindricalJoint-S2-范围准入与blocker矩阵.md` | 已实现 | 将候选路由到 supported、unsupported、nonGoal |
| S3 | `6-18-12-25-【已实现】P8-CylindricalJoint-S3-Ondsel子模块与构建闸门专项复审.md` | 已实现 | 关闭 `src/3rdParty/OndselSolver` 缺失导致的 build blocker |
| S4 | `6-18-12-26-【已实现】P8-CylindricalJoint-S4-JointType映射专项复审.md` | 已实现 | 验证 `Cylindrical -> ASMTCylindricalJoint` 映射和 supported matrix |
| S5 | `6-18-12-27-【已实现】P8-CylindricalJoint-S5-NativeOracle与capability发布专项复审.md` | 已实现 | 对齐 native expected、capabilities、tests、docs / TSV |
| S6 | `6-18-12-28-【已实现】P8-CylindricalJoint-S6-Oracle实现与发布闸门.md` | 已实现 | 关闭发布闸门并记录验收证据 |

## 执行顺序

1. S0 已确认本包只处理 Cylindrical request-local JointType，不扩张完整 Joint lifecycle。
2. S1/S2 已复核 source candidates，并把能力路由到 supported、unsupported 和 nonGoal。
3. S3 已通过子模块初始化和 hard-linked build 关闭构建闸门。
4. S4 已收口 C++ 映射和 supported / unsupported matrix。
5. S5 已锁定 native expected、focused tests 和既有 P8 包中的必要引用更新。
6. S6 已关闭 blocker 并记录最终验收。

## 当前矩阵闸门

| 矩阵 | 当前用途 | 当前结论 |
| --- | --- | --- |
| `p8_cylindrical_joint_source_candidates.tsv` | FreeCAD / cad-core 证据入口 | 已复核并对齐 C++ / fixture / expected / capabilities |
| `p8_cylindrical_joint_scope_review_matrix.tsv` | 当前能力路由 | `CYL-SCOPE-001..004` 已转 supported；`CYL-SCOPE-005` unsupported，`CYL-SCOPE-006` nonGoal |
| `p8_cylindrical_joint_blocker_queue.tsv` | 发布前 blocker | `CYL-BLOCK-001..005` 已关闭 |
| `p8_cylindrical_joint_non_goal_registry.tsv` | 非目标边界 | 完整 Joint 事务、GUI drag、额外复杂 JointType 不进入本包 |
| `p8_cylindrical_joint_backend_gap_classification.tsv` | 分类与优先级 | build / Cylindrical 发布闸门已关闭，remaining JointTypes 保持 unsupported |

## 状态纪律

- `supported` 只能在 build、focused tests、expected parity 和 capability/docs 同步后使用。
- 当前代码和 fixture 已通过构建与验收，可标为 `已实现` / `supported`。
- `unsupported` 只保留给 Parallel / Perpendicular / RackPinion / Screw / Gears / Belt 以及未采集的复杂 Distance / limit 语义。
- 不允许恢复 `representative_ondsel_solver` 或无 Ondsel fallback 来绕过构建问题。
- 不允许在 adapter、expected JSON 或 fixture 名称中补业务逻辑。

## 通用验收

```bash
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-CylindricalJoint-OndselSolver收口主线
for f in docs/FreeCAD几何生态迁移工程-细分/P8-CylindricalJoint-OndselSolver收口主线/矩阵/*.tsv; do
  awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"
done
```

代码阶段最小验收已由 S6 执行并记录。

## 非目标

- 不实现 GUI / Workbench / drag / postDrag 生命周期。
- 不引入跨请求 MBD solver session。
- 不实现 RackPinion / Screw / Gears / Belt、Parallel / Perpendicular 或完整 Distance geometry。
- 不把 Assembly Link / CopyOnChange / Web adapter 产品化混入本包。
