# 【已实现】C6-M8 S5 阶段回归与 release gate

## 目标

运行 C6-M8 release gate，证明 surface family published contract closure 不是只改文档。S5 完成后，C6-M8 队列应为空，root README 和 capability 应能说明当前公开边界。

## S5 live baseline

- `pwd=/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`25dc0ad331`
- `git log -1 --oneline`：`25dc0ad331 文档：完成 C6-M8 S4 发布一致性收口`
- `git -c core.quotepath=false status --short -uall`：空输出，S5 开始时工作区干净。
- `step_goal_queue.py .../工作步骤细分 --format markdown`：队列只剩本 S5 文件。

## 必跑

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/矩阵/*.tsv
git diff --check -- cad-core docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线 docs/CADCore6.0/README.md
```

## 重型收口触发

只有 S3/S4 修改了 `topo_shape_expansion`、ElementMap/history 主路径、collector expected 语义或批量 expected 文件时，才额外补跑 topology / broader expected suites，并把结果写入 validation matrix。

S5 结论：S3/S4 未修改 `cad-core/src/part/topo_shape_expansion.cpp`、ElementMap/history 主路径、`cad-core/tools/collect_freecad_expected.py`、collector expected 语义或批量 expected 文件；本步只执行 release gate 指定的 build、P8 / expected / adapter 阶段回归、queue、TSV 和 diff check，不触发 topology / broader expected suites。

## 验收结果

- `cmake --build build`：通过，`OndselSolver`、`cad-core-lib`、`cad_core_ffi`、`cad-core` 和 probe targets 均 built。
- `python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters`：`Ran 254 tests in 87.456s`，`OK (skipped=31)`。
- `step_goal_queue.py .../工作步骤细分 --format markdown`：S5 标记并重命名后返回空表。
- `awk -F '\t' ... 矩阵/*.tsv`：通过，所有 C6-M8 TSV 行字段数与表头一致。
- `git diff --check -- cad-core docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线 docs/CADCore6.0/README.md`：通过。

## 通过条件

- build 和阶段回归通过。
- C6-M8 队列返回空表。
- `part_workbench` surface family 的 `remaining_gaps`、`narrowed_gaps`、`non_goals`、fixtures 和 adapter assertions 一致。
- S5 文件名和标题标记为 `【已实现】`。
- 完成后按仓库规则提交本轮相关变更；不要混入 unrelated dirty work 或 build 产物。
