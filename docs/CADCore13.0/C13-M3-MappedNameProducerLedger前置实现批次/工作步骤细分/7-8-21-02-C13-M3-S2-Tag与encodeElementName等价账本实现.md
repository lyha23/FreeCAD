# C13-M3 S2 Tag 与 encodeElementName 等价账本实现

## 目标

实现 FreeCAD-equivalent mapped-name encode helper 与 request-local tag/op ledger，不依赖 expected 字符串。

## 必读文件

- S1 输出
- `src/App/ElementMap.cpp::encodeElementName()`
- `src/App/MappedName.cpp::MappedName::findTagInElementName()`
- `cad-core/CMakeLists.txt`
- `cad-core/tests/test_topo_naming_state_response.py`

## 操作

1. 新增 `cad-core/include/cad_core/topo/freecad_mapped_name_codec.h` 与 `cad-core/src/topo/freecad_mapped_name_codec.cpp`，或按 repo 现状选择等价位置。
2. 实现 source-backed raw/canonical encode 的最小 helper。
3. 在 CMake source list 注册。
4. 保留 FreeCAD source 注释，说明 `;:H<tag>:<len>,<type>`、hash/delete canonical 边界。

## 关闭条件

- `C13M3-BLOCKER-201` 关闭。
- build 通过。
- helper 不读取 expected、fixture 或 phase/case。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_topo_naming_state_response
cd ..
git diff --check
```
