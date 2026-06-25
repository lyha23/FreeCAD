# 【已实现】C6-M9 S0 live 基线与 exact blocker 冻结

## 目标

冻结 C6-M9 live 起点，记录 C6-M1 到 C6-M8 队列状态、`part_design.revolution_groove` capability exact blocker、adapter assertion 和 focused P7 fixture 当前语义。S0 是文档/矩阵步骤，不改 C++、fixtures 或 expected。

## 必读

- `docs/CADCore6.0/README.md`
- `docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线/README.md`
- `docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线/6-25-12-14-C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口方案.md`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_adapters.py`
- `cad-core/tests/test_p7_features.py`

## 动作

1. 记录 `pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git -c core.quotepath=false status --short -uall`。
2. 分别运行 C6-M1 到 C6-M9 `工作步骤细分` 的 `step_goal_queue.py`。
3. grep 当前 `part_design.revolution_groove` 的 `status`、`exact_blockers`、`remaining_gaps`、fixtures 和 adapter assertions。
4. 更新 C6-M9 README、总入口和矩阵中的 live baseline 行。
5. 确认 root `docs/CADCore6.0/README.md` 已链接 C6-M9。

## S0 冻结记录

- live cwd：`/Users/li/Chili3DProject/FreeCAD`。
- live HEAD：`17116567e4`（`17116567e4 文档：完成 C6-M8 S5 发布闸门`）。
- S0 执行起点 `git -c core.quotepath=false status --short -uall`：`docs/CADCore6.0/README.md` 为已修改；C6-M9 包 README、总入口、方案、S0-S5 step 文件和 8 个矩阵 TSV 为未跟踪新增；未发现 C6-M9 / 根 README 范围外 dirty 文件。
- C6-M1 到 C6-M8 `工作步骤细分` 队列均为空；C6-M9 S0 执行前队列从 S0 到 S5 全部 pending。
- `part_design.revolution_groove.status=supported_c51s1_advanced_with_exact_groove_upto_blocker`。
- `exact_blockers.id=partdesign_groove_upto_brepfeat_cut_native_failure`，`freecad_message=Revolution: Up to face: Could not revolve the sketch!`，fixtures 为 `c51m1/partdesign-groove-uptofirst-body` 和 `c51m1/partdesign-groove-uptoface-body`。
- `remaining_gaps=["partdesign_groove_upto_brepfeat_cut_native_failure"]`，`cad-core/tests/test_adapters.py` 当前按同一字符串断言。
- `cad-core/tests/test_p7_features.py::test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers` 当前断言两个 Groove UpTo fixture 的 diagnostics 为 `BRepFeat_MakeRevol could not revolve profile up to face` / `Could not revolve the sketch`，`Groove` 为 `error`，`Body` 为 `skipped`。
- S0 只冻结现状，不修改 C++、fixtures、expected 或测试语义；`partdesign_groove_upto_brepfeat_cut_native_failure` 已明确进入 S2 route 裁决。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线/工作步骤细分 --format markdown
rg -n 'partdesign_groove_upto_brepfeat_cut_native_failure|partdesign-groove-uptofirst-body|partdesign-groove-uptoface-body|BRepFeat_MakeRevol could not revolve' cad-core/src/runtime/capability_contract.cpp cad-core/tests docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线/矩阵/*.tsv
git diff --check -- docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线 docs/CADCore6.0/README.md
```

## 通过条件

- live baseline 写入本步骤、README / 总入口和矩阵。
- Groove UpTo exact blocker 被记录为 S2 必裁决项。
- S0 文件名和标题标记为 `【已实现】` 后，队列推进到 S1。
