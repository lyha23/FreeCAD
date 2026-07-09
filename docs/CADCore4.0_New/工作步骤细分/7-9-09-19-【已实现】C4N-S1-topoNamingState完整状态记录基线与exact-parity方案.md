# 【已实现】C4N-S1 topoNamingState 完整状态记录基线与 exact parity 方案

## 目标

建立新 `CADCore4.0_New` 主线的第一批实现闭环：以 C4M6 为最小完整语义批次，把完整 `topoNamingState` runtime publication 从“结构正确”推进到“FreeCAD expected exact parity”。

## 当前基线

已具备：

- `topoNamingState` runtime 发布完整对象框架。
- C4M6 Body Tip child map、Compound child map、mapperHistory、ReferenceShadow update、hard-fail 行为已有测试覆盖。
- native collector 校验当前 C4M6 expected 可通过。
- C4M6 Body Tip child map 和 Compound child map 的 entry key、`mappedName.canonical`、raw mapped-name 结构、endpoint 与 evidence 已纳入 native expected parity gate。

未具备：

- p2 / p6 的 Body mapped-name exact parity 仍保留在后续 C4N-S2/S3 expectedFailure 范围。
- 当前 C4M6 Face Prism producer ledger 已覆盖单矩形 profile Pad；任意 profile 的完整 `makeShapeWithElementMap` / `StringHasher` 序列仍归后续批次通用化。

## 实现状态

- `cad-core/src/part/topo_shape.cpp` 在矩形 face-prism producer 路径记录 Tip-local FreeCAD-style produced element names，最终 `;:H` tag 仍来自本次请求 producer tag。
- `cad-core/src/runtime/topo_naming_state.cpp` 发布 Body Tip child map 时优先消费 Tip `NamedShape` 的局部 element map；Compound child map 保持 child-local token，不把 source owner 写入 mapped name。
- `cad-core/tests/test_topo_naming_state_response.py` 已把 C4M6 Body / Compound child map key、canonical token、raw token 结构和 evidence 纳入 expected parity 断言。

## FreeCAD 依据

实现前必须复核这些源码语义：

- `/Users/li/Chili3DProject/FreeCAD/src/App/ElementMap.cpp`
  - `ElementMap::encodeElementName(...)`
  - `ElementMap::addChildElements(...)`
  - `ElementMap::hashChildMaps(...)`
- `/Users/li/Chili3DProject/FreeCAD/src/App/MappedName.cpp`
  - mapped-name hash / canonical 解析规则
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp`
  - `TopoShape::mapSubElement(...)`
  - child map / source range 传播
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp`
  - Body Tip Shape 写回和 Tip subshape namespace

## cad-core 落点

| 落点 | 任务 |
| --- | --- |
| `cad-core/src/topo/freecad_mapped_name_codec.cpp` | 补 FreeCAD mapped-name raw/canonical 编码规则，不把 display token 当 durable token |
| `cad-core/src/part/topo_shape.cpp` | 在 ElementMap 写入点保留 producer element id、occurrence、operation postfix、source tag |
| `cad-core/src/runtime/topo_naming_state.cpp` | 只序列化真实 topo ledger，不从 expected sidecar / fixture 名称补数据 |
| `cad-core/src/part/part_compound.cpp` | 保持 Compound child map source ranges，但 entry key 交给 topoNamingState child-local 规则 |
| `cad-core/src/part_design/body.cpp` | 保持 Body Tip child map，确保 child entries 消费 Tip 的真实 ElementMap |
| `cad-core/tests/test_topo_naming_state_response.py` | 增加 C4M6 native expected exact parity 断言 |

## 实施步骤

1. 先补 failing tests：
   - Body `elementMap.entries` key 集合等于 expected。
   - Body child map entries key / raw / canonical / evidence 等于 expected。
   - Compound child map entries key / raw / canonical / evidence 等于 expected。
   - mapperHistory ids 和 ReferenceShadow update 继续保持现有通过状态。
2. 修 `freecad_mapped_name_codec` 和 `topo_shape` producer evidence：
   - 不再把 `Pad.EdgeN` 作为最终 durable token。
   - 对 Pad / Body Tip 生成 `Pad.#...;XTR;:H...` 风格 token。
   - 对 Compound child map 生成 child-local `EdgeN;:H...` 风格 token。
3. 修 `topo_naming_state` publication：
   - `entries` key 必须等于 `mappedName.canonical`。
   - child map entry 的 `source.object` / `target.object` 仍完整保留 owner 信息。
   - split / deleted / ambiguous 不写入唯一 `elementMap.entries`。
4. 跑 C4M6 检查并记录差异：
   - native expected check 只证明 oracle 文件没漂；exact parity 由 unittest 负责。

## 非目标

- 不扩大到 p2 / p6 expectedFailure；那是 C4N-S2/S3 之后的扩展。
- 不手改 `cad-core/fixtures/**/expected/*.freecad.json`。
- 不恢复 sidecar overlay。
- 不用 bbox、面积、fixture 名或输出排序猜 token。
- 不把完整 BREP 放进 `topoNamingState`。

## 验收命令

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_topo_naming_state_response

cd /Users/li/Chili3DProject/FreeCAD
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd \
  python3 cad-core/tools/collect_freecad_expected.py --phase c4m6 --check --skip-unsupported
git diff --check -- docs/CADCore4.0_New cad-core
```

完成后把本文件重命名为：

`7-9-09-19-【已实现】C4N-S1-topoNamingState完整状态记录基线与exact-parity方案.md`
