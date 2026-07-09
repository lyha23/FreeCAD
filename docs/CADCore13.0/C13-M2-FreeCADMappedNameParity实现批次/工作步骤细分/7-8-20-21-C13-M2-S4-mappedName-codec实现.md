# C13-M2 S4 mappedName codec 实现

## 目标

实现 FreeCAD focused raw/canonical mappedName codec/helper，并让 runtime state publisher 消费它。

## 前置状态

C13-M3 S5 已回流：producer-ledger 前置阻塞已由 C13-M3 S1-S4 解除，S4 可恢复/继续执行。下一步应复核当前 live code、tests 和 fixture 范围，正式关闭 C13-M2 S4 或记录 narrowed blocker；不要重新把“缺 producer ledger”当成阻塞理由。`childElementMapKey` / `mapperHistoryIds` 仍属于 C13-M2 S5/S6，不在 S4 标成 supported。

## 必读文件

- S1/S2/S3 输出
- `src/App/MappedName.cpp`
- `src/App/ElementMap.cpp`
- `cad-core/src/runtime/topo_naming_state.cpp`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/src/part/topo_shape_mapper.cpp`
- `cad-core/CMakeLists.txt`

## 操作

1. 复核或扩展 mapped-name codec/helper，位置优先 `cad-core/include/cad_core/topo/` 或 `cad-core/include/cad_core/part/`。
2. 确认 helper 注释标明 FreeCAD 源文件、函数和关键字段。
3. 确认 `runtime/topo_naming_state.cpp` 消费 helper 输出 `mappedName.raw/canonical`，且没有 stable-token fake raw fallback。
4. 复核 focused tests 当前是否已普通通过；若已满足，关闭 C13-M2 S4 docs/matrix；若仍有差异，记录 narrowed blocker。

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
