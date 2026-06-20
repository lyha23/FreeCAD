# 【已实现】P8-LinkAssemblyRuntime S3 跨文档 Hash 与 Postfix 生命周期

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

## 完成结论

S3 已完成。当前实现按 P8LAR-SRC-005/006/008 收口在 request-local schema：`PropertyXLink` 的 missing / unloaded / pending / restored / hash mismatch 由 `cad-core/src/graph/recompute_plan.cpp` 与 `cad-core/src/runtime/recompute.cpp` 输出 diagnostics、`documentReference` 和 `elementReferenceUpdates`；label rename、object rename、DocMap name/label rename、FullSubList 外部前缀和 `;:X` mapped postfix mismatch 由 `cad-core/src/app/property_links.cpp` 的归一化和 Link retag alias 结果锁定。CAD Core 不保存外部文档状态，不实现 loader，也不传递完整外部文档 BREP。

本轮新增 `cad-core/fixtures/c3m2/xlink-mapped-postfix-rename-recovery.json`，并在 `cad-core/tests/test_p8_features.py` 增加 object rename 与 mapped postfix mismatch focused tests；已有 `xlink-missing-external-document`、`xlink-pending-external-document`、`xlink-unloaded-external-document`、`xlink-pending-external-document-restored`、`xlink-document-hash-mismatch`、`label-rename-recovery`、`cross-document-nested-label-rename-recovery` 和 `app-link-full-sublist-external-tag` 继续作为 S3 回归基线。

验证结果：`cmake --build build` 通过；`python3 -m unittest tests.test_p8_features tests.test_expected_fixtures` 通过 151 个测试，跳过 14 个 known gap。S4 仍保留多层 LinkSub / imported ElementMap / retag history 深链，S5 仍排队 Assembly solver 扩展，S6 仍排队 Web runtime 合同冻结。
