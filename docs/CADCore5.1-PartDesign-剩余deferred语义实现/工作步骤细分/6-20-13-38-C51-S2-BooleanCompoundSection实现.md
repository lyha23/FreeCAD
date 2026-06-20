# C51-S2 Boolean Compound / Section 实现

## 目标

将 C5 的 `C5-NG-005` 重开为实现范围：为 PartDesign Boolean 的 `Compound` / `Section` 明确产品语义、native expected、cad-core executor 和 topology history。

## 必读

- `src/Mod/PartDesign/App/FeatureBoolean.cpp`
- `src/Mod/PartDesign/App/Body.cpp`
- `src/Mod/PartDesign/App/Feature.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `docs/CADCore5.1-PartDesign-剩余deferred语义实现/矩阵/cadcore51_non_goal_registry.tsv`

## 工作内容

- 复核 FreeCAD 当前 `FeatureBoolean.cpp::TypeEnums` 与 disabled `Compound` / `Section` 分支，明确 cad-core 产品化语义是否直接采用 `TopoShape::makeElementBoolean(Compound/Section)`。
- 为 Compound / Section 采集 native expected 或写产品化 fixture；若 FreeCAD UI 不暴露，必须在方案里标明来源和差异。
- 实现 Group order、Body Tip replacement、AllowCompound、maker history 和 failure diagnostics。
- 删除或更新 C5 capability 中 `C5-NG-005` 的 non-goal 表述，改成 C5.1 supported / exact diagnostic。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.1-PartDesign-剩余deferred语义实现 cad-core
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 完成条件

- Boolean `Compound` / `Section` 有明确 runtime 行为和 focused tests。
- 不再把这两个 Type 归入 C5 non-goal。
