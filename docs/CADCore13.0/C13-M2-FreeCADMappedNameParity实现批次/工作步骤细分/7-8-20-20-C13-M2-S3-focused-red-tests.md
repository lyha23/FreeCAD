# C13-M2 S3 focused red tests

## 目标

先用 focused tests 锁定 mappedName evidence parity，再实现 codec/helper。

## 必读文件

- S2 输出
- `cad-core/tests/test_topo_naming_state_response.py`
- `cad-core/tests/test_adapters.py`
- focused fixtures 和 expected
- `cad-core/src/runtime/topo_naming_state.cpp`

## 操作

1. 新增 focused tests：`mappedName.raw/canonical` 不应只是 stable token fallback。
2. 对 p2 / c4m6 / p6 锁定 raw/canonical mappedName schema parity。
3. 对 child key / mapper ids 写明确 redline 或 blocker test。
4. 若当前实现尚未满足，使用 guarded expectedFailure 并写清 S4 删除条件。

## 关闭条件

- implementation matrix 标明测试对应实现落点。
- blocker queue 中 S3 blocker 关闭后才能进入 codec 实现。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_topo_naming_state_response
cd ..
git diff --check
```
