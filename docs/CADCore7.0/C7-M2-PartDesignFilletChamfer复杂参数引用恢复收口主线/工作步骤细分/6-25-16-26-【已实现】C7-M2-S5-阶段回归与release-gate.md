# 【已实现】C7-M2 S5 阶段回归与 release gate

## 目标

完成 C7-M2 release gate：验证队列为空、文档矩阵一致、必要的 build/tests 通过，并把最终发布状态写回 root README 和本包 README。

## 必读

- C7-M2 全部步骤文件
- C7-M2 `README.md`、总入口、方案和 `矩阵/*.tsv`
- S4 记录的 fixtures/tests/capability 变更范围
- 如有代码改动，读取对应 C++ 文件和 focused tests

## 动作

1. 记录 live baseline 和队列状态。
2. 运行 C7-M2 `step_goal_queue.py`，确认只有已实现文件或队列为空。
3. 运行 TSV、trailing whitespace 和 `git diff --check`。
4. 如果 C++ 变更存在，运行 `cad-core` build 和 S4 指定 focused tests。
5. 如果 fixtures/expected/topo/history/adapter schema 广泛变化，提升到 P7 stage regression；否则记录未触发重型回归的原因。
6. 更新 root README、本包 README、总入口、方案和矩阵中的 S5 release gate 结论。
7. 把本文件文件名和一级标题标记为 `【已实现】`。

## 非目标

- 不扩大到 C7-M3。
- 不补做未裁决 oracle。
- 不运行全仓库 FreeCAD build。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线 docs/CADCore7.0/README.md
git diff --check
```

代码变更存在时额外执行：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest
```

## 通过条件

- C7-M2 队列为空。
- S2 route、S4 publication 和 S5 release gate 记录一致。
- 必要验证通过。
- Root README 明确 C7-M2 最终状态和非目标边界。

## 完成记录

- S5 live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=5446576356`（`5446576356 文档：完成 C7-M2 S4 发布口径同步`），开始时 `git status --short -uall` 无输出。
- Release gate 已通过：C7-M2 队列清空，TSV 列数检查、trailing whitespace 检查、`git diff --check` 和 route / 发布口径检查通过。
- C7-M2 没有 `backend_gap_requires_implementation`；最终发布口径保持 inherited `already_closed_expected_backed`、`oracle_pending_collect`、`diagnostic_non_goal` 三类，oracle pending 不写成 supported。
- 本轮未改 C++、fixtures、expected、tests、topo/history 或 adapter schema；因此未触发 `cad-core` build、focused unittest 或 P7 stage regression。
