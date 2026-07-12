> 状态：早期设计背景。root fixture、role manifest、public/ledger native oracle 与 release-gate 边界以 [输入输出约定](../输入输出约定.md)、[检查流程](../检查流程.md) 和当前工具规定为准。producer trace 不是第三个 native 权威，只在 public/ledger 无法对齐时按需参考。

对，`fixtures/<phase>/*.json` 应该作为**根输入 fixture**单独设计和验收。它和 `fixtures/<phase>/expected/*.freecad.json` 的职责不一样：

```text
fixtures/<phase>/*.json
  = FreeCADCmd recompute 的根输入 / 前态账本 / 引用恢复场景声明

fixtures/<phase>/expected/*.freecad.json
  = FreeCADCmd recompute 后生成的权威输出 oracle
```

所以应该有两个 validator：

```text
tools/validate_freecad_fixture_inputs.py
  验收 fixtures/<phase>/*.json 根输入是否闭合、可解析、可作为权威输入

tools/validate_freecad_expected_ledger.py
  验收 fixtures/<phase>/expected/*.freecad.json 输出账本是否完整
```

---

## 根输入 fixture 的定位

`fixtures/<phase>/*.json` 不应该被设计成“完整输出账本”。它应该是：

```text
建模请求
+ recompute target
+ 可选的 topoNamingState 前态账本
+ 可选的引用恢复/失败场景声明
```

比如 `topo-state-body-tip-stable-recovery.json` 里，`Objects` 声明 Sketch、Pad、Body，`Pad.Profile` 使用 `StableSubListSource: topoNamingState`，recompute 目标是 `Body`。这说明 fixture 是一个“用旧 stable token 重新建模并恢复引用”的输入请求。 它的 `topoNamingState` 只包含 `Sketch` 的前态账本，用来把 `g1;SKT;FAC` 解析回 `InternalFace1`，并不是 recompute 后的完整输出账本。

再比如 `topo-state-link-compound-child-maps.json`，输入里的 `CompoundLink.LinkedObject` 引用了 stable token `Compound/ChildBoxA.#f:1;BOX,F`，而 `topoNamingState.objects.Compound` 提供这个 token 对应的 child path 账本。

还有一类是 protocol probe，比如 `topo-state-mapper-history-events.json`。它只有一个 `CadCore::TopoNamingStateProbe` 对象，输入 `topoNamingState.objects` 是空的；这类 fixture 的作用不是提供真实 FreeCAD native 前态账本，而是驱动 collector 生成 mapperHistory DTO/protocol oracle。

所以根输入 fixture 应该分成几种明确模式。

---

## 建议的根输入 fixture 模式

### 1. 普通 FreeCAD recompute fixture

没有 `topoNamingState`。

```json
{
  "fixtureCategory": "part.box.basic",
  "Objects": [...],
  "recompute": {
    "objs": ["Box"]
  }
}
```

它只验证 FreeCADCmd 可以创建对象、recompute、生成 expected。

### 2. state-backed recovery fixture

有 `topoNamingState`，并且某些 link property 使用：

```json
"StableSubListSource": "topoNamingState"
```

这类 fixture 表示：

```text
我有一个旧的 stable token；
请用输入 topoNamingState 把它解析成 FreeCAD 当前可用的 indexed subname；
然后执行 recompute；
最后生成新的 expected oracle。
```

这类 fixture 的 `topoNamingState` 是 **pre-recompute state**，不是 output state。

### 3. child path / child map fixture

这类 fixture 用来证明：

```text
旧 stable token 可以通过 childElementMaps 解析到 owner object 的 child path。
```

比如：

```text
Compound/ChildBoxA.#f:1;BOX,F
  -> Compound.Child0.Face1
```

这种输入 fixture 必须保证 `topoNamingState.objects[target].childElementMaps` 能解析被引用的 stable token。

### 4. mapperHistory/protocol probe fixture

