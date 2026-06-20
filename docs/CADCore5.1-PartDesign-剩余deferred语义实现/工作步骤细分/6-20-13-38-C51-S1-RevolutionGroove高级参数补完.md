# C51-S1 Revolution / Groove 高级参数补完

## 目标

实现 C5 留下的 Revolution / Groove deferred：UpToFirst / UpToLast / UpToFace、Profile subshape、`FuseOrder=FeatureFirst`、DatumLine / App line axis oracle 和支持。

## 必读

- `src/Mod/PartDesign/App/FeatureRevolved.cpp`
- `src/Mod/PartDesign/App/FeatureRevolution.cpp`
- `src/Mod/PartDesign/App/FeatureGroove.cpp`
- `src/Mod/PartDesign/App/FeatureSketchBased.cpp`
- `docs/CADCore5.1-PartDesign-剩余deferred语义实现/矩阵/cadcore51_oracle_fixture_matrix.tsv`

## 工作内容

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
