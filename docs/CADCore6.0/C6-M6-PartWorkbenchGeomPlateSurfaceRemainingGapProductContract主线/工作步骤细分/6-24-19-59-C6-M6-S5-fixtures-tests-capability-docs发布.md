# C6-M6-S5 fixtures tests capability docs 发布

## 目标

把 S3/S4 的实现或收窄结果发布到 fixtures、focused tests、capability contract 和文档矩阵。S5 是发布步骤，不引入新语义。

## 产物

- `cad-core/src/runtime/capability_contract.cpp` 的 `part_workbench.geomplate` 状态、covered、fixtures、narrowed_gaps、remaining_gaps、non_goals 与 S3/S4 结果一致。
- `cad-core/tests/test_adapters.py` capability 断言同步。
- `cad-core/tests/test_p8_features.py` 和 `cad-core/tests/test_expected_fixtures.py` 覆盖新增或保留的 representative。
- C6-M6 矩阵、README、总入口和 `docs/CADCore6.0/README.md` 更新为 S5 状态。
- 只有已有代码、fixture、focused tests、capability 同步证明的项才能从 `remaining_gaps` 删除。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features -k geomplate
python3 -m unittest tests.test_expected_fixtures -k geomplate
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线/矩阵/*.tsv
git diff --check -- cad-core docs/CADCore6.0/C6-M6-PartWorkbenchGeomPlateSurfaceRemainingGapProductContract主线 docs/CADCore6.0/README.md
```

验收通过后，将本文重命名为 `6-24-19-59-【已实现】C6-M6-S5-fixtures-tests-capability-docs发布.md`。

## 非目标

- 不扩大到 full GeomPlate parity。
- 不删除未被 fixture / focused test 覆盖的 `remaining_gaps`。
- 不把 S3/S4 保留的 diagnostic / non-goal 改成 supported。
