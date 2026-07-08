# 【已实现】C13-M1 S1 FreeCAD 与 collector 输出合同复核

## 目标

明确 `topoNamingState` 中哪些字段来自 FreeCAD 语义，哪些只是 collector expected schema，避免 runtime 代码照 fixture 字符串反推。

## live 基线

- `pwd`: `/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`: `41b56b0dda`
- `git log -1 --oneline`: `41b56b0dda 文档：关闭 C13-M1 S0 基线冻结`
- `git -c core.quotepath=false status --short -uall`: 无输出，S1 开始前工作区干净。
- `step_goal_queue.py ... --format markdown`: S1 开始前第一项为 `7-8-17-56-C13-M1-S1-FreeCAD与collector输出合同复核.md`。

## FreeCAD 语义 authority

### ElementMap 是稳定子元素身份账本

- `src/App/ElementMap.cpp::ElementMap::setElementName()` 通过 `addName(mappedName, element, ...)` 建立 `MappedName -> IndexedName` 和反向 `mappedRef(idx)` 关系；重复 mapped name 会进入 `renameDuplicateElement()`，不是简单保存当前 `FaceN` 字符串。
- `ElementMap::find(const MappedName&)` 和 `ElementMap::find(const IndexedName&)` 支持双向查找，并会递归进入 `child.elementMap`、叠加 `child.postfix`。这说明 child map 是身份账本的一部分，不是 response 展示路径。
- `ElementMap::hashChildMaps()`、`addChildElements()`、`getAll()` 共同说明 child element map key 和展开条目来自 ElementMap 内部账本。C13-M1 不能用 fixture 里的 `childElementMapKey` 字符串反推实现；精确 parity 留给后续批次。

### TopoShape 命名传播来自 makeShapeWithElementMap 和 mapper history

- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap()` 先 `setShape(shape)`，`ElementMapPolicy::Drop` 时直接 `dropElementNaming()`；正常路径先 `mapSubElement(shapes)`，再消费 mapper 的 `modified()` / `generated()` history。
- 该函数在生成、修改、上下层元素推导时调用 `ensureElementMap()->encodeElementName(...)` 与 `elementMap()->setElementName(...)`。所以 `NamedShape.elementMap` / mapper history 才是 runtime 发布 topo state 的来源。
- `src/Mod/Part/App/TopoShapeMapper.cpp::ShapeMapper::insert()` 把 generated / modified 互斥记录；`GenericShapeMapper::init()` 通过源/目标形状关系生成 history。C13-M1 发布器只能消费已有 `NamedShape` / `MapperHistory` 证据，不能在 adapter 或 fixture 层重新猜 ownership。

### PropertyLinks 通过 ElementMap 更新旧引用

- `src/App/PropertyLinks.cpp::PropertyLinkBase::_updateElementReference()` 调 `GeoFeature::resolveElement(..., ElementNameType::Export, ...)` 得到 `elementName.oldName/newName`，再更新 `ShadowSub` 与实际 subname。
- 缺失元素时它会尝试 `geo->searchElementCache(oldElement)` 做几何恢复；`PropertyLinkSub` / `PropertyLinkSubList` 再把 `_mapped` 中的旧引用替换成 `shadow.newName`。这条链路说明 persisted topo state 的意义是让下一次请求可消费旧 stable token，而不是让 response 复制 expected 字符串。

### Body display Shape 与 Tip child path 必须分离

- `src/Mod/PartDesign/App/Body.cpp::Body::execute()` 读取 `Tip.getValue()`，取 Tip feature 的 `Shape`，执行 `tipShape.transformShape(...)` 后写入 `Body.Shape`。
- `Body::getSubObject()` 对带 `.` 的非 mapped child path 走 `Part::BodyBase::getSubObject()`，否则可返回 Body 自身。C13-M1 response 的 display `subname/fullSubname/stableSubname` 与 topo state object payload 不能混用；Body/Tip alias 仍要由 `NamedShape` / `ElementMap` 证明。

## collector 合同边界

- `cad-core/tools/collect_freecad_expected.py::topo_naming_state_response()` 定义 expected 顶层 schema：`schemaVersion`、`producer`、`documentHash`、`objects`。
- `topo_state_object_payload()` 定义 object payload schema：`objectHash`、`elementMapVersion`、`subshapes`、`elementMap.encoding/status/entries`、`childElementMaps`、`mapperHistory`。
- `semantic_hash()` 和 `comparable_topo_naming_state()` 是 schema/comparison oracle；版本通配、hash 编码和 canonical mapped-name 归一化不能当成 FreeCAD runtime 业务语义。
- `topo_state_element_map_entry()` 会从 expected subshape 的 `rawFreecadMappedName` / stable token 组装 `mappedName.raw/canonical`。这只是 collector expected 形态；S3 不得照 fixture 字符串合成 FreeCAD mapped name。

## cad-core 当前可消费落点

- `cad-core/src/part/topo_shape.cpp::namedShapeToJson()` 已把 `NamedShape` 投影成 `element_map_status`、`element_map`、`child_element_maps`、`history`、`mapper_history`。
- `mapperHistoryForNamedShape()` 会把 legacy history 和 `elementMap` alias 转为 mapper events；`cad-core/src/part/topo_shape_mapper.cpp::mapperHistoryToJson()` 负责稳定 JSON 投影。
- C13-M1 runtime publisher 的实现方向应是消费 `ComputeContext.namedShapes`、当前 response subshapes 和 document hash 输入，发布 `topoNamingState`；不是复制 expected 中的 `Pocket.#...:H...`、`childElementMapKey` 或 fixture 顺序。

