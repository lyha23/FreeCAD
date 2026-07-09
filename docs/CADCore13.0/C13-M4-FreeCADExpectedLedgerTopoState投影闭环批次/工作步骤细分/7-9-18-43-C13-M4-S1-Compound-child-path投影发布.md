# C13-M4 S1 Compound child-path 投影发布

## 目标

在 `cad-core/src/runtime/topo_naming_state.cpp` 中补通用 child path projection 发布，让 `Compound` 的 public topoNamingState 能表达 `Child0.Face1` 这类 ledger 可解释的引用目标。

## 必读文件

- S0 输出
- `cad-core/src/runtime/topo_naming_state.cpp`
- `cad-core/src/runtime/recompute.cpp`
- `cad-core/include/cad_core/part/topo_shape.h`
- `cad-core/fixtures/c4m6/topo-state-link-compound-child-maps.json`
- `cad-core/fixtures/c4m6/expected/topo-state-link-compound-child-maps.freecad.json`

## 操作

1. 读取当前 `NamedShape.childElementMaps` 和 response subshape publication 的关系。
2. 为非 Body owner 支持 input-reference-driven projection map，例如 `Compound:ChildBoxA`。
3. 发布 projection subshape：`Compound.subshapes.Child0.Face1`。
4. 发布 projection child map entry：`ChildBoxA.#f:1;BOX,F -> Compound.Child0.Face1`。
5. 合并 projection entry 到顶层 `Compound.elementMap.entries`，使用 owner-qualified key，避免覆盖普通 child-local canonical key。
6. 保留 schema/producer/documentHash/objectHash hard fail 行为。

## 关闭条件

- `C13M4-IMPL-101` 关闭。
- `topo-state-link-compound-child-maps` actual response 包含 `topoNamingState.objects.Compound.subshapes.Child0.Face1`。
- actual response 包含 `Compound:ChildBoxA` projection child map。
- 不复制 expected JSON 字符串，不按 fixture 名称分支。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 tools/validate_freecad_expected_ledger.py --phase c4m6 --strict
python3 -m unittest tests.test_topo_naming_state_response
```