这类 fixture 不一定有真实 native 前态账本。它的作用是声明一个 mapperHistory DTO contract，例如：

```text
generated / modified / split / deleted / merge / ambiguous
```

这种 fixture 应该明确标记为 probe，不能和“FreeCAD native 前态账本”混在一起。

---

## 我建议给根输入增加一个轻量 contract 字段

短期可以不改已有 fixture，靠 `fixtureCategory` 推断。但长期最好加：

```json
"fixtureContract": {
  "schemaVersion": "cad-core.fixture.v1",
  "inputRole": "pre_recompute_state",
  "authority": "FreeCADCmd",
  "requires": [
    "state_backed_stable_sublist_resolution",
    "child_element_map_resolution"
  ]
}
```

`inputRole` 可以是：

```text
none
pre_recompute_state
protocol_probe_seed
negative_incompatibility_case
```

这样 validator 不需要猜。

如果暂时不想改 JSON，可以先用规则：

```text
fixtureCategory contains "topoNamingState.mapperHistory"
  -> protocol_probe_seed

fixtureCategory contains "schema-incompatible" / "producer-incompatible"
  -> negative_incompatibility_case

存在 StableSubListSource=topoNamingState
  -> pre_recompute_state

否则
  -> normal_recompute
```

---

# 根输入 validator 应该验证什么

建议新增：

```text
cad-core/tools/validate_freecad_fixture_inputs.py
```

命令：

```bash
cd cad-core

python3 tools/validate_freecad_fixture_inputs.py --phase c4m6 --strict

python3 tools/validate_freecad_fixture_inputs.py --all --strict

python3 tools/validate_freecad_fixture_inputs.py --phase c4m6 --strict --freecadcmd-dry-run
```

它的职责不是验证 expected 输出，而是证明根输入 fixture 自身合法、闭合、可作为 FreeCADCmd 权威生成的根。

---

## 1. 顶层结构校验

每个 `fixtures/<phase>/*.json` 必须满足：

```text
Objects 是非空 list
Objects[*].Name 唯一
Objects[*].ID 唯一，如果存在
Objects[*].TypeId 非空
Objects[*].Properties 是 object 或可省略
recompute.objs 是非空 list
recompute.objs 每个对象必须存在于 Objects
fixtureCategory 非空
```

根输入不应该包含：

```text
results
objects
named_shapes
diagnostics
elementReferenceUpdates
```

这些是 output/result 概念，不应该出现在 root input。

---

## 2. Object graph / link 引用闭包

所有 property 里的 object reference 都必须能解析到 `Objects[*].Name`。

需要覆盖：

```text
App::PropertyLink
App::PropertyLinkList
App::PropertyLinkSub
App::PropertyLinkSubList
App::PropertyXLinkSub
App::PropertyXLinkSubList
```

检查规则：

```text
value 指向的对象必须存在
values 里的对象必须存在
SubSet[*].value 必须存在
```

如果某个 link property 里有：

```json
"StableSubListSource": "topoNamingState"
```

则必须有：

```text
StableSubList 是非空 list
value 是有效对象名
topoNamingState 存在
topoNamingState.objects[value] 存在
```

---

## 3. topoNamingState 输入前态校验

如果根输入带 `topoNamingState`，它应该被视为 **pre-recompute ledger**。

必须校验：

```text
topoNamingState.schemaVersion == cad-core.topo-state.v1
topoNamingState.producer.cadCoreVersion == fixture-contract-v1
topoNamingState.objects 是 object
```

`documentHash` 也应该校验。当前 collector 的 `fixture_document_hash()` 是对：

```python
{
  "Objects": fixture.get("Objects", []),
  "recompute": fixture.get("recompute", {}),
}
```

做 semantic hash。 所以 input validator 应该要求：

```text
topoNamingState.documentHash == semantic_hash({"Objects": Objects, "recompute": recompute})
```

