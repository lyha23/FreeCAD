# P8-LinkAssemblyRuntime S2 Link 账本与 ShowElement 事务

## 目标

实现或补齐完整 FreeCAD Link ledger 和 `ShowElement=true` LinkElement / LinkGroup 持久写回事务生命周期。重点是 owner / child / sync / delete / reclaim / copy-on-change / touched / placement list 优先级，而不是只修一个 display fixture。

## 必读

- S0 / S1 已更新的主线方案、总入口和矩阵。
- `src/App/Link.cpp`、`src/App/Link.h`
- `src/App/DocumentObjectGroup.cpp`
- `cad-core/src/app/link.cpp`
- `cad-core/include/cad_core/app/link.h`
- `cad-core/fixtures/p8/app-link-show-element-*.json`
- `cad-core/fixtures/p8/app-link-element-count-*.json`
- `cad-core/tests/test_p8_features.py`

## 实现要求

- `documentObjectUpdates` 必须表达前端可应用的持久 graph 变更建议；CAD Core 不在后端保存 graph。
- 同一批次覆盖 materialized child、synthetic child、stale child、toggle-off reclaim、ElementList owner sync、child sync、PlacementList / ScaleList / VisibilityList 优先级。
- CopyOnChange / touched / deep copy 只在 FreeCAD source authority 明确且 fixture 能约束时发布 supported，否则保留 diagnostic 或 backendGap。

## 非目标

- 不处理跨文档 hash / postfix；留给 S3。
- 不处理 imported shape ElementMap；留给 S4。
- 不扩展 Assembly solver；留给 S5。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线 cad-core
```
