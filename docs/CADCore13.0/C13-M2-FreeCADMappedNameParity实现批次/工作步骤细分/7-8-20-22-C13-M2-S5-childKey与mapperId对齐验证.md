# C13-M2 S5 childKey 与 mapperId 对齐验证

## 目标

验证或 blocker 化 `childElementMapKey` 与 `mapperHistoryIds` evidence，对 focused fixtures 做最终差异分类。

## 前置状态

C13-M3 S5 只解除 C13-M2 S4 的 producer-ledger 前置阻塞，不关闭本步骤。`childElementMapKey` 与 `mapperHistoryIds` 仍未标成 supported；S5 必须基于 FreeCAD source、live expected 和当前输出实现或 blocker 化，不能因 C13-M3 raw/canonical 通过而顺手标绿。

## 必读文件

- S4 输出
- `src/App/ElementMap.cpp::hashChildMaps()`
- `src/App/ElementMap.cpp::getElementHistory()`
- `cad-core/src/part/topo_shape_mapper.cpp`
- `cad-core/src/runtime/topo_naming_state.cpp`
- focused `cad-core-res` 与 expected

## 操作

1. 刷新 focused `cad-core-res` 输出，不写 expected。
2. 对比 child key / mapper id evidence。
3. 能 source-backed 实现的补齐；不能实现的记录为 source blocker，不伪支持。
4. 更新 validation / fixture / blocker matrix。

## 关闭条件

- child key 和 mapper id 不再是未分类 gap。
- focused outputs 的 remaining diff 清晰。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_topo_naming_state_response
cd ..
git diff --check
```
