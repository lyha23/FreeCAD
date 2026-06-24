# C6-M5-S5 fixtures / tests / capability / docs 发布

## 目标

把 S3/S4 已实现的 Filling product contract 发布到 fixture、focused tests、capability contract 和文档矩阵。S5 是发布整理步骤，不新增大块语义实现。

## 输入

- S3/S4 已通过的 C++ 改动。
- 新增或更新的 C6-M5 Filling fixtures。
- `cad-core/tests/test_p8_features.py` focused tests。
- `cad-core/src/runtime/capability_contract.cpp` capability 更新。
- 本目录矩阵与 README。

## 发布规则

- 只有已有 fixture / focused test 覆盖的 row 才能从 `remaining_gaps` 删除。
- 被 native helper blocker 证明但未实现的行必须留在 `narrowed_gaps` 或 blocker queue。
- capability `covered`、`request_local_boundaries`、`field_boundaries`、`fixtures`、`remaining_gaps` 必须一致。
- docs 只能记录最终证据，不写流水账。

## 必须回写的矩阵行

- 所有 `SCOPE-*`、`BLK-*`、`GAP-*` 的最终 S5 状态。
- `ORC-*` 的 fixture 路由与 expected-backed/product-backed 状态。
- `VAL-301`、`VAL-302`、`VAL-303`。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest
python3 -m unittest tests.test_adapters
cd /home/user/Chili3DProject/FreeCAD
rg -n 'part_workbench\\.filling|remaining_gaps|filling_surface_native_helper_blocker|filling_params_all_native_helper_blocker' cad-core/src/runtime/capability_contract.cpp
git diff --check -- cad-core docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线 docs/CADCore6.0/README.md
```

验收通过后，将本文重命名为 `6-24-16-25-【已实现】C6-M5-S5-fixtures-tests-capability-docs发布.md`。

## 非目标

- 不扩大测试到全量 FreeCAD CI。
- 不在 S5 临时补核心语义。
- 不把尚未覆盖的 row 写成发布完成。
