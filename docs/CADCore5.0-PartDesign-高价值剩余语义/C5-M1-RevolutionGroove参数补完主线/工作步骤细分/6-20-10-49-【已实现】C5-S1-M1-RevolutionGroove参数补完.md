# 【已实现】C5-S1 M1 Revolution / Groove 参数补完

## 目标

基于 FreeCAD `FeatureRevolved.cpp::execute()` 补齐 Revolution / Groove 的高级参数主线，至少覆盖 `TwoAngles`、`ThroughAll` / `UpTo*`、Profile subshape 和 `FuseOrder=FeatureFirst` 的支持或稳定 diagnostic。

## 必读

- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M1-RevolutionGroove参数补完主线/6-20-10-48-【已实现】C5-M1-RevolutionGroove参数补完方案.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M1-RevolutionGroove参数补完主线/矩阵/revolved_scope.tsv`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M1-RevolutionGroove参数补完主线/矩阵/revolved_blocker_queue.tsv`
- `src/Mod/PartDesign/App/FeatureRevolved.cpp`
- `src/Mod/PartDesign/App/FeatureRevolution.cpp`
- `src/Mod/PartDesign/App/FeatureGroove.cpp`
- `src/Mod/PartDesign/App/FeatureSketchBased.cpp`

## 工作内容

- 记录 FreeCAD 调用链：method selection、angle validation、axis resolution、UpTo face resolution、Body replay 和 topo maker history。
- 采集或设计 native expected fixtures；无法采集的分支必须落成 locatable diagnostic。
- 更新 cad-core executor、fixture、expected、focused tests 和 capability metadata。
- 关闭或细化 `C5M1-REV-BLK-*`，不得留下 broad Revolved gap。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义 cad-core
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 完成条件

- TwoAngles / ThroughAll / UpTo / Profile subshape / FuseOrder 行均 supported 或 diagnostic-backed。
- capability metadata 不声明完整 Revolution / Groove 参数全覆盖，remaining boundary 必须具体。

## 实施结果

- FreeCAD 调用链已记录到 `../6-20-10-48-【已实现】C5-M1-RevolutionGroove参数补完方案.md`：Revolution 与 Groove 的 `TypeEnums` 不同，`TwoAngles` / Groove `ThroughAll` 走 `BRepPrimAPI_MakeRevol`，UpTo 走 `BRepFeat_MakeRevol`。
- supported：Revolution/Groove `TwoAngles`、Groove `ThroughAll`、Part EdgeN `ReferenceAxis`、Body additive/subtractive replay、`maker_history:revolve`。
- diagnostic-backed：Revolution `ThroughAll` invalid enum、UpToFirst/UpToLast/UpToFace、zero-sum `Angle + Angle2`、Profile subshape、`FuseOrder=FeatureFirst`。
- deferred：Datum/App line axis native oracle、custom Sketch `AxisN`、完整 `TopoShape::makeElementRevolution()` / BRepFeat history path。
- 新增 `cad-core/fixtures/c5m1/` support 与 diagnostic fixtures，capability metadata 和全局/M1 矩阵已同步；`C5-BLK-101` 与 `C5M1-REV-BLK-*` 已关闭为 supported/diagnostic split。
