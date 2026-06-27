# C8-M5-S3 BodyBaseFeature 对象漂移专项复审

## 目标

裁决 `shape-binder-subshape-binder-element-map-namedshape-body-replay` 中 `BodyBaseFeature` 缺失是 expected stale，还是 `cad-core` Body replay / documentObjectUpdates 实现回退。

## Source authority

- FreeCAD：`/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp::ShapeBinder::updatedShape()`。
- FreeCAD：`/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/ShapeBinder.cpp::SubShapeBinder::update()`。
- cad-core：`/home/user/Chili3DProject/FreeCAD/cad-core/src/part_design/body.cpp::appendBodyBaseFeatureChainUpdates()`，已有注释引用 `Body::onChanged()` 创建 `PartDesign::FeatureBase` 和 `Feature::onChanged()` 同步 Body Group。

## 执行

1. 读取该 fixture input、expected 和 current output，比较 `documentObjectUpdates`、objects map、Body Group、downstream feature `BaseFeature`。
2. 确认 expected 中 `BodyBaseFeature` 的存在是否来自 FreeCAD `Body::onChanged(BaseFeature)`，还是 C8-M1 旧 collector / expected 的输出格式。
3. 若 FreeCAD authority 要求保留 `BodyBaseFeature`，修复必须落在 Body replay 主路径，不能在 expected compare、adapter 或 fixture 名分支里补对象。
4. 若 current output 才是正确 request-local 语义，只刷新该 expected fixture 中与 `BodyBaseFeature` 直接相关的字段，并更新 focused test 说明。
5. 更新 fixture oracle 和 backend gap classification 矩阵。

## 验收

Focused 命令：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_c8_shapebinder
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results
```

文档检查：

```bash
cd /home/user/Chili3DProject/FreeCAD
git diff --check
```

## 退出标准

- `C8M5-BLOCKER-301` 关闭。
- `C8M5-GAP-101` 分类为 `approved_expected_refresh` 或 `code_fix_required`。
- 任何代码修复都带有 FreeCAD source 注释；任何 expected 更新都带有明确 approved refresh 依据。

## S3 完成记录

- 执行基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=3bcdfaf958`，`git log -1 --oneline=3bcdfaf958 docs: 完成 C8-M5 S2 expected authority 复核`，开始工作区干净。
- 复核 input / expected / current output：该 fixture input 没有 `BodyBaseFeature` 对象、没有 `Body.BaseFeature` 属性，也没有等价控制对象；current output objects 为 `Body`、`Box`、`Box001`、`Fusion`、`ShapeBinder`、`SubShapeBinder`，`documentObjectUpdates=[]`。
- FreeCAD authority：`Body::onChanged(BaseFeature)` 只在 `BaseFeature.getValue()` 存在时创建 `PartDesign::FeatureBase`；collector 中 `BodyBaseFeature` 来自额外 `base_feature_control` Body，不属于 request-local input。
- 裁决：`C8M5-GAP-101=approved_expected_refresh`；未改 C++、adapter、comparator 或 S4 cycle 诊断。
- 已刷新该 expected fixture 中直接相关字段：移除 `objects.BodyBaseFeature`，移除 `Fusion.InList` 里的 `BodyBaseFeature` / `BaseFeature` 控制引用，移除 `element_map_evidence.base_feature_body_shape_element_map_size`。
- Focused test 已补充 request-local 断言：不产生 `BodyBaseFeature`，`documentObjectUpdates=[]`，Body `group` / `tip` 保持 `SubShapeBinder`。
- 验证：`python3 -m unittest tests.test_c8_shapebinder` 通过；expected fixture gate 仅剩 S4 范围的 `subshape-binder-setlinks-normalization-diagnostics` 诊断码差异，`BodyBaseFeature` drift 已消失。
- `C8M5-BLOCKER-301` 已关闭，下一队列首项为 S4。
