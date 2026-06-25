# C6-M8 S0 live 基线与 surface-family 状态冻结

## 目标

冻结 C6-M8 live 起点，记录 C6-M1 到 C6-M7 队列状态、当前 surface family capability、adapter assertion 和 root README 入口。S0 是文档/矩阵步骤，不改 C++、fixtures 或 expected。

## 必读

- `docs/CADCore6.0/README.md`
- `docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/README.md`
- `docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/6-25-10-53-C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure方案.md`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_adapters.py`

## 动作

1. 记录 `pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git -c core.quotepath=false status --short -uall`。
2. 分别运行 C6-M1 到 C6-M8 `工作步骤细分` 的 `step_goal_queue.py`。
3. grep 当前 `part_workbench.project_on_surface`、`ruled_surface`、`loft`、`sweep`、`filling`、`geomplate` 的 `status`、`remaining_gaps`、`narrowed_gaps` 和 `non_goals`。
4. 更新 C6-M8 README、总入口和矩阵中的 live baseline 行。
5. 确认 root `docs/CADCore6.0/README.md` 已链接 C6-M8。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/矩阵/*.tsv
git diff --check -- docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线 docs/CADCore6.0/README.md
```

## 通过条件

- live baseline 写入本步骤、README / 总入口和矩阵。
- ProjectOnSurface 的 active/non-goal overlap 被记录为 S2 必裁决项。
- S0 文件名和标题标记为 `【已实现】` 后，队列推进到 S1。

