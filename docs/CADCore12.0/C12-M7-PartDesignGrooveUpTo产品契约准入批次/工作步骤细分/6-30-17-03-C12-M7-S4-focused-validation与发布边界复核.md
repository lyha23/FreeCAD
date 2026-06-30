# C12-M7 S4 focused validation 与发布边界复核

## 目标

复核 S3 的 expected/test/capability/docs 迁移是否一致，并确认没有误发布 FreeCAD parity 或 broad PartDesign 支持。

## 必读来源

- S0-S3 已实现文档
- 本包矩阵
- touched expected/test/capability/docs 文件

## 操作

1. 运行 focused tests。
2. 检查 capability JSON 中 `part_design.revolution_groove` 的 status、fixtures、diagnostics、narrowed gap / product contract wording。
3. 复核 non-goals：CopyOnChange、RuledSurface、full Groove family、geometry C++ parity 都未被误关闭。
4. 更新 validation / blocker / scope 矩阵。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_publication_smoke
cd /Users/li/Chili3DProject/FreeCAD
git diff --check
```
