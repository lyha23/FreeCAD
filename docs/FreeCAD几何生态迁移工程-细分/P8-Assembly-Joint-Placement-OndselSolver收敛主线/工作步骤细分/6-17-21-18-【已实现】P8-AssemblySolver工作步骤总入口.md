# 【已实现】P8 AssemblySolver 工作步骤总入口

## 目标

把 P8 Assembly solver 从“输入元数据 + `solve=not_migrated` 文档边界”收敛为可验证的 request-local solver 子集：先复核 live code、FreeCAD authority 和 focused tests，再决定哪些 releaseGate 进入发布回写，哪些 unsupported JointType 进入 C++ 实现，哪些缺 oracle 的路径保持 `notCollected`。

当前收口状态：本文只是索引文件，创建和链接校验已完成；S0 已完成声明口径与 live 基线复核，结论是正式 P8 文档、C ABI capabilities、focused tests、fixtures 与当前 C++ 之间存在 publication drift。S1 已完成 FreeCAD / cad-core source authority 候选补证。S2 已完成范围准入与 blocker 矩阵裁决：当前没有 evidence-backed backendGap，缺 oracle 的保持 notCollected，复杂 JointType 保持 unsupported，GUI / session / 完整 Link 写回保持 nonGoal。S3 已完成 Ondsel adapter 专项复审，确认当前 build 为 `CAD_CORE_HAS_ONDSEL_SOLVER=0` 时 focused real-solver test 会走 representative fallback，capability wording 与 test route 需 S6 按 build mode 收口。S4 已完成 PlacementWriteback 生命周期专项复审，确认 `documentObjectUpdates` 只表达 `Placement` 更新建议、下一请求由前端 graph 消费、invalid grounded rejection 会清空 updates；本轮 focused test 失败归因 S3 build-mode/test-route，native oracle 仍 `notCollected`。S5 已完成 JointType 裁决：Fixed / Revolute / Slider / Ball / Distance / Angle 是当前 releaseGate 子集，Cylindrical / Parallel / Perpendicular / RackPinion / Screw / Gears / Belt 仍为 diagnostic-only，复杂 Distance geometry 仍 `notCollected`。S6 仍待执行，尚未完成 oracle、实现或发布闸门；不得把整个主线写成“已实现”。

## 步骤索引

| 步骤 | 文件 | 当前状态 | 解决的问题 |
| --- | --- | --- | --- |
| S0 | [声明口径与 live 基线复核](6-17-21-19-【已实现】P8-AssemblySolver-S0-声明口径与live基线复核.md) | 已实现 | 冻结 solver 子集、禁止声明和 live drift；不关闭 SCOPE-002..008 |
| S1 | [FreeCAD 源码候选矩阵](6-17-21-20-【已实现】P8-AssemblySolver-S1-FreeCAD源码候选矩阵.md) | 已实现 | 建立 AssemblyObject / JointObject / cad-core adapter authority |
| S2 | [范围准入与 blocker 矩阵](6-17-21-21-【已实现】P8-AssemblySolver-S2-范围准入与blocker矩阵.md) | 已实现 | 分类 releaseGate、notCollected、unsupported、backendGap 和 nonGoal；未创建无证据 backendGap |
| S3 | [OndselSolver 适配专项复审](6-17-21-22-【已实现】P8-AssemblySolver-S3-OndselSolver适配专项复审.md) | 已实现 | 已复核 real solver、representative fallback、CMake link 和 diagnostics；unlinked build 缺口路由 S6 |
| S4 | [PlacementWriteback 生命周期专项复审](6-17-21-23-【已实现】P8-AssemblySolver-S4-PlacementWriteback生命周期专项复审.md) | 已实现 | 已复核 writeback updates、grounded validation、下一请求应用和多组件顺序；build-mode 缺口路由 S6 |
| S5 | [JointType 覆盖与 unsupported 矩阵专项复审](6-17-21-24-【已实现】P8-AssemblySolver-S5-JointType覆盖与unsupported矩阵专项复审.md) | 已实现 | 已裁决 supported / releaseGate 子集、diagnostic-only JointType 和复杂 Distance 边界 |
| S6 | [Oracle 实现与发布闸门](6-17-21-25-P8-AssemblySolver-S6-Oracle实现与发布闸门.md) | 待执行 | 消费 blocker，落 C++ / expected / docs / capability 发布 |

## 执行顺序

1. 先做 S0：只复核 live docs、capabilities、focused tests 和当前 git 状态，不改 C++。
2. 再做 S1：补 FreeCAD / cad-core 源码候选，候选不等于 supported。
3. 再做 S2：把候选分类为 `releaseGate`、`notCollected`、`unsupported`、`backendGap` 或 `nonGoal`。
4. S5 已完成：复审 JointType matrix，拆分 releaseGate、diagnostic-only、notCollected 和 S6 可实现顺序。
5. 最后做 S6：只消费 S2-S5 留下的可执行队列；若出现 backendGap 或 implementable unsupported，必须落 C++、fixture / expected 和 focused tests。

## 当前矩阵闸门

| 矩阵 | 当前用途 | 当前结论 |
| --- | --- | --- |
| `p8_assembly_solver_source_candidates.tsv` | 源码候选 | S1 已补齐 P8ASM-CAND-001..018；候选不等于状态裁决 |
| `p8_assembly_solver_scope_review_matrix.tsv` | scope 状态 | S4 已回写 P8ASM-SCOPE-002/003/005/006：writeback 是 stateless update 建议，invalid rejection 已有 gate，oracle 仍 notCollected |
| `p8_assembly_solver_blocker_queue.tsv` | 阻塞队列 | S5 已回写 P8ASM-BLOCK-005：unsupported JointType matrix 已裁决，capability / focused matrix 和实现项留给 S6 |
| `p8_assembly_solver_non_goal_registry.tsv` | 非目标 | S2 已明确 GUI、跨请求 session、完整 Link 写回、Worker/WASM、无 oracle 复杂 joint 的排除和 reopen 条件 |
| `p8_assembly_solver_backend_gap_classification.tsv` | 聚合 | S4 已补充 `releaseGate_placement_writeback_contract` 结论；不代表已有 C++ backendGap |

## 状态纪律

- `releaseGate` 表示当前代码和测试可能已覆盖，但 docs、capabilities、expected 或发布声明仍需复核。
- `notCollected` 表示缺 FreeCAD oracle 或 checked-in expected，不能直接写 C++。
- `backendGap` 必须同时有 FreeCAD authority 和当前 cad-core mismatch 证据。
- `unsupported` 若本轮能从 FreeCAD source 和 cad-core 结构定义 request-local 语义，S6 可以提升为实现任务；否则保持 unsupported 并写清 reopen 条件。
- `nonGoal` 必须公开用户 / 协议行为和 reopen 条件。

## 通用验收

```bash
git diff --check
awk -F '\t' 'FNR==1 {n=NF; next} NF!=n {print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END {exit bad}' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线/矩阵/*.tsv
rg -n "solve=not_migrated|representative_solver_adapter|ondsel_solver_adapter|assembly_set_placement" docs/CADCore方案/细化方案 cad-core/src cad-core/tests
```

代码修改后本轮 focused 验收优先使用：

```bash
cd cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

## 非目标

- 不实现 GUI、ViewProvider、TaskPanel、drag / postDrag 交互和 Simulation workbench。
- 不创建跨请求 solver session 或后端持久 Assembly 状态。
- 不把完整 FreeCAD Link 账本、copy-on-change、cross-document hash 生命周期混入本主线。
- 不从 cad-core 当前输出倒推 FreeCAD native oracle。