## 字段分类

| 分类 | 字段 / 规则 | S1 结论 |
| --- | --- | --- |
| FreeCAD authority | `ElementMap` entries、child element maps、mapper generated/modified history、PropertyLinks reference update、Body Tip shape/display 分离 | 已冻结为实现依据；S3/S4 必须从 `NamedShape` / `ElementMap` / mapper history 消费。 |
| collector schema | `schemaVersion`、`producer`、`documentHash`、`objectHash`、object payload keys、comparison canonicalization | 作为 expected schema 和 comparator 依据；hash parity 可记录 gap，不阻塞发布闭环。 |
| C13-M1 required_now | 顶层 `topoNamingState`、object `subshapes`、`elementMap.encoding/status/entries`、`childElementMaps` 数组、`mapperHistory` 数组 | 必须由 runtime official response 发布。 |
| gap_allowed | `documentHash` / `objectHash` 与 collector `semantic_hash()` 的字节级一致性 | 可先记录 `hash_encoding_gap`，不得用 fixture 特判修。 |
| followup_mapped_name | FreeCAD raw `#...:H...` mappedName、canonical mapped-name、`childElementMapKey` 精确 parity | 后续批次处理；C13-M1 不伪造。 |

## 非目标确认

- FreeCAD mapped-name 字节级 encoder、全量 expected fixture parity、frontend consumer 同步、persistent backend topology cache 仍为 active non-goal。
- C13-M1 不改 `subname/fullSubname/stableSubname` 既有 response 语义。
- 不新增“复制 expected mappedName”或按 fixture 字符串反推实现的路线。

## 关闭结果

- `c13m1_topo_state_source_matrix.tsv` 已把 FreeCAD authority 与 collector schema 分离为 `authority_frozen` / `reviewed`。
- `c13m1_topo_state_contract_matrix.tsv` 已标出 `required_now`、`gap_allowed`、`followup_mapped_name`、`followup_child_map_key`。
- `c13m1_topo_state_non_goal_registry.tsv` 确认 mapped-name、full parity、frontend、persistent cache 等仍 active。
- `C13M1-BLOCKER-201` 已关闭，后续进入 S2 red tests。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n "makeShapeWithElementMap|_updateElementReference|topo_naming_state_response|topo_state_object_payload|mapperHistoryToJson" src/App src/Mod/Part/App src/Mod/PartDesign/App cad-core/tools cad-core/src/part
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/矩阵/*.tsv
git diff --check
```
