# C5-M1 Revolution / Groove 参数补完方案

## 目标

把 C4 中已支持的 `Type=Angle` first slice 扩展为产品可用的 Revolved 参数主线。该主线按 FreeCAD `FeatureRevolved.cpp::execute()` 的共享逻辑推进，同时区分 Revolution additive 和 Groove subtractive 的 Body replay。

## 范围

- FreeCAD 源码依据：`src/Mod/PartDesign/App/FeatureRevolution.cpp`、`FeatureGroove.cpp`、`FeatureRevolved.cpp`、`FeatureSketchBased.cpp`、`Body.cpp`。
- topo 依据：`src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRevolution()`。
- cad-core 落点：`cad-core/src/part_design/feature_revolved.*`、`feature_revolution.*`、`feature_groove.*`、`cad-core/src/part/topo_shape_expansion.cpp`、`cad-core/tools/collect_freecad_expected.py`、`cad-core/src/adapters/c_api/c_api.cpp`。
- 验收：`tests.test_p7_features`、`tests.test_expected_fixtures`、`tests.test_adapters`。

## 最小完整语义批次

本包不要只实现一个 `TwoAngles` fixture。合理批次必须同时处理：

- `TwoAngles`：`Angle` / `Angle2` / `Reversed` / nullifying angles diagnostic。
- `ThroughAll` 与 `UpToFirst` / `UpToLast` / `UpToFace`：`getThroughAllLength()`、`getUpToFace()`、LinkSub face 解析和 failure diagnostic。
- axis 与 profile 边界：Sketch H/V/N/Axis、datum line / App::Line / Part edge axis、Profile subshape 不得静默退回 full profile。
- `FuseOrder=FeatureFirst`：Revolution additive fuse order 必须显式支持或稳定 diagnostic，不得默认 BaseFirst。

## 阶段

| 步骤 | 内容 |
| --- | --- |
| S0 | source audit：Revolved method、axis、UpTo、FuseOrder、Profile selection |
| S1 | native oracle：TwoAngles、ThroughAll/UpTo、Profile subshape/FuseOrder fixtures |
| S2 | cad-core executor / topo history / diagnostics / capability metadata |
| S3 | focused tests 与 remaining boundary 收口 |

## 非目标

- 不迁移 Revolution / Groove GUI task panel。
- 不把低频 UI editing state 当成 CAD Core request state。
- 不用 bbox 或输出顺序猜 `UpToFace`。
- 不把 full PartDesign Workbench completion 写进 capability。

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
