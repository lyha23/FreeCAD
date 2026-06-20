# P8-LinkAssemblyRuntime S3 跨文档 Hash 与 Postfix 生命周期

## 目标

补齐 cross-document document hash、postfix alias、mapped name、external document missing / unloaded / pending / restored / hash mismatch / rename 生命周期。S3 要让跨文档引用的 diagnostics 和恢复建议进入稳定 schema。

## 必读

- S0 / S1 已更新的 source 矩阵和 scope 矩阵。
- `src/App/PropertyLinks.cpp`
- `src/App/ElementNamingUtils.h`
- `src/App/Document.cpp`
- `cad-core/src/app/property_links.cpp`
- `cad-core/src/app/link.cpp`
- `cad-core/fixtures/c3m2/xlink-*.json`
- `cad-core/fixtures/p8/app-link-full-sublist-external-tag.json`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_expected_fixtures.py`

## 实现要求

- 区分 missing、unloaded、pending、restored、hash mismatch、object rename、label rename、mapped postfix mismatch。
- 输出必须是 request-local diagnostics / update suggestion，不允许后端持久保存外部文档状态。
- 与 `FullSubList`、`PropertyXLink*`、source-prefixed stable key 和 Link retag history 对齐。

## 非目标

- 不实现远程文件加载器。
- 不把完整外部文档 BREP 放进请求或响应。
- 不处理所有 UI rename 行为，只处理 CAD Core 需要的 graph / reference contract。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线 cad-core
```
