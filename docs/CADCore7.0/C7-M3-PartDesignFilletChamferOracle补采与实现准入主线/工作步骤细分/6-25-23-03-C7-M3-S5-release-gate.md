# C7-M3 S5 release gate

## 目标

完成 C7-M3 release gate：验证队列为空、oracle/parity/implementation route 一致、必要 build/tests 通过，并把最终状态写回 root README 和本包 README。

## 必读

- C7-M3 全部步骤文件
- C7-M3 `README.md`、总入口、方案和 `矩阵/*.tsv`
- S4 记录的代码、fixture、expected、test、capability 变更范围

## 动作

1. 记录 live baseline 和队列状态。
2. 运行 C7-M3 `step_goal_queue.py`，确认只有已实现文件或队列为空。
3. 运行 TSV、trailing whitespace 和 `git diff --check`。
4. 如果 C++、fixtures、expected 或 tests 变更存在，运行 S4 指定 focused tests；若 C++ 变更存在，运行 `cad-core` build。
5. 如果 topo/history/adapter schema 广泛变化，提升到 P7 stage regression；否则记录未触发重型回归的原因。
6. 更新 root README、本包 README、总入口、方案和矩阵中的 S5 release gate 结论。
7. 把本文件文件名和一级标题标记为 `【已实现】`。

## 非目标

- 不扩大到 C7-M4。
- 不补做未裁决 oracle。
- 不运行全仓库 FreeCAD build。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线 docs/CADCore7.0/README.md
git diff --check
```

代码变更存在时额外执行：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest
```

## 通过条件

- C7-M3 队列为空。
- Oracle、parity gate、S4 publication / implementation 和 S5 release gate 记录一致。
- 必要验证通过。
- Root README 明确 C7-M3 最终状态和非目标边界。
