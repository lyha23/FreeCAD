# C4N-S2 FreeCAD ElementMap / MappedName producer 语义通用化方案

## 目标

在 C4N-S1 已完成 `cad-core/fixtures/c4m6` exact parity 的基础上，下一批不继续堆单 fixture 规则，而是把 `cad-core` 的 topoNamingState producer 命名推进到 FreeCAD `ElementMap` / `StringHasher` / `TopoShape::makeShapeWithElementMap` 的通用语义。

本批已完成，`cad-core/tests/test_topo_naming_state_response.py` 中 p2 / p6 的 mapped-name expectedFailure 已移除：

- `p2/rect-pad-pocket` 的 `Body` raw / canonical mapped name 对齐 native expected。
- `p6/up-to-face-stable-body-history` 的 `ProbePad` 和 `Body` stable body history 对齐 native expected。
- `c4m6/topo-state-body-tip-stable-recovery` 与 `c4m6/topo-state-link-compound-child-maps` 保持回归通过。

## 完成状态

- 协议权威仍是 `docs/接口规定/7-8-11-08-topoNamingState客户端携带状态接口方案.md`：`topoNamingState` 是客户端携带的一份状态快照，不是后端 session cache。
- C4N-S1 已关闭 c4m6：Body Tip child map、Compound child map、mapperHistory、hard-fail 和 ReferenceShadow 边界已经进入 focused 回归。
- 本批关闭的显式红线在 `cad-core/tests/test_topo_naming_state_response.py`：
  - `test_c13m2_p2_body_mapped_name_raw_canonical_matches_freecad_expected`
  - `test_c13m2_p6_probe_pad_mapped_name_raw_canonical_matches_freecad_expected`
- `p2/rect-pad-pocket` 的 Body child element map 已发布 FreeCAD 风格 `Pocket.#...;CUT...` raw / canonical entry，避免从 Tip feature 的 XTR stable token 伪造 raw mapped name。
- `p6/up-to-face-stable-body-history` 的 ProbePad UpToFace 路径已通过 `PSM` producer ledger 发布 source-backed raw / canonical entry。
- native expected 只认 `cad-core/fixtures/**/expected/*.freecad.json`，并由 `cad-core/tools/collect_freecad_expected.py` + 本机 `FreeCADCmd` 采集或校验。
- `*.expeted.json` 属于 cad-core contract / diagnostic 对照证据，不是 native FreeCAD oracle，不参与本批 expected 裁决。

## FreeCAD 依据

本批实现必须从以下源码建立语义映射，不能从 fixture 输出倒推：

- `src/App/ElementMap.cpp::ElementMap::setElementName()`：负责元素名去重、重复名重命名和最终写入。
- `src/App/ElementMap.cpp::ElementMap::encodeElementName()`：负责 source tag、postfix、hash 和 element type 组合。
- `src/App/ElementMap.cpp::ElementMap::hashElementName()`：只在名称包含 element map prefix 时进入 `StringHasher`。
- `src/App/StringHasher.cpp::StringID::toString()`：输出 `#<hex>` 与 `#<hex>:<index>` 形式。
- `src/App/StringHasher.cpp::StringHasher::getID()`：建立 mapped-name string id，`getID(long id, int index)` 只查已存在 id。
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap()`：先收集 modified / generated source，再构造 producer 名称，再执行 reverse pass / forward pass 补低阶或高阶元素名称。
- `src/Mod/PartDesign/App/Body.cpp` 与 `src/Mod/Part/App/BodyBase.cpp`：Body Tip 是对外发布形状的入口，不能把 Body 的 child map 简化成 Tip feature 的裸 stable token。

相邻 C++ 新增语义代码要按仓库规则写 FreeCAD 来源注释，至少包含源文件、函数名和支撑当前语义的短句或字段名。

## 代码落点

| 层 | 落点 | 本批职责 |
| --- | --- | --- |
| topo | `cad-core/src/topo/freecad_mapped_name_codec.cpp` | 补齐 FreeCAD mapped-name 编码、hash token、canonical raw 对照规则；继续保持 `:H` 归一化只用于测试比较，不污染 runtime raw。 |
| part/topo | `cad-core/src/part/topo_shape.cpp` | 将 C4N-S1 的矩形棱柱 producer seed 扩成 request-local producer ledger，按 generated / modified / preserved / lower / upper pass 发布 ElementMap entry。 |
| runtime | `cad-core/src/runtime/topo_naming_state.cpp` | 只发布 source-backed 且可解释的 entry；split / deleted / ambiguous 保留在 mapperHistory 或 diagnostics，不伪造成 stable raw mapped name。 |
| tests | `cad-core/tests/test_topo_naming_state_response.py` | p2/p6 通过后移除对应 `expectedFailure`，保留 p5/p8 indexed-only 边界测试和 c4m6 回归守卫。 |
| fixtures | `cad-core/fixtures/{p2,p6,c4m6}` | 只使用 native expected 和 cad-core-res 对照输出；不得手改 `expected/*.freecad.json`。 |

## 实现原则

1. 先建立 producer ledger，再改 runtime 输出。runtime 不能靠 bbox、fixture 名、输出顺序或 stable token 猜 raw mapped name。
2. `StringID` / `#id[:index]` 生命周期必须是 request-local 的 FreeCAD 兼容 ledger，不是全局稳定 id，也不是前端长期状态。
3. Body / Tip / childElementMaps 的归属以 FreeCAD 发布形状和 ElementMap child 关系为准；Body 对外可见的 topology 不能退化成 Tip object 的裸输出。
4. canonical 比较只用于跨环境 hash 差异归一化；response 中的 raw mapped name 应保持 FreeCAD 风格。
5. 遇到 native expected 与当前 cad-core 输出不一致，先判定属于 collector、producer ledger、child map、命名顺序差异还是 unsupported，不用当前 cad-core 输出改 expected。

## 非目标

- 不实现服务端 session 或后端缓存；`topoNamingState` 仍由客户端随请求携带。
- 不把本批扩大成全 phase fixture 收口；本批只关闭 p2/p6 producer 语义红线并守住 c4m6。
- 不手工编辑 `expected/*.freecad.json`。
- 不在 adapter、document parser 或 frontend contract 层加入 fixture 特判。
- 不用 BREP 作为建模输入或长期状态；唯一例外仍是协议已允许的 `ReferenceShadow.brep`。

## 验收

### 本轮短跑

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_topo_naming_state_response
```

### native expected 校验

```bash
cd /Users/li/Chili3DProject/FreeCAD
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd \
  python3 cad-core/tools/collect_freecad_expected.py --phase p2 --check --skip-unsupported
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd \
  python3 cad-core/tools/collect_freecad_expected.py --phase p6 --check --skip-unsupported
```

### 文档与补丁检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore4.0_New cad-core
```

### 阶段收口

只有 p2/p6 expectedFailure 移除并通过后，才执行：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_expected_fixtures tests.test_adapters
```

## 完成判定

- `test_topo_naming_state_response.py` 不再对 p2/p6 producer parity 使用 `expectedFailure`。
- `rect-pad-pocket.freecad.json` 与 `up-to-face-stable-body-history.freecad.json` 中目标对象的 `elementMap.entries` raw / canonical / producer evidence 与 cad-core response 对齐。
- `c4m6` 的 Body Tip 和 Compound child maps 没有回退。
- README 与矩阵已更新为 `C4N-S2` 已实现，本文件按仓库规则改名为 `7-9-10-26-【已实现】C4N-S2-FreeCAD-ElementMap-MappedName-producer语义通用化方案.md`。
