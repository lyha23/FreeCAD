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
