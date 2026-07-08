# 【已实现】C13-M3 S2 Tag 与 encodeElementName 等价账本实现

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

## 关闭结果

- 新增 `cad-core/include/cad_core/topo/freecad_mapped_name_codec.h` 与 `cad-core/src/topo/freecad_mapped_name_codec.cpp`，并在 `cad-core/CMakeLists.txt` 注册进 `cad-core-lib`。
- helper 提供 `encodeFreeCadMappedName()`、`canonicalizeFreeCadMappedName()` 与 `encodedMappedNameProvenance()`：只从 `MappedNameProvenance.sourceElement/sourceTag/operationPostfix/elementType` 编码 raw/canonical；缺 source/tag/op/type evidence 时返回 missing/blocker status，不用 stable token 伪造 raw。
- 代码旁记录 FreeCAD source：`ElementMap::encodeElementName(... masterTag ... postfix ... tag ...)`、`MappedName::findTagInElementName()` 解析 `;:H<tag>:<len>,<type>`，以及 `ElementNamingUtils.h` 的 `POSTFIX_TAG` / `POSTFIX_GEN` / `POSTFIX_MOD` / `POSTFIX_DUPLICATE` 常量。
- canonicalization 对齐当前 expected comparator 边界：`:H...` 归一为 `:H*` 或 `:H*:*`，`;D...` 归一为 `;D*`；完整 FreeCAD SID hash/dehash 与 child-map key 仍留给 producer evidence 与后续 S3-S5。
- 本步不填充 PartDesign producers、不改 `runtime/topo_naming_state.cpp`、不移除 expectedFailure guard、不编辑 fixtures/expected/cad-core-res。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_topo_naming_state_response
cd ..
git diff --check
```

本步验收已通过；队列从 S3 开始。
