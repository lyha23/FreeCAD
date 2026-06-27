# C8-M5 C8M1 Expected Fixture Regression Recovery 阶段回归恢复主线

## 定位

C8-M5 不新增几何能力。它承接 C8-M4 S6 阶段回归中暴露的两个 C8-M1 expected fixture drift，把阶段回归重新恢复到可作为 C8 后续主线发布闸门的状态。

本轮只处理两个 live 失败点：

- `shape-binder-subshape-binder-element-map-namedshape-body-replay`：expected 中包含 `BodyBaseFeature`，当前 `cad-core` 输出缺失该对象。
- `subshape-binder-setlinks-normalization-diagnostics`：expected 诊断为 `cycle_rejected_by_property_link`，当前 `cad-core` 输出为 `cycle_dependency`。

`part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]` 继续按 C8-M2 结论保留为 `known_gap` / `oracle_blocked`，不是 C8-M5 的实现入口。

## 当前基线

- 仓库：`/home/user/Chili3DProject/FreeCAD`
- 基线提交：`e93ddd8746`（`feat: 完成 C8-M4 GeomPlate 曲线 criteria 发布闸门`）
- S0 live 冻结命令已在该提交复核：`pwd=/home/user/Chili3DProject/FreeCAD`，`git rev-parse --short=10 HEAD=e93ddd8746`，`git log -1 --oneline=e93ddd8746 feat: 完成 C8-M4 GeomPlate 曲线 criteria 发布闸门`。
- S0 开始工作区只包含 `docs/CADCore8.0/README.md` 修改和本 C8-M5 包未跟踪文件；未见 `src/`、`cad-core/src`、fixture 或 expected 文件 dirty。
- C8-M1 到 C8-M4 工作步骤队列均为空：四个 `step_goal_queue.py --format markdown` 均只返回表头。
- current capability 中唯一非空 active `remaining_gaps` 是 `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`；该项继续作为 C8-M5 non-goal。
- C8-M4 focused build / tests / capability smoke 通过。
- C8-M4 stage regression 命令 `python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters tests.test_diagnostics` 已在 S0 重跑，266 tests、35 skipped，稳定失败于上述两个 C8-M1 expected drift 行。

## S0 live drift evidence

| fixture | expected file | current code / evidence |
| --- | --- | --- |
| `shape-binder-subshape-binder-element-map-namedshape-body-replay` | `cad-core/fixtures/c8m1/expected/shape-binder-subshape-binder-element-map-namedshape-body-replay.freecad.json` | current output object map lacks `BodyBaseFeature`; expected contains it; stage regression error is `KeyError: 'BodyBaseFeature'`. |
| `subshape-binder-setlinks-normalization-diagnostics` | `cad-core/fixtures/c8m1/expected/subshape-binder-setlinks-normalization-diagnostics.freecad.json` | current diagnostic code is `cycle_dependency` at `graph` stage for `SubShapeBinderCycle`; expected code is `cycle_rejected_by_property_link`. |

## S1 owner 分类

S1 在 `f3cca29254` 上只做失败清单和 owner 路由，不改 C++、fixture expected 或测试断言。`C8M5-BLOCKER-101` 已关闭；两个 drift 都已绑定 expected、current implementation、focused test 和 FreeCAD source authority 四类证据。

| drift | expected evidence | current implementation evidence | focused test evidence | FreeCAD source authority | route | next owner |
| --- | --- | --- | --- | --- | --- | --- |
| `BodyBaseFeature` object drift | expected fixture object map contains `BodyBaseFeature` and `Fusion.InList` references it | `cad-core/src/part_design/body.cpp::appendBodyBaseFeatureChainUpdates()` can synthesize `PartDesign::FeatureBase`, but S0 current output still lacks the object | `test_c8_shapebinder.py::test_binder_element_map_namedshape_and_body_replay_stay_request_local()` covers the fixture without directly asserting `BodyBaseFeature`; expected fixture gate fails on the object drift | `src/Mod/PartDesign/App/Body.cpp::Body::onChanged()` creates `FeatureBase` when `BaseFeature` is set; ShapeBinder authority remains `SubShapeBinder::update()` for the binder shape | `expected_authority_pending` plus `implementation_regression_candidate` | S2 then S3 |
| `cycle_rejected_by_property_link` / `cycle_dependency` diagnostic drift | expected fixture `diagnostic_codes` contains `cycle_rejected_by_property_link` | `cad-core/src/graph/recompute_plan.cpp::visitObject()` emits `cycle_dependency`; `runtime/reference_lifecycle.cpp` has no setter-level cycle diagnostic | `test_c8_shapebinder.py` and `test_diagnostics.py` currently assert `cycle_dependency` for cycle fixtures | `src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::setLinks()` throws `Cyclic reference to ...` before Support is accepted | `diagnostic_policy_pending` with `expected_authority_pending` input | S2 then S4 |

## 主文件

- 总入口：`6-27-09-19-C8-M5-C8M1ExpectedFixtureRegressionRecovery阶段回归恢复主线总入口.md`
- 方案：`6-27-09-19-C8-M5-C8M1ExpectedFixtureRegressionRecovery阶段回归恢复方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 源码与测试落点

- FreeCAD source authority：`/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp::ShapeBinder::updatedShape()`、`SubShapeBinder::update()`、`SubShapeBinder::setLinks()`。
- FreeCAD link / cycle 语义入口：`/home/user/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp`、`/home/user/Chili3DProject/FreeCAD/src/App/Document.h` 的 dependency graph / `DepNoCycle` 语义。
- `cad-core` Body replay 落点：`/home/user/Chili3DProject/FreeCAD/cad-core/src/part_design/body.cpp::appendBodyBaseFeatureChainUpdates()`。
- `cad-core` cycle 诊断落点：`/home/user/Chili3DProject/FreeCAD/cad-core/src/graph/recompute_plan.cpp::visitObject()` 与 `cad-core/src/runtime/reference_lifecycle.cpp`。
- fixture gate：`cad-core/tests/test_expected_fixtures.py`、`cad-core/tests/fixture_expected.py`、`cad-core/fixtures/c8m1/expected/`。

## 验收分层

本轮短跑默认验收：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M5-C8M1ExpectedFixtureRegressionRecovery阶段回归恢复主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M5-C8M1ExpectedFixtureRegressionRecovery阶段回归恢复主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M5-C8M1ExpectedFixtureRegressionRecovery阶段回归恢复主线 docs/CADCore8.0/README.md
git diff --check
```

实现阶段 focused 验收：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results
python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics
```

发布闸门验收：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters tests.test_diagnostics
```
