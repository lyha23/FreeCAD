# C5-M2 Boolean / Body ownership 方案

## 目标

把 C4 中已支持的 `PartDesign::Boolean` Fuse / Cut / Common Body-tool first slice，扩展到前端 CAD runtime 常见的 Body ownership 压力场景：AllowCompound、multi-solid policy、multi-tool Group、BaseFeature reroute、missing/null tool diagnostics 和 element ownership。

## 范围

- FreeCAD 源码依据：`src/Mod/PartDesign/App/FeatureBoolean.cpp`、`src/Mod/PartDesign/App/Body.cpp`。
- topo 依据：`src/Mod/Part/App/TopoShapeExpansion.cpp`、`src/Mod/Part/App/TopoShape.cpp`。
- cad-core 落点：`cad-core/src/part_design/feature_boolean.*`、`cad-core/src/part_design/body.*`、`cad-core/src/part/topo_shape*`、`cad-core/tools/collect_freecad_expected.py`、`cad-core/src/adapters/c_api/c_api.cpp`。
- 验收：`tests.test_p7_features`、`tests.test_expected_fixtures`、`tests.test_adapters`。

## 最小完整语义批次

本包不只做一个 AllowCompound fixture。合理批次必须同时覆盖：

- AllowCompound false：多 solid result 保持结构化失败，诊断带 object / property / stage。
- AllowCompound true：多 solid result 的 shape、Body Tip replacement、subshape map 和 capability metadata。
- multi-tool Group：多个 Body tool 的顺序、缺失 tool、null shape 和 source ownership。
- BaseFeature / Group / Tip 更新：不破坏 C4 的 Fuse / Cut / Common first slice。
- LinkStage3-only Compound / Section：保持 non-goal 或 unsupported Type diagnostic，不混入本包 supported。

## 阶段

| 步骤 | 内容 |
| --- | --- |
| S0 | source audit：Boolean Type、Group、BaseFeature、AllowCompound、Body reroute |
| S1 | native oracle：AllowCompound true/false、multi-tool ownership、failure diagnostics |
| S2 | cad-core Body ownership / topo history / diagnostics / capability metadata |
| S3 | focused tests 与 remaining boundary 收口 |

## 非目标

- 不支持 FreeCAD 源码中已注释掉的 Compound / Section Type，除非产品 scope 单独重开。
- 不把 multi-solid result 修剪成单 solid 来通过 fixture。
- 不在 adapter 中判断 Body ownership。

## 验收

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义 cad-core
```

阶段回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```
