# C13-M1 S2 topoNamingState 发布器红线测试

## 目标

先用 focused tests 锁定 response state 发布与 round-trip 消费，再写 C++ 实现。

## 必读文件

- S1 输出
- `cad-core/tests/fixture_runner.py`
- `cad-core/tests/test_adapters.py`
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_topo_state_fixture_migration.py`
- `cad-core/fixtures/p2/rect-pad-pocket.json`
- `cad-core/fixtures/c4m6/topo-state-body-tip-stable-recovery.json`

## 操作

1. 新增或扩展 adapter focused test：正式 C API/worker/wasm response 包含 `topoNamingState`。
2. 新增 runtime focused test：`p2/rect-pad-pocket` response 中 `topoNamingState.objects.Body.elementMap` 存在。
3. 新增 round-trip test：第一次 response 的 `topoNamingState` 放回请求后，stable reference recovery 不回退。
4. 对 hash/mapped-name 未实现项只做 gap 断言，不强行要求 FreeCAD 原文一致。

## 关闭条件

- focused tests 先红后绿条件写清楚。
- implementation matrix 标明每条 test 对应的实现落点。
- blocker queue 中 S2 blocker 关闭后才能进入 S3。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_adapters.CadCoreAdapterTest
cd ..
git diff --check
```
