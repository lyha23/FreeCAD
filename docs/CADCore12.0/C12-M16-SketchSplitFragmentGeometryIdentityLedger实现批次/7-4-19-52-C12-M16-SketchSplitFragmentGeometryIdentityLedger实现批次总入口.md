# C12-M16 Sketch split fragment geometry identity ledger 实现批次总入口

## 目标

承接 C12-M15 `C12M15-CONTRACT-009 split_fragment_boundary` 的 implementation gap，实现 source one-to-many split fragment 的 request-local identity ledger，使 `g<ID>:splitN` 能被 response 与 reference resolution 统一发布和消费。

## 必读文件

- `README.md`
- `7-4-19-52-C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次方案.md`
- `矩阵/c12m16_split_fragment_identity_source_matrix.tsv`
- `矩阵/c12m16_split_fragment_identity_scope_matrix.tsv`
- `矩阵/c12m16_split_fragment_identity_contract_matrix.tsv`
- `矩阵/c12m16_split_fragment_identity_implementation_matrix.tsv`
- `矩阵/c12m16_split_fragment_identity_blocker_queue.tsv`
- `矩阵/c12m16_split_fragment_identity_non_goal_registry.tsv`
- `矩阵/c12m16_split_fragment_identity_validation_matrix.tsv`
- `docs/CADCore12.0/README.md`

## 队列顺序

1. `7-4-19-53-【已实现】C12-M16工作步骤总入口.md`
2. `7-4-19-54-【已实现】C12-M16-S0-live基线与C12-M15继承冻结.md`
3. `7-4-19-55-【已实现】C12-M16-S1-FreeCAD-split-history与current-reselect复核.md`
4. `7-4-19-56-【已实现】C12-M16-S2-red-fixture与focused-test设计.md`
5. `7-4-19-57-【已实现】C12-M16-S3-fragment-ledger-C++实现.md`
6. `7-4-19-58-【已实现】C12-M16-S4-response-reference-adapter接入验证.md`
7. `7-4-19-59-【已实现】C12-M16-S5-实现发布闸门.md`

## 当前状态

- C12-M16 是实现批次：用户明确要求写代码实现 C12-M15 没有授权的 split fragment 缺口。
- S0 live 冻结为 `HEAD=a4375f45a5`（`a4375f45a5 文档：关闭 C12-M16 工作步骤总入口`），起点 worktree clean；C12-M15 队列只输出 markdown 表头。
- S3 已实现 source one-to-many request-local fragment ledger，`g<ID>:splitN` 来自 split history / internal alias / current fragment edge 映射，不靠 mesh、bbox 或 response order 猜。
- S4 已验证 response、reference resolution、adapter public result 和 `sketcher.split_fragment_identity_ledger` capability wording 共享同一 fragment ledger，并明确不发布 persistent FreeCAD session parity。
- S5 发布 final status：`implemented_current_supported`；blocker queue 无悬空 open row，C12-M16 队列关闭后只输出 markdown 表头。

## 执行规则

- 每次只处理队列中的第一个未实现步骤，完成后刷新队列。
- `g<ID>` 普通 raw edge identity 维持 C12-M15 现状，不重做。
- split fragment identity 必须来自 split history / internal alias / fragment ledger，不能来自 mesh、bbox、response order 或 source order 猜测。
- 没有合法 `geometryId` 的 split fragment 不发布 durable token。
- 如果 fragment ownership 不唯一，必须输出 diagnostic / needs-reselect。

## 关闭条件

- 队列可由 `step_goal_queue.py` 读出入口 + S0-S5；S5 关闭后只输出 markdown 表头。
- TSV 字段数检查通过。
- README、方案、总入口、矩阵与 root README 均指向 C12-M16 implementation scope。
- blocker queue 中每个未关闭 blocker 都有明确 next action。

## 非目标

- 不处理 my-chili3d frontend consumer sync。
- 不处理 C12-M11 open wire mesh 产品契约。
- 不引入 persistent backend session/cache。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次 docs/CADCore12.0/README.md
git diff --check
```
