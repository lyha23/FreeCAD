# C5-M7 S1 现有 AdvancedConstraints 复核收口

## 目标

把现有 `c4m1/part-geomplate-advanced-constraints` 纳入 C5-M7 guard：它证明 advanced approximation params expected-backed，但不能替代 initial surface、G1 或 2D wrapper 支持。

## 工作

1. 复核 `cad-core/fixtures/c4m1/part-geomplate-advanced-constraints.json` 与 expected。
2. 复核 `test_c4m1_part_geomplate_advanced_constraints_are_expected_backed` 的断言范围。
3. 必要时补 adapter capability 断言，防止 `advanced_approximation_params_expected_backed` 被扩写成 full advanced support。
4. 更新 C5-M7 矩阵和方案中的 live guard 状态。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M7-PartWorkbenchSurface-GeomPlateSurfaceHelper第二批主线 cad-core
```

## 非目标

- 不新增 initial surface / 2D fixtures。
- 不修改 `Part.PlateSurface.Curves` policy。
