# 【已实现】C7-M2 S3 实现或 diagnostic 边界收口

## 目标

按 S2 裁决执行实现或 no-code 收口。若 S2 没有打开 `backend_gap_requires_implementation`，S3 只更新文档和矩阵，不改 C++、fixtures、expected 或 tests。

## 必读

- S2 完成后的本包 README、总入口、方案和 `矩阵/*.tsv`
- S2 明确列出的 FreeCAD 源码文件
- S2 明确列出的 cad-core 修改目标
- `AGENTS.md` 中 FreeCAD 迁移实现纪律、OpenCascade 使用规则和测试指南

## 动作

1. 记录 live baseline 和队列状态。
2. 读取 S2 route。若没有 `backend_gap_requires_implementation`，只做 diagnostic/no-code publication boundary。
3. 若 S2 授权实现，先写清 FreeCAD 调用链到 cad-core 分层映射，再修改代码。
4. 需要新增 public API、executor 主路径、mapper/history 字段时，在相邻注释写明 FreeCAD 源文件、类/函数和关键短句。
5. 需要引用恢复时，优先补正式 `topo` / history / naming 能力，不允许 adapter/output 层猜测。
6. 按实际修改范围补 focused tests 或 fixture route。
7. 把本文件文件名和一级标题标记为 `【已实现】`，队列推进到 S4。

## 非目标

- 不扩大到 S2 未授权的 scope。
- 不靠 fixture 名、边顺序、source edge 猜测实现。
- 不运行全量 FreeCAD build。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线
git diff --check
```

如果 S3 修改 C++ 或 fixtures，额外从当前 `cad-core/tests/test_p7_features.py` 中读取真实 Fillet / Chamfer / DressUp / SupportTransform test names，运行本轮相关 focused unittest。不要使用未复核的旧 test name。

## 通过条件

- S2 route 被落实，没有扩大 scope。
- 如有代码改动，FreeCAD 依据注释和 focused tests 同步完成。
- 本文件标记后，队列推进到 S4。

## 完成记录

- S3 live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=5adeee22a3`（`5adeee22a3 文档：完成 C7-M2 S2 准入裁决`），开始时 `git status --short -uall` 无输出。
- S2 没有产生 `backend_gap_requires_implementation`，Code edit gate 保持关闭；S3 走 no-code diagnostic/publication boundary，未改 C++、fixtures、expected 或 tests，也没有新增测试。
- `oracle_pending_collect` 保持不变：Fillet multi-edge / `UseAllEdges`、Chamfer `FlipDirection=true`、DressUp chain stale `ReferenceShadow` / Base recovery 只进入后续 oracle / publication 路线，不能声明 supported capability。
- `already_closed_expected_backed` 保持不变：Chamfer Two distances、Chamfer Distance and Angle、SupportTransform mirrored / chained DressUp regression 继续引用现有 fixture/expected/focused test 证据。
- `diagnostic_non_goal` 保持不变：GUI、full DressUp universe、full MapperHistory 和输出端引用恢复猜测不进入本包实现。
- publication drift 保持 `publication_closure_only` 并进入 S4；队列应跳过本文件，下一步为 S4。
