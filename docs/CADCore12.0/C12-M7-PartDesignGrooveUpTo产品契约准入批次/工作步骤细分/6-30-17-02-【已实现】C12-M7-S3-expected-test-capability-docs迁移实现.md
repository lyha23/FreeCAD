# 【已实现】C12-M7 S3 expected / test / capability / docs 迁移实现

## 目标

S2 已批准 Groove UpTo current exact diagnostic 作为 CAD Core product diagnostic contract。本步把该合同落到 expected、focused tests、capability 和 C12-M7 文档 / 矩阵；仍不实现几何 C++，也不声称 FreeCAD native parity。

## 本轮基线

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=c1955ab56e`。
- `git log -1 --oneline=c1955ab56e docs: 批准 C12-M7 S2 产品诊断契约`。
- 起点 `git -c core.quotepath=false status --short -uall` 无输出。
- S3 执行前队列首项仍是本文件，S4-S5 pending。

## 迁移结果

- 新增 `cad-core/fixtures/c51m1/expected/partdesign-groove-uptofirst-body.freecad.json`。
- 新增 `cad-core/fixtures/c51m1/expected/partdesign-groove-uptoface-body.freecad.json`。
- 两个 expected 均记录 `schema_version`、`freecad_version=1.2.0 revision 20260519`、`freecad_native_parity=false`、`diagnostic_codes=["execution_failed","execution_failed"]`、native failure evidence、primary / secondary diagnostic 和 locatable fields。
- UpToFirst primary diagnostic 定位到 `object=Groove`、`property=Type`、`stage=runtime`、`subname=UpToFirst`；UpToFace primary diagnostic 定位到 `object=Groove`、`property=UpToFace`、`target=Pad`、`stage=runtime`、`subname=Face4`。
- expected `objects` 断言 `Groove.status=error`、`Body.status=skipped`，并保留 `Body.reason=dependency Groove failed`。
- `cad-core/tests/test_p7_features.py::test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers()` 改为读取 expected-backed product diagnostic contract，同时保留 code、primary / secondary message 和 Groove / Body status 断言。
- `cad-core/src/runtime/capability_contract.cpp` 将 `part_design.revolution_groove` 公开状态迁移为 `supported_c12m7_groove_upto_product_diagnostic_contract`，narrowed gap route 改为 `product_diagnostic_contract_non_parity`。
- `cad-core/tests/test_adapters.py` 同步断言 product diagnostic contract、native failure note、fixture pair、delete/reopen condition 和 locatable fields。
- C12-M7 README / 总入口 / 方案 / 矩阵和 `docs/CADCore12.0/README.md` 已回写；`C12M7-BLOCKER-301`、S3 contract/source/scope/validation 行已关闭。

## 保留边界

- FreeCAD native failure 仍保留：`Groove: Revolution: Up to face: Could not revolve the sketch!`。
- 本步没有修改 `cad-core/src/part_design/feature_revolved.cpp`、`cad-core/src/part/topo_shape_expansion.cpp`、`cad-core/include`、recompute protocol 或 adapter runtime。
- 若未来同一 FreeCAD / LibPack / OCCT baseline 证明 Groove UpTo native 成功，且 current CAD Core mismatch 成立，才另开 geometry implementation package。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_publication_smoke
```

以上 focused build / tests 已通过；队列、TSV field count 和 `git diff --check` 由本轮收口验证覆盖。