这样可以防止有人改了 `Objects` 或 `recompute`，但忘了更新输入前态账本。

注意：输入 `topoNamingState.objects` 不必包含所有 `Objects`。它可以是最小前态账本。例如 body-tip fixture 只需要 `Sketch` 前态；link-compound fixture 只需要 `Compound` 前态。这是对的。

---

## 4. stable token 解析校验

核心规则：

```text
所有 StableSubListSource=topoNamingState 的 stable token，
必须能在输入 topoNamingState 中解析。
```

解析路径应该和 collector 保持一致：

```text
1. object_state.elementMap.entries
2. object_state.childElementMaps[*].elementMap.entries
3. object_state.mapperHistory
```

collector 当前的 `topo_state_entry_for_stable_subname()` 就是这个逻辑：先找 object 的 `elementMap.entries`，再找 `childElementMaps`，最后找 `mapperHistory`；如果 mapperHistory 是 `generated/modified/merge/historical` 且 target subname 是 indexed，就可以解析，否则返回 terminal diagnostic。

input validator 应该复用这套解析逻辑，而不是另写一套。

规则可以这样定：

```text
默认：StableSubList 必须解析到 indexed target subname。

如果解析到 split/deleted/ambiguous terminal event：
  只有 fixtureContract.inputRole == protocol_probe_seed
  或 fixtureContract.allowTerminalDiagnostics 包含该 code
  才允许通过。
```

否则，根输入就是不完整的。

---

## 5. SubList 与 StableSubList 的关系

`StableSubListSource=topoNamingState` 的情况下：

```text
StableSubList 是权威输入
SubList 只是 FreeCAD indexed fallback / legacy display hint
```

因此 validator 应该这样处理：

```text
如果 SubList 为空：
  OK，collector 会用 topoNamingState 解析出 native subname。

如果 SubList 非空：
  每个 StableSubList 解析出的 indexed subname 必须等于对应 SubList。
```

例如 link-compound fixture 里 `SubList` 是 `Child0.Face1`，stable token 是 `Compound/ChildBoxA.#f:1;BOX,F`，输入 state 应该能把它解析到同一个 `Child0.Face1`。

这样可以防止：

```text
SubList 写的是 Face1
StableSubList 实际解析成 Face2
```

这种隐藏错误。

---

## 6. ReferenceShadow 校验

如果 link item 里有 `ReferenceShadow`，必须校验：

```text
ReferenceShadow 是 list
ReferenceShadow 长度 == StableSubList 长度
ReferenceShadow[*].target 如果存在，必须等于 item.value
```

collector 现在已经要求 `ReferenceShadow` 和 `StableSubList` index-aligned；长度不一致会直接 Unsupported。 input validator 应该在更早阶段报错。

---

## 7. elementMap 输入账本校验

对输入 `topoNamingState.objects[*].elementMap.entries`：

```text
elementMap.encoding == cad-core.element-map.v1
entries 是 object
entry key 必须等于 mappedName.canonical
mappedName.canonical 必须等于 canonical_freecad_mapped_name(mappedName.raw)
target.object 必须等于当前 object，或明确允许 child-owner 场景
target.subname 必须存在于 object_state.subshapes
source.object 非空
source.subname 非空
recoverability 必须存在
evidence.source 必须存在
evidence.mapperHistoryIds 必须是 list
evidence.childElementMapKey 必须存在，可为 null
```

如果 `evidence.mapperHistoryIds` 引用了 mapper history id，则该 id 必须存在于同一个 object 的 `mapperHistory`。

---

## 8. childElementMaps 输入账本校验

对输入 `childElementMaps`：

```text
key 非空
ownerObject == 当前 object
childObject 必须是 Objects 中存在的对象
pathPrefix 非空
elementMap.entries 非空
每个 child entry 的 target.object == ownerObject
每个 child entry 的 source.object == childObject
每个 child entry 的 evidence.childElementMapKey == child map key
```

