# C6-M7-S4 fixtures tests capability docs 发布

## 目标

把 S3 的实现或收窄结果发布到 fixtures、focused tests、capability contract 和 C6-M7 文档矩阵。S4 只做发布同步，不引入新语义。

## 产物

- `cad-core/src/runtime/capability_contract.cpp` 的 `part_workbench.loft` 状态、covered、fixtures、remaining_gaps、narrowed_gaps / non_goals 与 S3 一致。
- `cad-core/tests/test_adapters.py` capability 断言同步。
- `cad-core/tests/test_p8_features.py` / `tests/test_expected_fixtures.py` 覆盖新增或保留代表项。
- C6-M7 README、总入口、矩阵和 `docs/CADCore6.0/README.md` 同步。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features -k loft
python3 -m unittest tests.test_expected_fixtures -k loft
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线/矩阵/*.tsv
git diff --check -- cad-core docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线 docs/CADCore6.0/README.md
```

## 非目标

- 不跑 heavy gate。
- 不做 Surface Family freeze。
