# C6-M9 PartDesign Groove UpTo BRepFeat Cut Native Failure 收口主线

本目录承接 C6-M8 之后的 CAD Core 6.0 下一批工作。C6-M8 已把 Part Workbench surface family 的公开 capability 合同收口；当前 live capability 中仍保留的明确实现候选不是 surface family，而是 `part_design.revolution_groove` 下的 exact blocker：`partdesign_groove_upto_brepfeat_cut_native_failure`。

C6-M9 的目标是围绕 `PartDesign::Groove` 的 `Type=UpToFirst` / `Type=UpToFace` 做一轮可执行收口：先冻结 FreeCAD native failure、cad-core 当前失败、fixtures 和 adapter assertion；再裁决它是应继续作为 historical/native exact blocker，还是可以作为 CAD Core request-local product contract non-parity 实现；若进入实现，必须同时补 cad-core C++、fixtures/product metadata、focused tests、capability/docs 和 release gate。

## 入口

- 主线总入口：`6-25-12-14-C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线总入口.md`
- 方案：`6-25-12-14-C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- S0 live 起点已冻结：`pwd=/Users/li/Chili3DProject/FreeCAD`，当前 `HEAD=17116567e4`（`17116567e4 文档：完成 C6-M8 S5 发布闸门`）。
- S0 执行起点 `git status --short -uall` 只包含根 `docs/CADCore6.0/README.md` 修改和本 C6-M9 包新增文件；未发现 C6-M9 / 根 README 范围外 dirty 文件。
- C6-M1 到 C6-M8 的 `工作步骤细分` 队列均为空；C6-M9 S0 执行前队列从 S0 到 S5 全部 pending，S0 完成后应推进到 S1。
- `cad-core/src/runtime/capability_contract.cpp` 当前发布 `part_design.revolution_groove.status=supported_c51s1_advanced_with_exact_groove_upto_blocker`。
- `part_design.revolution_groove.remaining_gaps=["partdesign_groove_upto_brepfeat_cut_native_failure"]`，同一 blocker 也存在于 `exact_blockers`，并绑定 `c51m1/partdesign-groove-uptofirst-body` 与 `c51m1/partdesign-groove-uptoface-body`。
- `cad-core/tests/test_p7_features.py::test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers` 当前断言两个 Groove UpTo fixture 失败，diagnostic 为 `BRepFeat_MakeRevol could not revolve profile up to face` / `Could not revolve the sketch`。
- `Revolution Type=UpToFirst/UpToLast/UpToFace` 已由 C51 fixtures 支持，C6-M9 不重开 `PartDesign::Revolution` 已支持路径；只处理 `PartDesign::Groove` subtractive UpTo exact blocker。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线/矩阵/*.tsv
git diff --check -- docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线 docs/CADCore6.0/README.md
```
