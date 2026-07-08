# C13-M1 S3 runtime 发布器 C++ 实现

## 目标

实现 `runtime/topo_naming_state` 模块，并在正式 response 中输出 `topoNamingState`。

## 必读文件

- S2 red tests
- `cad-core/include/cad_core/runtime/recompute.h`
- `cad-core/src/runtime/recompute.cpp`
- `cad-core/include/cad_core/runtime/compute_context.h`
- `cad-core/include/cad_core/part/topo_shape.h`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/src/part/topo_shape_mapper.cpp`
- `cad-core/CMakeLists.txt`

## 操作

1. 新增 `cad-core/include/cad_core/runtime/topo_naming_state.h`。
2. 新增 `cad-core/src/runtime/topo_naming_state.cpp`。
3. 在 `recomputeResultJson()` 中缓存 target `responseSubshapes()`。
4. 最终 payload 添加 `topoNamingState`。
5. 实现 producer 继承、document/object hash、object payload、element map entries、child maps、mapper history 输出。
6. 保证 legacy test output 不被无关改动破坏；如需 legacy 输出 state，单独说明。

## 关闭条件

- S2 tests 通过。
- `cad-core` 构建通过。
- 没有在 executor/adapter 层新增 fixture 特判。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
build/cad-core recompute fixtures/p2/rect-pad-pocket.json --output out/c13m1-rect-pad-pocket.result.json
jq '.topoNamingState.schemaVersion, (.topoNamingState.objects | keys)' out/c13m1-rect-pad-pocket.result.json
python3 -m unittest tests.test_adapters.CadCoreAdapterTest
cd ..
git diff --check
```
