# C51X-S1 Groove UpTo native 证据复核

## 目标

复核 `partdesign_groove_upto_brepfeat_cut_native_failure` 是否仍是 FreeCAD native exact blocker。默认不实现；只有 native oracle 证明同类输入可以成功时，才转入 cad-core parity 实现。

## 必读

- `src/Mod/PartDesign/App/FeatureRevolved.cpp`
- `src/Mod/PartDesign/App/FeatureGroove.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `cad-core/src/part_design/feature_revolved.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/fixtures/c51m1/partdesign-groove-uptofirst-body.json`
- `cad-core/fixtures/c51m1/partdesign-groove-uptoface-body.json`

## 工作内容

- 用当前 oracle collector 重新采集或复核 Groove UpToFirst / UpToFace native 行为。
- 若 FreeCAD 仍报 `Revolution: Up to face: Could not revolve the sketch!`，保持 exact blocker，只更新证据日期、capability 文案和 tests 中的 diagnostic expectation。
- 若 FreeCAD native 成功，才实现完整 `BRepFeat_MakeRevol` subtractive path：目标面选择、support face、cut mode、Body subtractive replay、NamedShape / ElementMap history 和 focused tests。
- 禁止用 bbox、输出顺序或 fixture 名称猜 target face。

## 完成记录

- 当前 FreeCADCmd：1.2.0 revision 20260519。
- `partdesign-groove-uptofirst-body` 与 `partdesign-groove-uptoface-body` 仍打印 `Groove: Revolution: Up to face: Could not revolve the sketch!`，collector 返回 target Body no shape。
- cad-core fixture 返回 `execution_failed` exact diagnostic；未进入 BRepFeat cut parity 实现。
- blocker 状态：`still_exact_blocker_native_refreshed`。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.1-PartDesign-剩余deferred语义实现 cad-core
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 完成条件

- Blocker 状态是 `still_exact_blocker` 或 `freecad_parity_implemented`，不能停留在 broad deferred。
