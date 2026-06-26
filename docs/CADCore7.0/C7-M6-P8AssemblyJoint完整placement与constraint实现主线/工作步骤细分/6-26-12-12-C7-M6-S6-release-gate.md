# C7-M6 S6 release gate

## 目标

执行 C7-M6 release gate，确认队列、矩阵、发布口径和验证结果一致。S6 只允许收口文档、矩阵、状态和必要的 release validation；不新增 scope。

## 必读文件

- C7-M6 本包 README、总入口、方案、工作步骤、矩阵。
- `docs/CADCore7.0/README.md`
- `docs/CADCore方案/细化方案/11-P8-Part导入导出与Assembly后续.md`
- S3/S4/S5 产生或更新的 fixture、expected、tests 和源码文件。

## 执行要点

1. 记录 live baseline 和 C7-M6 queue。
2. 复跑本包 queue、TSV 列数、trailing whitespace 和 `git diff --check`。
3. 若 S5 改 C++，复跑 `cmake --build build` 和 focused unittest。
4. 若 S5 只改文档/矩阵，记录为什么不跑 C++ build。
5. 更新 root README、本包 README/总入口/方案、矩阵和 P8 细化口径。
6. 将完成的步骤文件标记为 `【已实现】`，确认队列为空。
7. 按仓库规则提交本轮相关变更并证明工作区干净，除非用户明确要求不提交。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M6-P8AssemblyJoint完整placement与constraint实现主线 docs/CADCore7.0/README.md docs/CADCore方案/细化方案/11-P8-Part导入导出与Assembly后续.md
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

- C7-M6 队列为空。
- release 结论明确：expected-backed、implemented、oracle_blocked 或 diagnostic_non_goal。
- root README、本包 README/方案/总入口和矩阵口径一致。
- 最终工作区状态符合仓库提交规则。
