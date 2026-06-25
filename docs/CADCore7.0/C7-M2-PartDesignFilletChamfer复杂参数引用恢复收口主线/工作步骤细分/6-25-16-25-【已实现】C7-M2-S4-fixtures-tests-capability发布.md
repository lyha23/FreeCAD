# 【已实现】C7-M2 S4 fixtures tests capability 发布

## 目标

把 S2/S3 的裁决或实现结果同步到 fixtures、tests、capability/docs 和本包发布口径。S4 的重点是公开状态一致，不是新增实现。

## 必读

- S3 完成后的本包文档和矩阵
- `cad-core/fixtures/p7/`
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_adapters.py`
- `cad-core/src/runtime/capability_contract.cpp`
- `docs/CADCore方案/细化方案/10-P7-PartDesign常用生态.md`

## 动作

1. 记录 live baseline 和队列状态。
2. 如果 S3 有实现，确认 fixtures/expected/tests 与 capability docs 同步。
3. 如果 S3 是 no-code closure，确认 known gap route、non-goal rows 和 publication-only rows 写清。
4. expected 文件只能来自 FreeCAD oracle 或明确 diagnostic，不得从当前 `cad-core` 输出倒推。
5. 更新 root README、本包 README、总入口、方案和矩阵。
6. 把本文件文件名和一级标题标记为 `【已实现】`，队列推进到 S5。

## 非目标

- 不新增 S2/S3 未批准的 fixtures。
- 不把 oracle pending 写成 supported。
- 不用 capability 文案掩盖 backend gap。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/工作步骤细分 --format markdown
rg -n 'Fillet|Chamfer|SupportTransform|known_gap|remaining_gaps|native_oracle|backend_gap_requires_implementation|oracle_pending_collect' docs/CADCore7.0 docs/CADCore方案/细化方案/10-P7-PartDesign常用生态.md cad-core/src/runtime/capability_contract.cpp cad-core/tests cad-core/fixtures/p7
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线 docs/CADCore7.0/README.md
git diff --check
```

## 通过条件

- Capability/docs/tests/fixtures 口径一致。
- active gap、oracle pending 和 non-goal 没有混写。
- 本文件标记后，队列推进到 S5。

## 完成记录

- S4 live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=934ffa6ff8`（`934ffa6ff8 文档：完成 C7-M2 S3 no-code 边界收口`），开始时 `git status --short -uall` 无输出。
- S4 是 no-code publication closure：未改 C++、fixtures、expected、tests 或 `cad-core/src/runtime/capability_contract.cpp`，也没有新增 fixture/test。
- 发布口径已同步为三类：Chamfer Two distances、Chamfer Distance and Angle、SupportTransform mirrored / chained DressUp regression 是 inherited `already_closed_expected_backed`；Fillet multi-edge / `UseAllEdges`、Chamfer `FlipDirection=true`、DressUp chain stale `ReferenceShadow` / Base recovery 是 `oracle_pending_collect`；GUI、full DressUp universe、full MapperHistory 和 output-side guessing 是 `diagnostic_non_goal`。
- publication drift 已关闭为 `publication_closure_only`；队列应跳过本文件，下一步为 S5 release gate。
