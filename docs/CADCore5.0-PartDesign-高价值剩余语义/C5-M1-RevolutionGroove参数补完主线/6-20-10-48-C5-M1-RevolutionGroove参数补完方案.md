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

## C5-S1 实施记录

### FreeCAD 调用链

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolution.cpp::Revolution::TypeEnums` 只枚举 `Angle / UpToLast / UpToFirst / UpToFace / TwoAngles`；`Revolution::execute()` 调 `executeRevolved(Part::RevolMode::FuseWithBase)`；`makeShape()` 按 `FuseOrder` 在 `base.makeElementFuse(revolve)` 与 `revolve.makeElementFuse(base)` 间切换。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureGroove.cpp::Groove::TypeEnums` 枚举 `Angle / ThroughAll / UpToFirst / UpToFace / TwoAngles`；`Groove::execute()` 调 `executeRevolved(Part::RevolMode::CutFromBase)`；Groove 没有 `UpToLast` source enum。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolved.cpp::tryExecuteRevolved()` 先验证 `Angle`、`Angle2` zero-sum、`Profile`、`ReferenceAxis`，再分支：`ToFirst/ToLast/ToFace` 走 `getUpToFace*()` 与 `tryToRevolveToFace()`；普通角度分支走 `generateRevolution()`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolved.cpp::generateRevolution()` 对 `TwoAngles` 使用 `angleTotal = Angle + Angle2` 且 `angleOffset = -Angle2`，对 `ThroughAll` 使用 `2 * pi`，然后调 `TopoShape::makeElementRevolve()`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp::getAxis()` 支持同 sketch 的 `V_Axis/H_Axis/N_Axis/AxisN`、`PartDesign::Line`、`App::Line` 与 `Part::Feature` 的线/圆边；本轮只把已可 native oracle 的 Part EdgeN 轴列为 expected-backed。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureSketchBased.cpp::getUpToFace()` 用 `Part::findAllFacesCutBy(..., gp_Ax1)` 查最近/最远旋转切面；`getUpToFaceFromLinkSub()` 必须解析显式 face LinkSub。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRevolution()` 使用 `BRepFeat_MakeRevol` 执行 UpTo face 路径；在 cad-core 补齐该 topo path 前不得用 bbox、输出顺序或 fixture 名称猜目标 face。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp::Body::execute()` 只把 `Tip` shape 写回 Body；cad-core 的 Body replay 继续按 Group 到 Tip 重放 additive/subtractive AddSubShape。

### cad-core 落点

- `cad-core/src/part_design/feature_revolved.cpp`：支持 `Type=TwoAngles` 与 Groove `Type=ThroughAll`，记录 `angle2`、`angle_total`、`angle_offset`，保留 `maker_history:revolve`。
- `cad-core/src/part_design/feature_revolved.cpp`：`UpToFirst/UpToLast/UpToFace` 保持稳定 diagnostic；`UpToFace` 解析显式 `UpToFace` LinkSub 并回传 `target/subname`，不做 bbox 或 topo alias 猜测。
- `cad-core/src/part_design/feature_revolved.cpp`：`Profile.SubList` 继续 `unsupported_profile_region`，`FuseOrder=FeatureFirst` 继续 `unsupported_property`，均不静默回退。
- `cad-core/fixtures/c5m1/`：新增 TwoAngles、Groove ThroughAll、Part edge axis native expected fixtures，以及 zero-sum、UpTo、Profile/FuseOrder diagnostic fixtures。
- `cad-core/src/adapters/c_api/c_api.cpp`：capability metadata 更新为 C5-M1 support/diagnostic split，不声明完整 Revolution/Groove 参数覆盖。

### 当前边界

- supported：Revolution/Groove `TwoAngles`；Groove `ThroughAll`；Sketch `H_Axis/V_Axis` 与 Part EdgeN `ReferenceAxis`；Body additive/subtractive replay。
- diagnostic-backed：Revolution `ThroughAll` invalid enum；UpToFirst/UpToLast/UpToFace BRepFeat path；Profile subshape；`FuseOrder=FeatureFirst`；zero-sum `Angle + Angle2`。
- deferred：Datum/App line axis native oracle、custom Sketch `AxisN` 和 full `TopoShape::makeElementRevolution()` / BRepFeat history path。
