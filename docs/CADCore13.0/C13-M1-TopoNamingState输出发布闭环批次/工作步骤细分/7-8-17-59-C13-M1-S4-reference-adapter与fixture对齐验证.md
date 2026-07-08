# C13-M1 S4 reference adapter 与 fixture 对齐验证

## 目标

验证输出 `topoNamingState` 不只是存在字段，还能被下一次请求消费，并且 adapter / worker / wasm 响应一致。

## 必读文件

- S3 实现 diff
- `cad-core/src/runtime/reference_resolution.cpp`
- `cad-core/src/runtime/element_reference_update.cpp`
- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/src/adapters/cli/cli.cpp`
- `cad-core/tools/collect_freecad_expected.py`
- focused fixture expected

## 操作

1. 运行 CLI recompute，保存 `cad-core/fixtures/<phase>/cad-core-res/*.cad-core.json` 或 `cad-core/out/` 临时结果，不混入 `expected/`。
2. 验证 C API、worker、wasm 均返回 `topoNamingState`。
3. 把一次 response 的 `topoNamingState` 注入下一次请求，验证 stable reference recovery。
4. 对照 expected 记录剩余差异：
   - `hash_encoding_gap`
   - `freecad_mapped_name_encoding_gap`
   - `child_element_map_key_gap`
   - `mapper_history_id_gap`

## 关闭条件

- focused validation matrix 更新。
- adapter response contract 没有回退。
- C13-M1 必须字段通过；后续字段都有 gap 分类。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_adapters.CadCoreAdapterTest
python3 -m unittest tests.test_topo_state_fixture_migration
cd ..
git diff --check
```
