# C6-M9 S5 阶段回归与 release gate

## 目标

运行 C6-M9 release gate，证明 Groove UpTo blocker 的 route 不是只改文档。S5 完成后，C6-M9 队列应为空，root README 和 capability 应能说明当前公开边界。

## 必跑

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线/矩阵/*.tsv
git diff --check -- cad-core docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线 docs/CADCore6.0/README.md
```

## 重型收口触发

只有 S3/S4 修改了 maker history、ElementMap/history 主路径、collector expected 语义或批量 expected 文件时，才额外补跑 topology / broader expected suites，并把结果写入 validation matrix。

## 通过条件

- build 和阶段回归通过。
- C6-M9 队列返回空表。
- `part_design.revolution_groove` 的 `remaining_gaps`、`exact_blockers` / `narrowed_gaps`、fixtures 和 adapter assertions 一致。
- S5 文件名和标题标记为 `【已实现】`。
- 完成后按仓库规则提交本轮相关变更；不要混入 unrelated dirty work 或 build 产物。
