# C4-M2 Revolution / Groove 审计主线方案

## 目标

确认 PartDesign Revolution / Groove 是否属于 CADCore4.0 产品目标内的必补能力。若进入实现范围，先补 native oracle、Body chain 语义、maker history 和 adapter capability，再进入 executor。

## 范围

- FreeCAD 源码依据：`src/Mod/PartDesign/App/FeatureRevolution.cpp`、`FeatureGroove.cpp`、`FeatureSketchBased.cpp`、`Body.cpp`。
- cad-core 落点：`cad-core/src/part_design`、`cad-core/src/part/topo_shape*`、`cad-core/tools/collect_freecad_expected.py`、`cad-core/src/adapters/c_api/c_api.cpp`。
- 验收：`tests.test_p7_features`、`tests.test_expected_fixtures`、`tests.test_adapters`。

## 阶段

| 步骤 | 内容 |
| --- | --- |
| S0 | source audit、产品范围和参数矩阵 |
| S1 | native expected 与 diagnostic policy |
| S2 | executor / history / capability 或 deferred 收口 |

## 非目标

- 不迁移 GUI command / task panel。
- 不把低频或产品不用的参数直接标成必做。
- 不绕过 Body Tip replacement 和 topo history。
