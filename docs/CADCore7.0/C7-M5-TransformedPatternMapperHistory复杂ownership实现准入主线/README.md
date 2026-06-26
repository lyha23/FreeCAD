# C7-M5 Transformed Pattern MapperHistory 复杂 ownership 实现准入主线

本目录承接 C7-M4 release gate 和 CAD Core 总览后续队列中的 P7 transformed / pattern 完整 MapperHistory 与复杂 ownership 方向。

C7-M5 不重开已经由旧 P7 Transformed 主线关闭的基础 topology_counts、final-result `FeatureRefine`、TransformN alias、terminal history 和 supported / covered 发布闸门。它只处理更复杂 ownership 是否存在 source-backed native oracle mismatch，以及 mismatch 是否足以打开 `cad-core` implementation gate。

## 入口

- 主线总入口：`6-26-08-37-C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线总入口.md`
- 方案：`6-26-08-37-C7-M5-TransformedPatternMapperHistory复杂ownership实现准入方案.md`
- 工作步骤总入口索引：`工作步骤细分/6-26-08-37-【已实现】C7-M5工作步骤总入口.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- 创建基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=1f2b86990d`（`1f2b86990d 文档：完成 C7-M4 S5 发布闸门`），创建前 `git status --short -uall` 无输出。
- C7-M1 / C7-M2 / C7-M3 / C7-M4 `工作步骤细分` 队列均为空。
- 旧 P7 Transformed 主线已经 S0-S6 完成，`P7T-SCOPE-001..007` 保持 supported / covered；`polar-pattern-whole-shape` standalone 仍是 nonGoal lifecycle boundary。
- C7-M5 工作步骤总入口索引已按 C7-M1/C7-M2/C7-M3 约定标记为 `【已实现】`；收口 live 起点 `HEAD=2b8c09b242`（`2b8c09b242 docs: 新增 C7-M5 transformed ownership 准入主线`），开始时 `git status --short -uall` 无输出。索引收口不执行 S0-S6，实际队列仍为 S0-S6 pending，下一 pending 必须是 S0。S0/S1 只允许文档和矩阵；S2 才形成 native oracle 候选；S3 才允许采集 oracle；S4 裁决 implementation gate；S5 只有在 S4 打开 code gate 后才改 C++。

## 收口边界

- 先证明 FreeCAD native source 和 oracle，再比较 `cad-core`；不得从当前 `cad-core` 输出倒推 expected。
- 只处理 transformed / pattern 复杂 ownership、MapperHistory、slot ownership、source alias 和 terminal / merge / split / deleted history。
- 不重开 C7-M4 stale `ReferenceShadow` recovery，不处理 GUI、full DressUp universe、full MapperHistory 全量迁移、P8 Assembly / Link / Web / WASM。
- 如果 S3/S4 不能取得 source-backed native oracle 或 mismatch，必须发布为 `oracle_blocked`、`diagnostic_non_goal` 或 `already_closed_expected_backed`，不打开 C++ 实现。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线 docs/CADCore7.0/README.md
git diff --check
```
