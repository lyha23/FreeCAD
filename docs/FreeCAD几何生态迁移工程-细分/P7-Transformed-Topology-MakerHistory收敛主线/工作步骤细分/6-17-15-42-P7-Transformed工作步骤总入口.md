# 【已实现】P7 Transformed 工作步骤总入口

## 目标

建立 P7 transformed family 的 S0-S6 执行索引，驱动 Mirrored / LinearPattern / PolarPattern / Scaled / MultiTransform 的 topology oracle 和 maker-history 收敛。本文只做索引，真实审计和实现由 S0-S6 执行。

当前收口状态：S0、S1、S2、S3、S4、S5、S6 均已实现。已建立矩阵骨架、完成 FreeCAD source authority / cad-core route 复核、scope / blocker / backendGap / nonGoal 分类、topology oracle 专项复审、Pattern Ownership 专项复审、MultiTransform / fallback 专项复审和 S6 C++ / expected landing；SCOPE-001 到 SCOPE-005 已由 backendGap 转 supported。

## 步骤索引

| 步骤 | 文件 | 当前状态 | 解决的问题 |
| --- | --- | --- | --- |
| S0 | [声明口径与 live 基线复核](6-17-15-43-【已实现】P7-Transformed-S0-声明口径与live基线复核.md) | 已实现 | 冻结“bbox/volume-only topology 未收敛”的表达边界 |
| S1 | [FreeCAD 源码候选矩阵](6-17-15-44-【已实现】P7-Transformed-S1-FreeCAD源码候选矩阵.md) | 已实现 | 建立 FreeCAD transformed 源码和 cad-core 落点候选 |
| S2 | [范围准入与 blocker 矩阵](6-17-15-45-【已实现】P7-Transformed-S2-范围准入与blocker矩阵.md) | 已实现 | 把候选分类成 supported / notCollected / backendGap / releaseGate / nonGoal |
| S3 | [Topology Oracle 专项复审](6-17-15-46-【已实现】P7-Transformed-S3-Topology-Oracle专项复审.md) | 已实现 | 审计 bbox/volume-only expected 是否可采 topology_counts |
| S4 | [Pattern Ownership 专项复审](6-17-15-47-【已实现】P7-Transformed-S4-Pattern-Ownership专项复审.md) | 已实现 | 审计 AddSubShape slot、source alias、terminal history；确认 ownership release evidence supported/covered |
| S5 | [MultiTransform Fallback 专项复审](6-17-15-48-【已实现】P7-Transformed-S5-MultiTransform-Fallback专项复审.md) | 已实现 | 审计组合语义、Scaled diagonal 和 fallback 边界；确认无新增 S6 前置 blocker |
| S6 | [Oracle 实现与发布闸门](6-17-15-49-【已实现】P7-Transformed-S6-Oracle实现与发布闸门.md) | 已实现 | 已消费 blocker，落 C++、P7 expected 与 focused tests |

## 执行顺序

1. 先做 S0，确认当前 P7 文档、fixtures 和 expected 的 live 基线。
2. 再做 S1，补齐 FreeCAD source candidates 和 cad-core landing。
3. S2 只做分类，不写 C++。
4. S3 先采或确认 topology oracle；没有 oracle 的项不能直接实现。
5. S4 审计 mapper-history / ownership 是否已经能解释 topology。
6. S5 审计 MultiTransform 组合和 fallback 边界。
7. S6 已消费 S2-S5 留下的 collected oracle backendGap；standalone nonGoal 继续保留。

## 当前矩阵闸门

| 矩阵 | 当前用途 | 当前结论 |
| --- | --- | --- |
| `p7_transformed_source_candidates.tsv` | FreeCAD source 候选 | S1 已完成 source authority 和 cad-core route，未判定 backendGap |
| `p7_transformed_scope_review_matrix.tsv` | scope 状态分类 | SCOPE-001 到 SCOPE-005 已由 S6 转为 `supported`；SCOPE-006 ownership release evidence 和 SCOPE-007 source alias baseline 保持 `supported` |
| `p7_transformed_blocker_queue.tsv` | 可执行 blocker | P7T-BLOCK-001 到 P7T-BLOCK-004 已由 S6 关闭；P7T-BLOCK-005 的 ownership / fallback / composition release evidence 已由 S4/S5/S6 关闭 |
| `p7_transformed_non_goal_registry.tsv` | 非目标边界 | S2 已固化 5 个 `nonGoal` 用户 / 协议行为和 reopen 条件 |
| `p7_transformed_backend_gap_classification.tsv` | backendGap 聚合 | collected-oracle backendGap 已关闭为 supported；ownership / fallback / composition supported/covered；standalone lifecycle boundary 保持 nonGoal |

## 状态纪律

- `notCollected` 只能先采 FreeCAD oracle 或补 checked-in expected。
- `backendGap` 必须有 FreeCAD 源码依据和当前 cad-core mismatch evidence。
- topology 命名顺序差异不等于失败；数量、几何内容、stable subname 语义不稳定才进入 blocker。
- transformed ownership 不得靠 fixture 名称、几何类型、source index、transform order、bbox/volume 或面积/长度猜测。

## 通用验收

```bash
git diff --check
for f in docs/FreeCAD几何生态迁移工程-细分/P7-Transformed-Topology-MakerHistory收敛主线/矩阵/*.tsv; do
  awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"
done
```

代码实现阶段再按 touched scope 运行 focused P7 tests；文档包创建阶段不跑 cad-core build。

## 非目标

- 不迁移完整 Sketcher solver。
- 不扩大到 P8 Assembly solver、完整 Link 账本或跨请求事务。
- 不把导入 shape 完整 ElementMap、Worker / WASM / Web adapter 产品化纳入本主线。
- 不把 BREP 或 shape cache 引入长期状态；仍保持 CAD Core 无状态边界。
