# 【已实现】C6-M2 S5 ApprovedExpectedOrCodeFix 实施

## 目标

执行 S2-S4 后唯一剩余的 approved implementation row：`C6M2-ORC-007` / `p2/pocket-without-base`。本轮不刷新 expected，不改已关闭的 ORC-001/003/005/006/012/013/015，也不扩大到非 C6-M2 范围。

## 本轮基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`4dcffa5c57`
- `git log -1 --oneline`：`4dcffa5c57 完成 C6-M2 S4 bbox 与 OCCT 差异收口`
- `git -c core.quotepath=false status --short -uall`：无输出，S5 开始时工作区干净。

## FreeCAD 依据

- FreeCAD：`/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp::Body::setBaseProperty()` 在首个 solid feature 没有前序 solid 时允许 `prevSolidFeature == nullptr`，原文注释为 `NULL is ok here, it just means we made the current one ... the base solid`。
- FreeCAD：`/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp::Body::execute()` 只读取 Tip feature 的 `Shape`，再执行 `Shape.setValue(tipShape)`。
- FreeCAD：`/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePocket.cpp::Pocket::execute()` 调用 `buildExtrusion()`，并设置 `MakeFace | MakeFuse | InverseDirection`。
- FreeCAD：`/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp::FeatureExtrude::buildExtrusion()` 先生成 prism 并写 `AddSubShape`；只有 `base.shapeType(true) <= TopAbs_SOLID && fuse` 时才做 cut/fuse。无 base 但 prism 是 solid 时走 `this->Shape.setValue(prism)`。

结论：FreeCAD 支持首个 subtractive Pocket 在无 base 时把生成的 subtractive solid 作为该 feature 的 Shape，Body 再把 Tip Shape 作为 Body Shape。因此 ORC-007 是 cad-core Body 重放语义缺口，不是 expected stale。

## 实现

- `cad-core/src/part_design/body.cpp`：`getBodyTopoShapeAtFeature()` 重放 `AddSubShape.subShape` 时，如果当前 Body 还没有 `bodyShape`，采纳该 subtractive tool solid 作为 Body 当前累计 shape；已有 base 的后续 subtractive 仍走 `cutShapes()`。
- `cad-core/tests/test_expected_fixtures.py`：新增 `test_c6m2_s5_pocket_without_base_matches_body_oracle`，断言 `p2/pocket-without-base` 无 diagnostics、Body ok、`replayed_subtractive_features=["Pocket"]`，并匹配 checked-in FreeCAD expected。
- `cad-core/tests/test_diagnostics.py`：`p2/pocket-without-base` 预期诊断改为 `[]`。

## 结果

- `C6M2-ORC-007` 已关闭：`p2/pocket-without-base` 当前输出 `diagnostics=[]`，Body bbox `[2,1,0]..[8,4,10]`、volume `180`、topology counts `faces=6/edges=12/vertices=8`，与 checked-in FreeCAD expected 一致。
- expected fixture 总闸门通过：此前 C6-M2 最后一个失败消失。
- 剩余状态：S5 无新增 blocker；`ORC-013` 仍只是 S4 已登记的 local OCCT imported LinkGroup bbox `bbox_delta=0.028` known environment gap，已被 expected fixture 合同显式容纳，不再阻塞 S5。S6 仍需执行阶段回归发布闸门。

## 非目标保持

- 未刷新 `p2/pocket-without-base.freecad.json`。
- 未修改已关闭 ORC-001/003/005/006/012/013/015 的 expected 或实现。
- 未改上游 FreeCAD，未采集 native FreeCAD expected。
- 未放宽 expected fixture 断言或新增 fixture-name 分支。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_c6m2_s5_pocket_without_base_matches_body_oracle
python3 -m unittest tests.test_diagnostics.CadCoreDiagnosticsTest.test_p2_fixture_diagnostics
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results
```

结果：全部通过；expected fixture 总闸门为 `OK (skipped=29)`。

最终收口命令：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore6.0 cad-core
for f in docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M2-ExpectedFixtureRegressionRecovery主线/工作步骤细分 --format markdown
```
