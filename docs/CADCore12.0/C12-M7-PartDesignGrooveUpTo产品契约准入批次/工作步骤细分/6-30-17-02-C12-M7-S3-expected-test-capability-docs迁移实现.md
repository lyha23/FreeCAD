# C12-M7 S3 expected / test / capability / docs 迁移实现

## 目标

若 S2 批准 product diagnostic contract，则把该合同落到 expected、focused tests、capability 和文档公开口径。若 S2 未批准，只更新矩阵为 retained route，不改 `cad-core`。

## 必读来源

- S2 裁决文档
- `cad-core/fixtures/c51m1/partdesign-groove-uptofirst-body.json`
- `cad-core/fixtures/c51m1/partdesign-groove-uptoface-body.json`
- `cad-core/tests/test_p7_features.py`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_adapters.py`
- `docs/CADCore12.0/README.md`

## 操作

1. 若 product contract 被批准，新增或更新两个 Groove UpTo diagnostic expected。
2. 更新 focused test，让 diagnostic contract 走 expected-backed 路径，同时保留 exact message/code/object/property 断言。
3. 更新 capability source 和 adapter assertion：route 从 pure historical native failure 调整为 product diagnostic contract 或等价清晰 wording。
4. 更新 C12-M7 README / 总入口 / 矩阵和 CADCore12.0 README。
5. 若 S2 未批准，只写 retained conclusion，不改 `cad-core`。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_publication_smoke
```
