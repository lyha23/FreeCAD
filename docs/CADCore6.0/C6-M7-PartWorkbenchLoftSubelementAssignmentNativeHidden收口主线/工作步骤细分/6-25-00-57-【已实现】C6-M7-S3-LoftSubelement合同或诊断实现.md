# 【已实现】C6-M7-S3 LoftSubelement 合同或诊断实现

## 目标

按 S2 路由新增 C6-M7 request-local Loft subelement product contract，保持 FreeCAD native-hidden diagnostic expected 分离。

## 执行基线

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=1b0c3ce588`。
- `git log -1 --oneline=1b0c3ce588 完成 C6-M7 S2 路由判定`。
- S3 开始时工作区干净。
- S3 开始时 C6-M7 队列从 `6-25-00-57-C6-M7-S3-LoftSubelement合同或诊断实现.md` 继续。

## 实现结果

- `cad-core/src/app/property_links.cpp`：`App::PropertyLinkList.values[]` 接收 request-local rich link item，用于依赖排序和对象目标归一；注释明确不改变 FreeCAD 原生 `PropertyLinkList` 持久语义。
- `cad-core/src/part/part_loft.cpp`：`Sections` link item 带 `SubList/StableSubList` 时解析 selected subshape，发布 `contract_provenance=cad_core_product_contract_non_parity`、`section_entries` 和 `selected_sections`；解析失败给 `invalid_subshape`。
- `cad-core/fixtures/c6m7/part-loft-subelement-product*.json` 与对应 expected：新增 valid product contract 和 invalid subshape diagnostics，均声明不是 FreeCAD native parity。
- `cad-core/tests/test_p8_features.py` 与 `cad-core/tests/test_expected_fixtures.py`：覆盖 valid product metadata、invalid diagnostic、C5-M12 native-hidden expected 不变。

## 产物

- 已新增 C6-M7 product fixture 与 expected/product metadata。
- 已保留 `cad-core/fixtures/c5m12/expected/part-loft-subelement-assignment-diagnostic.freecad.json` 不变。
- 已更新 C6-M7 README、主线总入口、方案、步骤总入口和矩阵 S3 actual evidence。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features -k loft
python3 -m unittest tests.test_expected_fixtures -k loft
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线/矩阵/*.tsv
git diff --check -- cad-core docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线
```

S3 实测：`cmake --build build`、`tests.test_p8_features -k loft`（12 tests）、`tests.test_expected_fixtures -k loft`（2 tests）均通过。

## 非目标

- 不做 PartDesign Loft 新语义。
- 不重开 complex profile family。
- 不改 Sweep / Filling / GeomPlate。
- 不改 capability publication 或 adapter capability 断言；该项留给 S4。
- 不删除 `remaining_gaps`。
