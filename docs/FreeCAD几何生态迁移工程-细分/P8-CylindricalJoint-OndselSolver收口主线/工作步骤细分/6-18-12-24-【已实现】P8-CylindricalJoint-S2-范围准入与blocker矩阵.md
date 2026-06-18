# P8 CylindricalJoint S2 范围准入与 blocker 矩阵

## 目标

把 S1 候选路由成明确的 scope、blocker、nonGoal 和发布状态，避免泛化成完整 Assembly solver 复刻。

## 分类规则

| 状态 | 准入条件 | 本包动作 |
| --- | --- | --- |
| `supported` | build、focused tests、expected parity、capability/docs 同步全部通过 | 发布为最小 request-local 支持 |
| `unsupported` | FreeCAD 有语义，但本包没有 DTO / oracle / test 完整链 | 保持 diagnostic-only |
| `notCollected` | 需要 native oracle 或更细 constraint 语义 | 不进入代码实现 |
| `nonGoal` | 破坏 stateless CAD Core 边界或属于 GUI/session | 公开排除 |

## scope 路由

| scope | 当前状态 | 理由 |
| --- | --- | --- |
| `CYL-SCOPE-001` | supported | Ondsel 子模块已初始化，hard-linked build 已通过 |
| `CYL-SCOPE-002` | supported | C++ `ASMTCylindricalJoint` 映射和 supported predicate 已验收 |
| `CYL-SCOPE-003` | supported | native expected 已采集，collector `--check` 与 expected parity 已通过 |
| `CYL-SCOPE-004` | supported | capabilities、focused tests、docs/matrices 已保持 supported / unsupported matrix 一致 |
| `CYL-SCOPE-005` | unsupported | 其它 JointType 保持 diagnostic-only |
| `CYL-SCOPE-006` | nonGoal | GUI / persistent solver session 不属于本包 |

## blocker 路由

- `CYL-BLOCK-001`：恢复或确认 `src/3rdParty/OndselSolver`，关闭 build blocker。
- `CYL-BLOCK-002`：C++ `ASMTCylindricalJoint` adapter 与 supported type predicate 一致。
- `CYL-BLOCK-003`：c3m6 fixture / expected 进入 expected parity。
- `CYL-BLOCK-004`：C ABI capabilities、focused tests 和既有 P8 docs/matrices 同步。
- `CYL-BLOCK-005`：剩余 unsupported JointTypes 不被误发布。

## 验收标准

- `p8_cylindrical_joint_scope_review_matrix.tsv` 的每个 `scope_id` 都有合法状态和 `next_step`。
- `p8_cylindrical_joint_blocker_queue.tsv` 的每个 blocker 都指向一个 scope。
- `p8_cylindrical_joint_backend_gap_classification.tsv` 不得把 `notCollected` 直接变成代码实现。
- 检查命令：

```bash
for f in docs/FreeCAD几何生态迁移工程-细分/P8-CylindricalJoint-OndselSolver收口主线/矩阵/*.tsv; do
  awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"
done
rg -n "CYL-SCOPE-00[1-6]" docs/FreeCAD几何生态迁移工程-细分/P8-CylindricalJoint-OndselSolver收口主线/矩阵
rg -n "CYL-BLOCK-00[1-5]" docs/FreeCAD几何生态迁移工程-细分/P8-CylindricalJoint-OndselSolver收口主线/矩阵
```

## 非目标

- S2 不修改源代码。
- S2 记录已关闭 blocker 和剩余边界，不扩大实现范围。
- S2 不扩大到完整 Joint constraints、Distance geometry 或 CopyOnChange / Link 生命周期。
