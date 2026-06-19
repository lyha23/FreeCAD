# C4-S4 M2 PartDesign Revolution / Groove 审计与 oracle

## 目标

审计 PartDesign Revolution / Groove 是否属于前端 CAD 运行时 4.0 必补范围。若进入范围，先建立 oracle-first fixture 和 cad-core 落点，再进入实现。

## 必读文件

- `docs/CADCore4.0/C4-M2-PartDesignFeatureFamily总览/6-19-23-55-C4-M2PartDesignFeatureFamily补完方案.md`
- `docs/CADCore4.0/矩阵/cadcore4_scope_review_matrix.tsv`
- `docs/CADCore4.0/矩阵/cadcore4_source_candidates.tsv`
- `src/Mod/PartDesign/App/FeatureRevolution.cpp`
- `src/Mod/PartDesign/App/FeatureGroove.cpp`
- `src/Mod/PartDesign/App/FeatureSketchBased.cpp`
- `src/Mod/PartDesign/App/Body.cpp`
- `cad-core/src/part_design`
- `cad-core/tests/test_p7_features.py`

## 产物

- C4-M2 source / oracle / blocker 矩阵行。
- fixture 草案：axis、angle、reversed/symmetric、additive/subtractive、Body Tip replacement、failure diagnostics。
- 若实现，新增 `feature_revolution.*` / `feature_groove.*` 或明确复用现有 helper。

## 非目标

- 不迁移 GUI command / task panel。
- 不把低频或产品不用的参数直接标为必做。
- 不绕过 Body chain / topo history。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 完成口径

Revolution / Groove 有明确 supported / deferred / non-goal 状态；若 supported，至少有 native expected、Body Tip replacement 和 maker history 证据。

## 完成记录

- Supported：`PartDesign::Revolution` / `PartDesign::Groove` 的 `Type=Angle`、Sketch `H_Axis` / `V_Axis` 或线性 edge `ReferenceAxis`、`Angle`、`Reversed`、`Midplane`、Body Tip additive / subtractive replay、`maker_history:revolve`。
- Native oracle：`cad-core/fixtures/c4m2/partdesign-revolution-axis-angle-body.json`、`partdesign-groove-axis-angle-body.json` 及对应 `expected/*.freecad.json`。FreeCAD expected 的 volume / topology counts 与 cad-core 输出一致；`bbox_delta` 只覆盖 OCCT conservative bbox 扩张。
- Deferred diagnostics：`Type=TwoAngles`、`Type=ThroughAll`、`Type=UpToFirst`、`Type=UpToLast`、`Type=UpToFace`、`FuseOrder=FeatureFirst`、explicit `InternalFace` profile selection；`partdesign-revolution-groove-deferred.json` 和 `partdesign-revolution-invalid-angle.json` 覆盖失败路径。
- Non-goal：GUI command / task panel、全量 PartDesign feature 默认纳入、绕过 Body chain 或 executor-only synthetic topo alias。
