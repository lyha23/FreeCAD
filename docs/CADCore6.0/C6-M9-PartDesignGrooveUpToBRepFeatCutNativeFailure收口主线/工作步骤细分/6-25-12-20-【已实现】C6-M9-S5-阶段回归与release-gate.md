# 【已实现】C6-M9 S5 阶段回归与 release gate

## 目标

运行 C6-M9 release gate，证明 Groove UpTo blocker 的 route 不是只改文档。S5 完成后，C6-M9 队列应为空，root README 和 capability 应能说明当前公开边界。

## S5 发布记录

- live cwd：`/Users/li/Chili3DProject/FreeCAD`。
- live HEAD：`e10ee2b966`（`e10ee2b966 文档：完成 C6-M9 S4 发布一致性收口`）。
- S5 执行起点 `git -c core.quotepath=false status --short -uall`：空输出，工作区干净。
- S5 执行起点队列：仅 `6-25-12-20-C6-M9-S5-阶段回归与release-gate.md` pending；S5 标记 `【已实现】` 后队列应为空。
- 公开 capability 边界保持 S4 口径：`part_design.revolution_groove.remaining_gaps=[]`、`exact_blockers={}`；`partdesign_groove_upto_brepfeat_cut_native_failure` 保留在 `narrowed_gaps` / `field_boundaries.historical_native_evidence`。
- 两个 `c51m1` Groove UpTo fixtures 继续作为 failure-oriented guard：`partdesign-groove-uptofirst-body` 与 `partdesign-groove-uptoface-body` 仍断言 `BRepFeat_MakeRevol could not revolve profile up to face` / `Could not revolve the sketch`，不声明 product success 或 FreeCAD parity。
- S5 未修改几何实现、fixtures、expected、collector、capability route 或 P7 failure fixture 语义，未新增 product fixtures。
- 重型收口未触发：S3/S4 未修改 maker history、ElementMap/history 主路径、collector expected 语义或批量 expected 文件。

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

## 验收记录

- `cmake --build build`：通过，`cad-core-lib`、`cad_core_ffi`、`cad-core` 和 probe targets 均已构建。
- `python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters`：`Ran 184 tests in 92.318s`，`OK (skipped=31)`。
- C6-M9 TSV 字段数检查：通过，全部 `矩阵/*.tsv` 行字段数与表头一致。
- `git diff --check -- cad-core docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线 docs/CADCore6.0/README.md`：通过。
- queue 检查：S5 标记 `【已实现】` 后，C6-M9 `工作步骤细分` 队列为空。
