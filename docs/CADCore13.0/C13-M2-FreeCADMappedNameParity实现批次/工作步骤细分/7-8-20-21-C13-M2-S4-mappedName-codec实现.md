# C13-M2 S4 mappedName codec 实现

## 目标

实现 FreeCAD focused raw/canonical mappedName codec/helper，并让 runtime state publisher 消费它。

## 必读文件

- S1/S2/S3 输出
- `src/App/MappedName.cpp`
- `src/App/ElementMap.cpp`
- `cad-core/src/runtime/topo_naming_state.cpp`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/src/part/topo_shape_mapper.cpp`
- `cad-core/CMakeLists.txt`

## 操作

1. 新增或扩展 mapped-name codec/helper，位置优先 `cad-core/include/cad_core/topo/` 或 `cad-core/include/cad_core/part/`。
2. 在 helper 注释中标明 FreeCAD 源文件、函数和关键字段。
3. `runtime/topo_naming_state.cpp` 消费 helper 输出 `mappedName.raw/canonical`。
4. 移除 S3 guarded expectedFailure，让 focused tests 普通通过。

## 关闭条件

- focused mappedName red tests 变绿。
- 没有 adapter / fixture 字符串特判。
- build 通过。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_topo_naming_state_response
git diff --check
```
