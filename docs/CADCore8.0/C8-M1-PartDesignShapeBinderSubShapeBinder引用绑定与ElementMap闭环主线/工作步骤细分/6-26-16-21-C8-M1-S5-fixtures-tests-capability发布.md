# C8-M1-S5 fixtures / tests / capability 发布

## 目标

在 S4 实现后补齐 fixtures、focused tests、diagnostic tests、capability contract 和文档发布口径。S5 必须把 supported、request-local contract、oracle-blocked 和 non-goal 明确分开。

## 必须发布的能力口径

- `part_design.shape_binder.status`
- `part_design.shape_binder.covered`
- `part_design.shape_binder.remaining_gaps`
- `part_design.sub_shape_binder.status`
- `part_design.sub_shape_binder.covered`
- `part_design.sub_shape_binder.remaining_gaps`
- `topo_history.producer_matrix.shapebinder`
- `topo_history.maker_history` 中的 `shapebinder` / `subshapebinder` history tag

## fixtures / tests

| 类型 | 路径 | 要求 |
| --- | --- | --- |
| fixtures | `cad-core/fixtures/c8m1/*.json` | 输入 graph 覆盖批量场景 |
| expected | `cad-core/fixtures/c8m1/expected/*.freecad.json` | 来自 S3 FreeCAD native oracle |
| focused tests | `cad-core/tests/test_c8_shapebinder.py` | 成功、diagnostic、capability |
| diagnostics | `cad-core/tests/test_diagnostics.py` | missing / invalid / lifecycle boundary |
| expected comparator | `cad-core/tests/test_expected_fixtures.py` 或等价现有 harness | expected-backed parity |

## 发布规则

- 有 expected 且 current pass：发布 supported。
- 有 request-local product decision 但非 FreeCAD parity：发布 product contract，并写明 non-parity。
- FreeCAD lifecycle 不可观察：发布 `oracle_blocked`，写 delete condition。
- GUI / session：发布 `diagnostic_non_goal`。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_c8_shapebinder
python3 -m unittest tests.test_diagnostics
./cad-core capabilities > /tmp/c8m1-capabilities.json
```

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'shape_binder|sub_shape_binder|PartDesign::ShapeBinder|PartDesign::SubShapeBinder|remaining_gaps|oracle_blocked|diagnostic_non_goal' cad-core/src/runtime/capability_contract.cpp cad-core/tests docs/CADCore8.0
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M1-PartDesignShapeBinderSubShapeBinder引用绑定与ElementMap闭环主线/矩阵/*.tsv
git diff --check
```

验收通过后，将本文件重命名为 `6-26-16-21-【已实现】C8-M1-S5-fixtures-tests-capability发布.md`。

## 非目标

- 不扩大到 full PartDesign Workbench GUI。
- 不把 oracle-blocked lifecycle 写成 supported。
- 不用 docs-only 发布替代 focused tests。