这能证明 child stable token 不是孤立字符串，而是有 owner/child/path 映射闭包。

---

## 9. mapperHistory 输入账本校验

对输入 mapperHistory：

```text
id 唯一
relation 合法
source.object/source.subname 存在
target.object/target.subname 结构合法
recoverability 存在
```

并且：

```text
elementMap entry 只能引用 generated/modified/merge/historical 这类 resolved event。

split/deleted/ambiguous 这类 terminal event 不能被 elementMap 当成 resolved entry 引用；
只能用于触发 diagnostics 或 protocol coverage。
```

这和 collector 当前解析语义一致：`generated/modified/merge/historical` 可以返回 target subname；其他 relation 会返回 terminal diagnostic。

---

# 推荐的 validator 实现结构

```text
cad-core/tools/validate_freecad_fixture_inputs.py
```

结构：

```python
def validate_fixture_file(path: Path) -> list[str]:
    fixture = load_json(path)
    errors = []

    errors += validate_top_level_contract(path, fixture)
    errors += validate_object_graph(path, fixture)
    errors += validate_recompute_targets(path, fixture)
    errors += validate_fixture_mode_policy(path, fixture)

    if "topoNamingState" in fixture:
        errors += validate_input_topo_state_schema(path, fixture)
        errors += validate_input_document_hash(path, fixture)
        errors += validate_input_topo_state_objects(path, fixture)
        errors += validate_stable_sublist_resolution(path, fixture)
        errors += validate_reference_shadow_alignment(path, fixture)

    return errors
```

CLI：

```bash
python3 tools/validate_freecad_fixture_inputs.py --phase c4m6 --strict
python3 tools/validate_freecad_fixture_inputs.py --all --strict
```

可选加：

```bash
python3 tools/validate_freecad_fixture_inputs.py --phase c4m6 --strict --freecadcmd-dry-run
```

`--freecadcmd-dry-run` 做：

```text
对每个 fixtures/<phase>/*.json 调 collect_freecad_expected.py --out /tmp/xxx.freecad.json
```

这一步证明：

```text
输入 fixture 不只是静态结构合法；
它确实能被 FreeCADCmd collector 消费。
```

---

# 和 expected validator 的关系

最终建议建立这条证明链：

```text
1. validate_freecad_fixture_inputs.py
   证明 fixtures/<phase>/*.json 是合法根输入，前态账本闭合，StableSubList 可解析。

2. collect_freecad_expected.py --phase <phase> --check
   证明 checked-in expected 可以由当前 FreeCADCmd collector 复现。

3. validate_freecad_expected_ledger.py
   证明 fixtures/<phase>/expected/*.freecad.json 输出账本闭合完整。
```

CI 可以这样：

```bash
cd cad-core

python3 tools/validate_freecad_fixture_inputs.py \
  --phase c4m6 \
  --strict

python3 tools/collect_freecad_expected.py \
  --phase c4m6 \
  --check \
  --skip-unsupported

python3 tools/validate_freecad_expected_ledger.py \
  --phase c4m6 \
  --strict
```

这样权威链非常清楚：

```text
合法根输入
  -> FreeCADCmd 生成/复现 expected
  -> expected 输出账本自洽闭合
  -> expected 可以作为 cad-core 后续 parity 的权威 oracle
```

---

# 最关键的设计原则

`fixtures/<phase>/*.json` 的 `topoNamingState` 应该叫“输入前态账本”，不是“expected 输出账本”。

因此它允许是最小集合：

```text
只包含 StableSubList 解析需要的对象和 token。
```

但它不允许断链：

```text
凡是 fixture 里声明要从 topoNamingState 解析的 StableSubList，
都必须能在输入 topoNamingState 里解释清楚；
要么解析成 indexed subname，
要么明确落到允许的 terminal diagnostic。
```

这就是根输入 fixture 的完整性标准。
