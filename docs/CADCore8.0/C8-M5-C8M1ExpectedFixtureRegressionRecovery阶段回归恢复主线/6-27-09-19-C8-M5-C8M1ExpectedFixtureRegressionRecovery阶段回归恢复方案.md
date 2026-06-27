# C8-M5 C8M1 Expected Fixture Regression Recovery 阶段回归恢复方案

## 背景

C8-M4 已完成 GeomPlate CurveConstraint criteria request-local 批量收口，focused build / tests / capability smoke 通过。阶段回归命令：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters tests.test_diagnostics
```

稳定失败于两个 C8-M1 expected fixture drift：

- `shape-binder-subshape-binder-element-map-namedshape-body-replay`：expected 的 `documentObjectUpdates` / object map 仍包含 `BodyBaseFeature`，当前输出缺失。
- `subshape-binder-setlinks-normalization-diagnostics`：expected 诊断为 `cycle_rejected_by_property_link`，当前输出为 `cycle_dependency`。

C8-M5 的职责是把这两个 drift 恢复为有 source authority、fixture authority、focused test 和阶段回归共同支撑的稳定状态。

## FreeCAD 调用链

`BodyBaseFeature` 漂移的 source authority：

- `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp::ShapeBinder::updatedShape()` 调用 `ShapeBinder::getFilteredReferences()` 和 `ShapeBinder::buildShapeFromReferences()`，决定 ShapeBinder 目标 shape。
- `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::update()` 负责 SubShapeBinder 引用更新、copy-on-change 状态和 shape 输出。
- `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp::Body::onChanged()` 的 `BaseFeature` 语义在 `cad-core/src/part_design/body.cpp::appendBodyBaseFeatureChainUpdates()` 已有对应实现：创建 `PartDesign::FeatureBase` 并同步 Body Group / downstream feature `BaseFeature`。

cycle 诊断漂移的 source authority：

- `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::setLinks()` 在 `inSet.find(v.first) != inSet.end()` 时抛出 `Cyclic reference to ...`，这是 property-link setter 阶段拒绝 cycle 的 FreeCAD 依据。
- `/home/user/Chili3DProject/FreeCAD/src/App/PropertyLinks.h::PropertyLinkBase` 是 dependency-bearing property base；`cad-core/src/graph/recompute_plan.cpp::visitObject()` 当前在 graph 拓扑阶段统一发出 `cycle_dependency`。
- 本轮需要裁决：该 fixture 是否应表达 setter-level `cycle_rejected_by_property_link`，还是 current graph-level `cycle_dependency` 才是无状态 request-local 输入的正确诊断。

## cad-core 分层落点

- `document`：仅负责 JSON property/link 解析与 normalized dependencyLinks，不写 PartDesign 特化。
- `graph`：继续负责依赖图 cycle；如果 S4 裁决该 fixture 应为 graph-level cycle，则 expected 更新必须说明 property setter 语义无法在该 request-local 输入中稳定重放。
- `runtime`：如果需要保留 setter-level cycle，应在 reference lifecycle / diagnostics 层输出明确诊断，不能在 adapter 输出端改字符串。
- `part_design`：`BodyBaseFeature` 若判定为实现回退，优先修 `cad-core/src/part_design/body.cpp` 的 Body BaseFeature chain update，不在 fixture compare 或 adapter 层补对象。
- `tests/fixtures`：expected 只在 S5 审批后按精确 fixture / 字段更新，不做全集刷新。

## 实施顺序

1. S0 冻结 live 基线：记录 `HEAD`、工作区、C8-M1 到 M4 队列、current capability、两行失败的原始 diff。
2. S1 生成 owner 分类：把两个 drift 拆到 fixture expected、current runtime、focused test assertion 和 source authority 四类，更新 blocker / oracle 矩阵。
3. S2 复核 expected authority：重新运行两个相关 fixture 的 current output compare，必要时重新采集 FreeCAD native expected，但不能直接覆盖文件。
4. S3 专项复审 `BodyBaseFeature`：确认 expected 中 `BodyBaseFeature` 是否仍是 FreeCAD 语义要求；若是，实现修复必须落在 Body replay / documentObjectUpdates 主路径。
5. S4 专项复审 cycle 诊断：确认 property setter cycle 与 graph cycle 在 request-local JSON 中的可重放边界；统一 expected、focused tests 和 diagnostics vocabulary。
6. S5 准入裁决：每个 drift 独立给出 `approved_expected_refresh` 或 `code_fix_required`，并落最小变更。
7. S6 发布闸门：focused expected fixture、C8 shapebinder / diagnostics tests、stage regression 全部通过后，更新 README 和矩阵状态。

## 退出标准

- 两个 drift 都有明确裁决，不再作为 C8-M4 的遗留失败挂起。
- `python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results` 通过，或只剩文档中明确豁免的 unrelated known issue。
- `python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics` 通过。
- `python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters tests.test_diagnostics` 通过。
- `copy_on_change_full_temporary_document_cache` 未被误标为 supported。
