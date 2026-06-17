# P6 MakerHistory 工作步骤总入口

## 目标

建立 P6 MakerHistory / ShapeFix / DressUp / Taper 的 S0-S6 执行索引，先复核当前 live 状态，再决定哪些项是文档发布闸门、oracle 缺口、真实 backendGap 或非目标。

当前收口状态：S0 live 口径已冻结；S1 FreeCAD 源码候选矩阵已复核；S2 范围准入与 blocker 矩阵已完成；S3 ShapeFix History 专项复审已完成；S4 DressUp / Refine 传播专项复审已完成；S5 taper partial/full history 专项复审已完成；S6 发布闸门已完成。当前没有 C++ backendGap；只保留复杂 split / deleted 旧引用恢复的 `notCollected` oracle 队列。

总入口复核状态：2026-06-17 S0 已复核 live docs、C ABI capabilities、focused tests 和矩阵 seed；S1 已把 FreeCAD source authority 和 cad-core landing 补成 10 条候选，且只更新 scope 的 source_candidate 反链；S2 已把 6 个 scope 分类为 4 个 `releaseGate`、1 个 `notCollected`、1 个发布闸门聚合行；S3 已把 ShapeFix producer 裁决为 `supported`；S4 已把 DressUp / Refine / transformed 传播裁决为 `supported`；S5 已把 taper ThruSections history 裁决为 `supported`；S6 已完成正式文档发布回写，当前无 `backendGap`。

## 步骤索引

| 步骤 | 文件 | 当前状态 | 解决的问题 |
| --- | --- | --- | --- |
| S0 | [声明口径与 live 基线复核](6-17-17-52-【已实现】P6-MakerHistory-S0-声明口径与live基线复核.md) | 已完成：live 口径冻结 | 消解 P6 文档与 C ABI capabilities / tests 的状态漂移 |
| S1 | [FreeCAD 源码候选矩阵](6-17-17-53-【已实现】P6-MakerHistory-S1-FreeCAD源码候选矩阵.md) | 已完成：候选矩阵已复核 | 建立 ShapeFix / DressUp / taper / transformed source authority |
| S2 | [范围准入与 blocker 矩阵](6-17-17-54-【已实现】P6-MakerHistory-S2-范围准入与blocker矩阵.md) | 已完成：分类矩阵已冻结 | 把候选分类成 supported / releaseGate / notCollected / backendGap / nonGoal |
| S3 | [ShapeFix History 专项复审](6-17-17-55-【已实现】P6-MakerHistory-S3-ShapeFix-History专项复审.md) | 已完成：ShapeFix producer supported | 复核 ShapeFix_Root / ReShape producer 是否仍缺 generated / deleted / modified 证据 |
| S4 | [DressUp / Refine 传播专项复审](6-17-17-56-【已实现】P6-MakerHistory-S4-DressUp-Refine-传播专项复审.md) | 已完成：DressUp / Refine / transformed supported | 复核 DressUp AddSubShape、Refine 和 transformed 链路传播 |
| S5 | [Taper Partial History 专项复审](6-17-17-57-【已实现】P6-MakerHistory-S5-Taper-Partial-History专项复审.md) | 已完成：taper covered_full accepted | 复核 taper `known_gap` 是否已关闭或仍缺 oracle / C++ |
| S6 | [Oracle 实现与发布闸门](6-17-17-58-【已实现】P6-MakerHistory-S6-Oracle实现与发布闸门.md) | 已完成：发布漂移关闭，保留 notCollected oracle 队列 | 消费 S2-S5 队列，执行 C++ 或文档/capability 发布 |

## 执行顺序

1. 先做 S0：只复核 live docs、capabilities、tests 和当前 git 状态，不写 C++。
2. 再做 S1：只补源码候选和 cad-core landing，不把候选提升为 supported。
3. S2 分类所有 scope，禁止无 FreeCAD 依据和 cad-core mismatch 的 `backendGap`。
4. S3-S5 分别复核 ShapeFix、DressUp/Refine/transformed、taper 三个高风险边界。
5. S6 只消费 `notCollected`、`backendGap`、`unsupported`、`releaseGate`，并按队列决定 C++、expected 或文档发布。

## 当前矩阵闸门

| 矩阵 | 当前用途 | 当前结论 |
| --- | --- | --- |
| `p6_maker_history_source_candidates.tsv` | FreeCAD source 候选 | S1 已复核 10 条候选；S2-S5 再裁决状态 |
| `p6_maker_history_scope_review_matrix.tsv` | scope 状态分类 | S6 后为 5 个 `supported`、1 个 `notCollected`、0 个 `backendGap` |
| `p6_maker_history_blocker_queue.tsv` | 可执行 blocker | `P6MH-BLOCK-001..004` 已关闭；`P6MH-BLOCK-005` 保留为后续复杂 split / deleted oracle 队列 |
| `p6_maker_history_non_goal_registry.tsv` | 非目标边界 | S2 已保留 `P6MH-NG-001..005`，每行都有用户 / 协议表现和 reopen 条件 |
| `p6_maker_history_backend_gap_classification.tsv` | backendGap 聚合 | S6 已确认当前无 backendGap；复杂 oracle 未采前不得升级 |

## 状态纪律

- `supported` 必须有当前源码、fixture/focused test 或 capability 证据。
- `releaseGate` 用于能力已可能实现但文档 / capability / expected 状态未一致的项。
- `notCollected` 只能先采 FreeCAD oracle 或补 checked-in expected。
- `backendGap` 必须同时有 FreeCAD source authority 和当前 cad-core mismatch evidence。
- 不得按 fixture 名称、几何类型、面积/长度、输出顺序、adapter 输出修剪或跨请求 BREP 状态实现历史恢复。

## 通用验收

```bash
git diff --check
for f in docs/FreeCAD几何生态迁移工程-细分/P6-MakerHistory-ShapeFix-DressUp-Taper收敛主线/矩阵/*.tsv; do
  awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"
done
```

代码实现阶段再按 touched scope 执行 focused tests；文档包创建阶段不跑 cad-core build。

## 非目标

- 不迁移完整 Sketcher constraint solver。
- 不实现完整 FreeCAD ShapeFix Python API、GUI / Workbench 行为或 ViewProvider。
- 不把 BREP、shape、mesh、NamedShape 或 ElementMap 变成跨请求持久状态。
- 不把 Assembly solver、完整 Link 写回事务或 Worker / WASM / Web adapter 纳入本主线。
