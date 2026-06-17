# 【已实现】P7 Transformed S2 范围准入与 blocker 矩阵

## 目标

把 S1 候选压成 scope review、blocker queue、nonGoal registry 和 backend gap classification。S2 只做分类和路由，不采 oracle，不写 C++。

## scope review 字段

```text
scope_id	source_candidate	semantic_item	current_status	scope_reason	cad_core_landing	fixture_or_test_route	next_step
```

## 分类规则

| 状态 | 进入条件 | 下一步 |
| --- | --- | --- |
| `supported` | 已有 FreeCAD source、cad-core 实现和 checked-in topology / history evidence | 保持 regression |
| `notCollected` | native expected 只冻结 bbox / volume，尚未采 topology_counts | 进入 S3 oracle 队列 |
| `backendGap` | 已采 FreeCAD topology / history oracle 且 cad-core mismatch | 进入 S6 C++ 实现 |
| `releaseGate` | 现有实现可能可发布，但 fallback / capability / exactness 需审计 | 进入 S4/S5/S6 |
| `unsupported` | FreeCAD 语义可见但本主线不支持或证据不足 | 输出 diagnostic 或保留非目标 |
| `nonGoal` | 明确排除 | 写入 nonGoal registry |

## blocker 路由

| blocker 类型 | 来源状态 | 处理步骤 |
| --- | --- | --- |
| topology oracle | `notCollected` | S3 采 FreeCAD expected 或说明采集阻塞 |
| ownership release gate | `releaseGate` | S4 审计 source alias / AddSubShape slot / terminal history |
| MultiTransform composition | `notCollected` / `releaseGate` | S5 审计 multiplication / diagonal / Whole shape |
| backend implementation | `backendGap` | S6 落 C++ 和 focused tests |

## nonGoal 要求

每个 nonGoal 必须说明：

- 排除原因。
- 用户或协议看到什么行为。
- 重新打开的条件。

## TSV 验证

```bash
for f in docs/FreeCAD几何生态迁移工程-细分/P7-Transformed-Topology-MakerHistory收敛主线/矩阵/*.tsv; do
  awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"
done
```

## 必须回写的矩阵行

- `P7T-SCOPE-001` 到 `P7T-SCOPE-005`：bbox / volume-only topology oracle 队列。
- `P7T-SCOPE-006`：transformed pattern full history release gate。
- `P7T-NG-001` 到 `P7T-NG-005`：非目标边界。

## 完成结果

- `p7_transformed_scope_review_matrix.tsv`：`P7T-SCOPE-001` 到 `P7T-SCOPE-005` 保持 `notCollected`，原因均指向 bbox / volume-only topology oracle；`P7T-SCOPE-006` 保持 `releaseGate`；`P7T-SCOPE-007` 保持 `supported` baseline。
- `p7_transformed_blocker_queue.tsv`：`P7T-BLOCK-001` 到 `P7T-BLOCK-004` 路由到 S3 topology oracle；`P7T-BLOCK-005` 路由到 S4/S5 release audit；各行 `close_condition` 均要求 checked-in oracle、documented blocker、source audit 或 concrete backendGap 证据。
- `p7_transformed_backend_gap_classification.tsv`：分类为 `oracle-pending-not-backendGap`、`releaseGate`、`standalone lifecycle boundary`；只有已采 FreeCAD topology / history oracle 且出现 cad-core mismatch，才能进入 `backendGap`。
- `p7_transformed_non_goal_registry.tsv`：固化 complete Sketcher solver、Assembly solver、完整 transformed 参数全集、跨请求 BREP/cache、standalone geometry-equivalent native golden 的用户 / 协议行为和 reopen 条件。

## 非目标

- 不把 notCollected 当成实现任务。
- 不把 topology 命名顺序差异直接归为失败。
- 不把 standalone geometry-equivalent fixture 伪装成 native FreeCAD oracle。
