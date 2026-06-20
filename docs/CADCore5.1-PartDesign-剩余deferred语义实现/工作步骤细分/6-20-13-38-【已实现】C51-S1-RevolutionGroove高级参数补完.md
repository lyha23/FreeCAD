# 【已实现】C51-S1 Revolution / Groove 高级参数补完

## 目标

实现 C5 留下的 Revolution / Groove deferred：UpToFirst / UpToLast / UpToFace、Profile subshape、`FuseOrder=FeatureFirst`、DatumLine / App line axis oracle 和支持。

## 必读

- `src/Mod/PartDesign/App/FeatureRevolved.cpp`
- `src/Mod/PartDesign/App/FeatureRevolution.cpp`
- `src/Mod/PartDesign/App/FeatureGroove.cpp`
- `src/Mod/PartDesign/App/FeatureSketchBased.cpp`
- `docs/CADCore5.1-PartDesign-剩余deferred语义实现/矩阵/cadcore51_oracle_fixture_matrix.tsv`

## 工作内容

- 先关闭矩阵 child blockers：`C51-BLK-111` UpTo BRepFeat、`C51-BLK-112` Profile subshape、`C51-BLK-113` FuseOrder、`C51-BLK-114` Datum/App/Sketch axis；对应 oracle 为 `C51-ORC-111`..`114`，validation 为 `C51-VAL-111`..`114`。
- 记录 FreeCAD 调用链：`tryExecuteRevolved()`、`generateRevolution()`、`getUpToFace()`、`getAxis()`、`FuseOrderEnums`。
- 扩展 native expected：UpToFirst / UpToLast / UpToFace、selected profile subshape、FeatureFirst、Datum/App line axis。
- 在 cad-core 中补 `feature_revolved` 和 topo maker history，不在 adapter 中写建模语义。
- 同步 fixtures、expected、focused tests、capability metadata 和矩阵。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.1-PartDesign-剩余deferred语义实现 cad-core
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 完成条件

- UpTo/Profile/FuseOrder/Axis 不再是 broad deferred。
- 无 fixture-name、bbox 或输出顺序猜测逻辑。

## 实现记录

- FreeCAD 调用链：`FeatureRevolved.cpp::tryExecuteRevolved()` 读取 Profile/Type/ReferenceAxis，`FeatureSketchBased.cpp::getUpToFace()/getUpToFaceFromLinkSub()` 选择 UpTo 面，`TopoShapeExpansion.cpp::TopoShape::makeElementRevolution()` 通过 `BRepFeat_MakeRevol` 执行 UpTo，`FeatureRevolution.cpp::makeShape()` 按 `FuseOrderEnums` 选择 BaseFirst/FeatureFirst，`FeatureSketchBased.cpp::getAxis()` 接受 Sketch AxisN、`PartDesign::Line` 和 `App::Line`。
- cad-core 落点：`feature_revolved.cpp` 补 UpTo/Profile/InternalFace/ReferenceAxis/FuseOrder 入口，`topo_shape_expansion.*` 补 BRepFeat revolution-until bridge，`body.cpp` 消费 AddSubShape FeatureFirst，`datum_line.cpp` 区分 `PartDesign::Line` z 轴与 `App::Line` x 轴，`c_api.cpp` 只同步 capability。
- checked-in expected：`cad-core/fixtures/c51m1/expected/partdesign-revolution-{uptoface-body,uptofirst-body,uptolast-body,internalface-profile,featurefirst-body,datumline-axis,appline-axis,sketch-axisn}.freecad.json`。
- exact blocker：`partdesign_groove_upto_brepfeat_cut_native_failure`。`partdesign-groove-uptofirst-body` 和 `partdesign-groove-uptoface-body` 在 native FreeCAD 1.2.0 revision 20260519 同样报 `Revolution: Up to face: Could not revolve the sketch!`，cad-core focused test 固定为 exact diagnostic，不恢复 broad UpTo deferred。
- 验证通过：`cd cad-core && cmake --build build && python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters`。
