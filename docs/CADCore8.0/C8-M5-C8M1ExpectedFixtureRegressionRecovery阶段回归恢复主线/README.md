# C8-M5 C8M1 Expected Fixture Regression Recovery 阶段回归恢复主线

## 定位

C8-M5 不新增几何能力。它承接 C8-M4 S6 阶段回归中暴露的两个 C8-M1 expected fixture drift，把阶段回归重新恢复到可作为 C8 后续主线发布闸门的状态。

本轮只处理两个 live 失败点：

- `shape-binder-subshape-binder-element-map-namedshape-body-replay`：expected 中包含 `BodyBaseFeature`，当前 `cad-core` 输出缺失该对象。
- `subshape-binder-setlinks-normalization-diagnostics`：expected 诊断为 `cycle_rejected_by_property_link`；S4 已把 current `cad-core` 从 generic `cycle_dependency` 修复为 setter-level `cycle_rejected_by_property_link`。

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

## S2 expected authority 复核

S2 在 `528f294e98` 上只做 current recompute、expected compare 和 native collector 复核，不改 C++、expected fixture 或测试断言。`C8M5-BLOCKER-201` 已关闭；完整 expected fixture gate 仍失败，但失败范围只剩两个目标 drift：1 error、1 failure、35 skipped。

| drift | current compare | fresh native collector | 初步裁决 | 后续 owner |
| --- | --- | --- | --- | --- |
| `shape-binder-subshape-binder-element-map-namedshape-body-replay` | current diagnostics 为空，object map 为 `Body`、`Box`、`Box001`、`Fusion`、`ShapeBinder`、`SubShapeBinder`，缺 expected 的 `BodyBaseFeature`；`documentObjectUpdates=[]` | `collect_c8m1_shapebinder_expected.py` 用 `FreeCADCmd 1.2.0 revision 20260519` 重跑到 `/tmp/c8m5-s2/native`，仍产出 `BodyBaseFeature`；但 input fixture 本身没有 `BodyBaseFeature` 对象，也没有 `Body.BaseFeature` 属性，collector 中该对象来自额外 `base_feature_control` 控制 Body | `approved_refresh_candidate`：expected/collector 覆盖范围与 request-local input 不一致，S3 决定刷新该 expected 字段或拆出独立 BaseFeature fixture | S3 |
| `subshape-binder-setlinks-normalization-diagnostics` | current diagnostics 为 `cycle_dependency`，stage=`graph`，object=`SubShapeBinderCycle` | fresh native collector 仍产出 `cycle_rejected_by_property_link`，`SubShapeBinder::setLinks()` 对 self link 抛出 `ValueError: self linking` | `code_fix_required_candidate`：setter-level property-link cycle 有 FreeCAD authority，S4 应在 runtime/reference lifecycle 或 graph 诊断语义层处理，不在 adapter 改字符串 | S4 |

## S3 BodyBaseFeature 裁决

S3 在 `3bcdfaf958` 上复核 `shape-binder-subshape-binder-element-map-namedshape-body-replay` 的 input、expected、current output、FreeCAD `Body::onChanged(BaseFeature)` 和 `cad-core/src/part_design/body.cpp::appendBodyBaseFeatureChainUpdates()` 后，最终裁决 `C8M5-GAP-101=approved_expected_refresh`。

结论：该 request-local input 只有 `Body.Group=["SubShapeBinder"]` 与 `Body.Tip=SubShapeBinder`，没有 `BodyBaseFeature` 对象、没有 `Body.BaseFeature` 属性，也没有等价控制对象；current output 的 object map 为 `Body`、`Box`、`Box001`、`Fusion`、`ShapeBinder`、`SubShapeBinder`，`documentObjectUpdates=[]`，这是正确请求语义。FreeCAD authority 中 `Body::onChanged()` 只在 `prop == &BaseFeature` 且 `BaseFeature.getValue()` 存在时创建 `PartDesign::FeatureBase`；C8-M1 collector 的 `BodyBaseFeature` 来自 `collect_binder_element_map()` 额外创建的 `base_feature_control` Body，不属于该 input graph。

已最小刷新 `cad-core/fixtures/c8m1/expected/shape-binder-subshape-binder-element-map-namedshape-body-replay.freecad.json` 中直接相关字段：移除 `objects.BodyBaseFeature`，移除 `Fusion.InList` 中的 `BodyBaseFeature` / `BaseFeature` 控制引用，移除 `element_map_evidence.base_feature_body_shape_element_map_size`。Focused test 现在显式断言该 fixture 不产生 `BodyBaseFeature` 且 `documentObjectUpdates=[]`。

S3 验证结果：`python3 -m unittest tests.test_c8_shapebinder` 通过；expected fixture gate 不再出现 `BodyBaseFeature` drift，当前仅剩 S4 范围的 `subshape-binder-setlinks-normalization-diagnostics` 诊断码差异。

## S4 cycle 诊断裁决

S4 在 `b375ffa973` 上复核 `subshape-binder-setlinks-normalization-diagnostics` 的 input、expected、FreeCAD `SubShapeBinder::setLinks()` 和 current `cad-core` graph 输出后，最终裁决 `C8M5-GAP-201=code_fix_landed`。

结论：该 request-local input 中 `SubShapeBinderCycle.Support` 是 `App::PropertyXLinkSubList`，且唯一 target 为 owner 自身。FreeCAD authority 中 `SubShapeBinder::setLinks()` 先构造 `inSet` 并插入 `this`，命中后抛出 `Cyclic reference to ...`；fresh native collector 同步产出 `cycle_rejected_by_property_link` / `ValueError: self linking`。因此 expected 不刷新，代码修复落在 `cad-core/src/runtime/reference_lifecycle.cpp`，只重放 `PartDesign::SubShapeBinder` 的 `Support` self-link setter-cycle 拒绝；generic graph cycle 仍由 `cad-core/src/graph/recompute_plan.cpp` 输出 `cycle_dependency`。

已同步 diagnostics vocabulary、focused C8 test、diagnostics test 和 adapter capability smoke。full FreeCAD property editor lifecycle、reverse in-list cache、copy-on-change temporary document cache 仍不是 C8-M5 supported scope。

S4 验证结果：`cmake --build build` 通过；`python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics tests.test_adapters` 通过；`python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results` 通过，35 skipped 保持不变。

## S5 准入收口

S5 在 `4e8d00798e` 上复核 S3/S4 落地状态，最终裁决为 docs/status 收口，不再改 C++、expected fixture 或测试。

- `C8M5-GAP-101=approved_expected_refresh`：S3 只刷新 `shape-binder-subshape-binder-element-map-namedshape-body-replay` 中 `BodyBaseFeature` 直接相关字段，未做 blanket C8-M1 expected refresh。
- `C8M5-GAP-201=code_fix_landed`：S4 代码落在 `cad-core/src/runtime/reference_lifecycle.cpp`，不是 adapter、comparator 或 fixture-name 字符串重写；generic graph cycle 仍保持 `cycle_dependency`。
- `copy_on_change_full_temporary_document_cache` 仍按 C8-M2 保持 `known_gap` / `oracle_blocked`，不是 C8-M5 supported scope。

S5 验证结果：`python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics` 通过，18 tests；`python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results` 通过，35 skipped。`C8M5-BLOCKER-501` 已关闭，下一队列首项为 S6 阶段回归发布闸门。

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
