# 【已实现】C7-M7 S6 release gate

## 目标

执行 C7-M7 release gate，确认队列、矩阵、发布口径和验证结果一致。S6 只允许收口文档、矩阵、状态和必要的 release validation；不新增 scope。

S5 已按 no-code publication closure 完成：未做 C++ implementation，未修改 `cad-core/src`、adapter、tests、fixtures、expected、collector、capability 或生成输出。S6 默认只跑文档 / 矩阵 / 队列 release validation；只有 S6 自己引入代码、fixture、expected、test 或 capability 改动时，才需要补跑 build / focused unittest。

## 完成结论

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=fb133d0fe6`（`fb133d0fe6 文档：完成 C7-M7 S5 no-code 发布收口`），开始状态干净。
- release route 已关闭：already-covered rows closed；`C7M7-ORACLE-202=oracle_blocked`、`C7M7-ORACLE-302=oracle_blocked`、`C7M7-ORACLE-402=oracle_blocked`；`C7M7-ORACLE-203=oracle_blocker`；GUI / frontend / cache / Worker 继续 `diagnostic_non_goal`。
- C7-M7 没有 `backend_gap_requires_implementation`，没有 C++ implementation；S6 也未修改 `cad-core/src`、adapter、tests、fixtures、expected、collector、capability 或生成输出。
- `C7M7-BLOCKER-601` 已关闭，`C7M7-GATE-701` 已关闭，C7-M7 队列为空。
- 因本轮只改文档和矩阵，未运行 `cmake --build build`、focused P8 unittest、adapter unittest、FreeCADCmd 或 collector。

## 必读文件

- C7-M7 本包 README、总入口、方案、工作步骤、矩阵。
- `docs/CADCore7.0/README.md`
- S3/S4 已产生或裁决的 fixture、expected、tests 和源码证据；S5 只更新发布文档和矩阵。

## 执行要点

1. 已记录 live baseline 和 C7-M7 queue。
2. 已复跑本包 queue、TSV 列数、trailing whitespace 和 `git diff --check`。
3. S5/S6 均未改 C++、fixture、expected、test 或 capability，因此不跑 `cmake --build build` 和 focused unittest。
4. 已确认 root README、本包 README/总入口/方案和矩阵发布口径一致。
5. 本文件标题和文件名已标记为 `【已实现】`，队列为空。
6. 按仓库规则提交本轮相关变更并证明工作区干净。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线 docs/CADCore7.0/README.md
git diff --check
```

如果 S5 改了 C++：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

## 完成标准

- C7-M7 队列为空。
- release 结论明确：expected-backed、implemented、oracle_blocked 或 diagnostic_non_goal。
- root README、本包 README/方案/总入口和矩阵口径一致。
- 最终工作区状态符合仓库提交规则。
