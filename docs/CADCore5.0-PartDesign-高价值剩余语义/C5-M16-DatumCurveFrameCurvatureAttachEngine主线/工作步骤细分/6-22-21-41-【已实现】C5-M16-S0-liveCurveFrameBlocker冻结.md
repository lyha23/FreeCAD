# 【已实现】C5-M16-S0 live CurveFrame Blocker 冻结

## 目标

冻结 C5-M16 的声明口径、live blocker、C5-M15 依赖和禁止声明，确保后续实现按 curve-frame / curvature 语义批次推进。

## 必读

- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/tests/test_adapters.py`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M15-Datum3DPlaneAttachEngine主线`
- `docs/CADCore5.1-PartDesign-剩余deferred语义实现/矩阵/cadcore51_followup_blocker_queue.tsv`

## 工作内容

1. 运行 C5-M15 队列，确认 S6 是否已关闭。
2. 读取 capability exact blocker 中 `datum_attach_engine_remaining_modes` 的当前 modes。
3. 只冻结 curve-frame / curvature family：`FrenetNB/TN/TB`、`Concentric`、`SectionOfRevolution`、`AxisOfCurvature`、`Normal`、`Binormal`、`CenterOfCurvature`。
4. 把 `Folding`、conic landmarks、`IntersectionPoint`、`TangentU/V` 写入 non-goal / later package。
5. 更新 package-local 矩阵中的 S0 状态和 root C5 blocker 状态，不修改 code。

## S0 live 结论

- C5-M15 live 队列当前仍只有 `C5-M15-S6 Oracle 实现与发布闸门` pending；因此 M16 S6 不得发布 capability，也不得并行修改同一个 `datum_attach_engine_remaining_modes` exact blocker。
- `cad-core/src/adapters/c_api/c_api.cpp` 当前 `datum_attach_engine_remaining_modes` 仍包含：`FrenetNB`、`FrenetTN`、`FrenetTB`、`Concentric`、`SectionOfRevolution`、`Folding`、`AxisOfCurvature`、`Directrix1`、`Directrix2`、`Asymptote1`、`Asymptote2`、`Normal`、`Binormal`、`TangentU`、`TangentV`、`Focus1`、`Focus2`、`CenterOfCurvature`、`IntersectionPoint`。
- C5-M16 只冻结 curve-frame / curvature scope：`FrenetNB/TN/TB`、`Concentric`、`SectionOfRevolution`、`AxisOfCurvature`、`Normal`、`Binormal`、`CenterOfCurvature`。
- `Folding`、conic landmarks（`Focus1/2`、`Directrix1/2`、`Asymptote1/2`）、`IntersectionPoint`、`TangentU/V`、GUI/session 继续作为 non-goal / later-package guard，不允许被 M16 顺带声明 supported。
- S0 不采集 oracle、不改 C++/Python 测试、不移除 exact blocker；下一步进入 S1 FreeCAD CurveFrame 源码候选矩阵。

## 完成条件

- live blocker 与矩阵一致。
- C5-M15 dependency 写清：M15 S6 未关闭时不得执行 M16 S6 release gate。
- `C5M16-SCOPE-900` 和 `C5M16-NG-*` 能阻止 excluded family 被顺带实现。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M16-DatumCurveFrameCurvatureAttachEngine主线/工作步骤细分 --format markdown
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M16-DatumCurveFrameCurvatureAttachEngine主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
```
