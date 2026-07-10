# FreeCAD Expected Release Gate 深模块重构方案

## 来源与结论

当前 `cad-core/tools/compare_freecad_expected.py` 同时承担 fixture discovery、JSON 规范化、事实 diff、差异分类、协议豁免、report 汇总、文件写入和 CLI 退出码。`cad-core/tests/test_topo_naming_state_response.py` 又维护了另一套只比较部分字段的 expected helper，`regenerate_cad_core_res.py` 则重复维护 discovery 和 CAD Core 运行入口。

这组实现的主要问题不是函数数量，而是没有一个稳定 seam 能回答同一个问题：**当前 CAD Core live 输出与 FreeCADCmd public expected 在事实、语义和发布三个层次分别是什么状态？** 当前 raw diff、policy decision 和 release verdict 混在一个浅 module 中，导致：

- strict report 有 diff 就是 `red`，即使全部差异已经登记为 transport protocol divergence。
- strict CLI 即使 `red` 仍返回 0，0 case discovery 还可能得到假 green。
- c4m6 的 hash case、Link result 和整个 `results.subshapes` 类别存在过宽接受路径，可能掩盖新回归。
- focused runtime test 没有完整比较 object set、hash、完整 `elementMap`、`childElementMaps`、`mapperHistory` 和 `ReferenceShadow`。
- checked-in `cad-core-res` 可以通过 snapshot 测试，但不能证明它来自当前 binary。

本方案新增一个通用的 `FreeCADExpectedParity` 深模块，把比较事实、精确 policy、live source 和 release verdict 集中到一个小 interface 后面。`c4m6` 是首个最小完整语义批次，不进入长期 module 名称；本轮不扩到 C13-M5 S4 的五个 phase family。

## 当前 live 基线

当前 c4m6 已用 clean rebuild 后的 `build/cad-core` 重新生成 `cad-core-res`。现有 strict report 的稳定结果是：

- 9 个 native expected case。
- 2 个 exact green、7 个 exact red。
- 共 8 个 actual-only diff。
- 5 个 `results.*.mesh`、1 个 `results.ProbeSketch.subshapes`、整个 `results.CompoundLink`、整个 `results.HistoryProbe`。
- `diagnostics`、`geometry.numeric`、`topoNamingState.objects/subshapes/elementMap/childElementMaps/mapperHistory` 当前均为 0 diff。

这 8 项只是迁移前 characterization，不是最终永久白名单。hash policy、CompoundLink oracle coverage 和 HistoryProbe provenance 修正后，最终 registry 必须从新的 live report 重新冻结。

当前三个 gate 的实际证明边界不同：

- `validate_freecad_expected_ledger.py --phase c4m6 --strict` 证明 checked-in expected 与同名 ledger 的 hash、projection、coverage、round-trip 闭包，不运行 CAD Core。
- `tests.test_topo_naming_state_response` 运行当前 binary，但其 expected helper 只比较 public DTO 的部分字段。
- `compare_freecad_expected.py --strict` 比较完整 saved output，但只读 checked-in `cad-core-res`，而且 report `red` 不影响 shell exit code。

重构后必须保留这三类证据的职责差异，不能再把任一单独入口称为完整 release gate。

## 必须先解决的权威冲突

当前仓库存在一个 release gate 不能自行裁决的合同冲突：

- `AGENTS.md` 当前规则要求 schema、producer、document/object hash、element-map encoding 和对象归属校验失败时请求级 hard fail，不继续 recompute，不返回新 state。
- `cad-core/src/runtime/topo_naming_state.cpp::shouldHardFailHashMismatch()` 当前固定返回 `false`，把 document/object hash mismatch 当作 stale-state signal 后继续 recompute。
- `tests/test_topo_naming_state_response.py::test_c4m6_document_and_object_hash_mismatch_recompute_with_topo_state()` 锁定了继续 recompute。
- 当前两个 `.freecad.json` expected 也记录正常 result 和新 state。
- C13-M4 矩阵仍写 hard fail，C13-M5 S3 文档则写继续 recompute。

本方案默认以当前 `AGENTS.md` 为 CAD Core protocol authority：document/object hash mismatch 与 foreign object ownership 同属请求级 hard fail。实施时必须形成以下闭环：

1. collector 在 public protocol 投影阶段生成 rejected expected 与 rejected ledger，不伪造 result、update 或新 state。
2. runtime 真正执行 hard fail，不再保留恒 `false` fallback。
3. focused test 精确断言 diagnostics-only 响应。
4. C13-M5 中“hash mismatch 继续 recompute”的状态全部撤销。
5. 若后续要改成 stale-state recompute，必须先修改 `AGENTS.md` 和 `docs/接口规定` 的正式合同；comparator registry 无权替代该决策。

同一请求校验批次还要补一个 unknown / foreign topo-state object ownership case。只修两个 hash fixture 而继续静默跳过 graph 中不存在的 state object，不满足最小完整语义批次。

## FreeCAD 权威调用链与 cad-core 分层映射

本轮不从现有 `cad-core-res` 反推 FreeCAD 语义。hash/ownership 是 CAD Core 对不可信客户端状态的协议选择，不伪装成 FreeCAD 原生行为；Link、MapperHistory 和 ReferenceShadow 的几何/引用语义才回到 FreeCAD 源码取证。

| lane | FreeCAD / protocol authority | 关键语义与调用顺序 | cad-core 落点 |
| --- | --- | --- | --- |
| hash / ownership request validation | `/Users/li/Chili3DProject/FreeCAD/AGENTS.md::拓扑命名与引用状态纪律`；`docs/接口规定/7-8-11-08-topoNamingState客户端携带状态接口方案.md` | 客户端 state 先于 recompute 校验；schema、producer、hash、encoding、top-level state object ownership 任一失败都不得进入建模 | `cad-core/src/runtime/topo_naming_state.cpp::topoNamingStateRequestFailureJson()`；`cad-core/src/runtime/recompute.cpp` 的请求级 early return |
| CompoundLink / child map | `/Users/li/Chili3DProject/FreeCAD/src/App/Link.cpp::LinkBaseExtension::extensionGetSubObject()` / `getTrueLinkedObject()`；`/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::createChildMap()` | `linked->getSubObject(...)` 后调用 `checkGeoElementMap(...)`；child map 保存 `child.elementMap = topoShape.elementMap()`，再投影 Link target 与 child-local identity | `cad-core/src/app/link.cpp`；`cad-core/src/part/topo_shape*.cpp`；`cad-core/src/runtime/topo_naming_state.cpp`；collector `object_expected_payload()` / `app_link_payload()` |
| MapperHistory | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp::ShapeMapper::populate()` / `insert()` | mapper 展开 destination shapes，并执行“Prevent an element shape from being both generated and modified”；public DTO 只投影可解释的 generated/modified/split/deleted/ambiguous evidence | `cad-core/src/part/topo_shape.cpp` 与 mapper/history 账本；`cad-core/src/runtime/topo_naming_state.cpp` 只做 public projection |
| ReferenceShadow / element update | `/Users/li/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp::PropertyLinkBase::_updateElementReference()` | `GeoFeature::resolveElement(...)` 先解析当前对象；missing/reverse 时再用 `searchElementCache()` 恢复，最后更新 item-local shadow evidence | `cad-core/src/runtime/reference_resolution.cpp`、`reference_lifecycle.cpp`、`element_reference_update.cpp`；`topo_naming_state.cpp` 不消费 BREP 建模 |

这里的模块归属必须保持清楚：`NamedShape`、MapperHistory 和 OCCT shape ledger 属于 `part/`；`topo/` 只承接跨模块 identity codec / stable identity，不用“topo/part ledgers”这类模糊落点混写。

完整调用链如下：

```text
FreeCAD src/App/PropertyLinks.cpp、ElementMap.cpp
  + src/Mod/Part/App/TopoShapeExpansion.cpp
        ↓ FreeCADCmd / collector public projection
collect_freecad_expected.py
        ↓ 同次生成并绑定
expected/<case>.freecad.json + <case>.freecad.ledger.json

DocumentObject graph + client-carried topoNamingState
        ↓ request validation
runtime/topo_naming_state.cpp
        ↓ request-local recompute
runtime/recompute.cpp + app/link.cpp + part/NamedShape/MapperHistory ledgers
        ↓ full frontend response
cad-core-res/<case>.cad-core.json

native expected + ledger + live CAD Core result
        ↓
FreeCADExpectedParity.evaluate(...)
        ↓
exact evidence + semantic verdict + release verdict
```

分层落点：

- `collect_freecad_expected.py`：只负责 FreeCADCmd/native public expected 与 ledger provenance；不消费 cad-core output。
- `validate_freecad_expected_ledger.py`：只验证 expected/ledger 闭包；不成为 runtime comparator。
- `runtime/topo_naming_state.cpp`：负责请求级 schema/producer/hash/encoding/ownership 合同和 public topo state 发布。
- `runtime/recompute.cpp`、`app/link.cpp`：继续发布前端需要的 mesh、subshapes 和 Link result；不得为了 expected 形状在输出端修剪。
- `tools/freecad_expected_parity/`：负责比较、policy 和 release gate，不承载建模业务语义。
- `compare_freecad_expected.py`、`regenerate_cad_core_res.py`：作为 CLI adapter，只解析参数、选择 source、输出 JSON 和设置退出码。

Expected/ledger 的既有来源与工具边界继续以 `docs/工具规定/7-9-16-55-FreeCADCmdExpectedLedger工具规定.md` 为准，本方案不重复定义 ledger v1。

## 目标深模块

新增内部 package：

```text
cad-core/tools/freecad_expected_parity/
  __init__.py
  model.py
  engine.py
  catalog.py
  sources.py
  registry.py
  protocol_divergences.v1.json
  fixture_roles.v1.json
```

外部 interface 只暴露两个入口及 request/report 类型：

```python
report = evaluate(request: EvaluationRequest) -> ParityReport

generation = materialize_current(request: MaterializeRequest) -> GenerationReport
```

职责：

- `evaluate()` 不做仓库持久写入；live 模式可以使用临时目录，但同一份 actual payload 必须完成 normalized exact diff、registry audit、semantic verdict 和 release verdict。
- `materialize_current()` 是唯一允许写 `cad-core-res` 的入口；先执行 live binary、校验 JSON，再原子替换目标文件。
- request 只声明 `snapshot / live / in_memory` source kind 及必要参数；内部 `ActualSource` port 由 `SnapshotActualSource`、`LiveCadCoreSource` 和测试使用的 `InMemoryActualSource` 三个 adapter 实现，具体 adapter 类型不进入外部 interface。
- `fixture_roles.v1.json` 是 native/protocol-only/unsupported discovery 的单一机器来源，collector、catalog、generator 和 tests 不再各自猜 suffix 或 case 名。
- CLI、unittest 和后续 CI 都跨同一个 interface，不再各自维护字段比较规则。
- module 内部可以拆 model、catalog、source 和 registry，但这些内部 seam 不泄漏给 CLI 调用者。

删除该 module 时，discovery、canonicalization、diff、registry audit、live freshness、report schema 和退出语义会重新散落到多个脚本和测试中；因此它通过 deletion test，具有足够 depth。

## 核心模型与状态

建议外部 report 只暴露聚合状态和证据摘要；`FixtureCase`、`DiffKey`、`ParityDiff`、registry model 都保持为 module 内部类型。report 证据分成运行级与 case 级：

```text
RunEvidence
  sourceKind, binarySha256, comparisonProfile,
  comparisonProfileSha256, registrySha256, fixtureRolesSha256

ArtifactEvidence (per case)
  inputSha256, expectedSha256, ledgerSha256,
  actualRawSha256, currentRawSha256,
  actualNormalizedSha256, currentNormalizedSha256, currentFresh

ParityReport
  schemaVersion, selection, runEvidence,
  exactStatus, semanticStatus, releaseStatus, releaseGatePassed,
  summary, cases[*].artifactEvidence, registryAudit, preflight
```

状态必须分开：

```text
exactStatus:    green | red | not_evaluated
semanticStatus: green | red | not_evaluated
releaseStatus:  green | protocol_divergence |
                red | invalid | not_evaluated
```

语义：

- `exactStatus` 是 versioned comparison profile 后的 normalized exact，不是字节相等。profile 必须显式记录 raw `:H...` canonicalization、keyed-list 对齐、数值容差和顶层 envelope projection。
- `semanticStatus` 只有在所有 exact diff 都精确命中已批准 protocol registry，且 actual contract 仍成立时才可 green。
- `releaseStatus=green` 只表示没有 diff。
- `releaseStatus=protocol_divergence` 表示只有已批准、可解释、受独立合同保护的 protocol diff；不得伪装成 exact green。
- 0 case、ledger 缺失、registry stale/duplicate、runner 失败、非法 JSON 或 authority conflict 都是 `invalid`。
- snapshot source 最多产生 exact/semantic 诊断，release 必须是 `not_evaluated`；只有 live source 且 `cad-core-res` digest 与 live result 一致时才可计算正式 release verdict。
- phase 聚合优先级固定为 `invalid > red > protocol_divergence > green`；比较没有完成时 exact/semantic 为 `not_evaluated`。
- `known_gap`、`unsupported` 继续是 C13-M5 phase matrix 的外部 disposition，本轮 engine 不凭 classifier 文本生成它们；未来若要进入 engine，必须先新增独立、机器可验证的 manifest。
- `releaseGatePassed` 只表示 parity module 自身通过；它不替代 registry 引用的 adapter/behavior tests、FreeCADCmd reproducibility 或 C13-M5 S5 的组合验收。
- raw digest 只保留 provenance；`currentFresh` 必须比较同一 `comparisonProfile` 规范化后的 actual/current digest，避免允许的 `:H...`、keyed-list 顺序或数值容差漂移制造假 stale。report 同时记录 normalized digest 与 profile hash。

## 精确 protocol divergence registry

`protocol_divergences.v1.json` 是机器可读的批准清单，不再在 Python 中使用 case-wide/category-wide `if`。每项至少包含：

```json
{
  "id": "C13M5-C4M6-TRANSPORT-001",
  "selector": {
    "phase": "c4m6",
    "case": "topo-state-body-tip-stable-recovery",
    "category": "results",
    "kind": "extra",
    "path": "results.Body.mesh"
  },
  "actualContract": {
    "type": "object",
    "keysMode": "exact",
    "requiredKeys": [
      "vertices",
      "normals",
      "indices",
      "faceIds",
      "edgeSegments",
      "vertexPoints"
    ]
  },
  "nativeExpected": "native summary does not publish display mesh",
  "cadCoreProtocol": "full result retains frontend display mesh",
  "frontendImpact": "removing it breaks display/picking consumers",
  "authority": "docs/CADCore方案/00-CAD-Core抽取方案.md",
  "contractTests": [
    "tests.test_adapters.CadCoreAdapterTest.test_c_api_mesh_edge_segments_reference_result_subshapes"
  ],
  "removeWhen": "native expected adopts the same transport contract"
}
```

不变量：

- selector 必须精确到 `phase + case + category + kind + path`，禁止 glob、regex、`endswith()` 和整 case/category 放行。
- 一个 diff 必须恰好匹配 0 或 1 项；多项匹配是 registry invalid。
- 选中 scope 内每个 registry entry 必须恰好被消费一次；未消费 entry 是 stale registry，不能悄悄保留。
- `kind` 由 `extra` 变成 `missing`、`value` 或 `type` 时自动阻断。
- whole-result extra 必须使用 exact/nested actual contract；field-level transport 可按证据使用 required contract。type/key/value contract mismatch 是被成功检测到的 runtime regression，必须得到 semantic/release `red`，不是配置 `invalid`。
- `contractTests` 是可追溯的精确 dotted test id 列表；parity module 校验其格式和非空性，但不宣称已经执行这些 unittest。完整 CI release closure 仍需显式运行对应 focused tests。
- classifier 只能补 owner、authority、known-gap 和 close condition，不能决定 acceptance。

当前 8 个 diff 只用于 characterization。完成下述 oracle/protocol 修正后，只把重新生成报告中仍满足“expected 已有同名 semantic result、actual 仅多前端 transport”的项写入正式 registry。

## Oracle 与协议边界修正

### Hash 与对象归属 hard fail

同一批次覆盖：

- schema incompatible。
- producer incompatible。
- documentHash mismatch。
- objectHash mismatch。
- object element-map encoding incompatible。
- child element-map encoding incompatible。
- unknown / foreign topo-state object ownership。

hard-fail 响应必须满足：

- `results=[]`。
- `elementReferenceUpdates=[]`。
- top-level key 集合严格保持为 `diagnostics`、`elementReferenceUpdates`、`results`；不发布 `documentObjectUpdates`、`binaryPayloads` 或新 `topoNamingState`。
- diagnostics 的 `code`、`severity`、`source`、`message` 及 case-specific actual/expected evidence 可精确比较；本轮不为 hard fail 额外发明 `stage` 字段。

ownership 的拒绝条件固定为：`topoNamingState.objects` 的 top-level object key 不存在于当前请求的 `DocumentObject graph`，diagnostic code 使用 `topo_state_object_owner_incompatible` 并记录 offending object。PropertyXLink 指向 graph 内其它对象、Link child path 或合法的 `SubList=[] + StableSubList-only` 恢复不属于 foreign ownership，不能误杀。

expected 和 ledger 都由 collector 同次生成；不得手改 JSON。

### CompoundLink native result coverage

当前 child-map collector 通过 `topo_state_protocol_response()` 固定返回 `results=[]`，因此整个 `results.CompoundLink` 被标成 transport divergence，但其中还包含 bbox、volume、topology counts 等语义字段，现有 expected 无法证明这些字段 parity。

应让 collector 的 protocol response 接受 native target summary，并对真实 `App::Link` 调用现有 object/shape summary 路径：

- expected 发布 `CompoundLink` 的 native semantic result。
- strict comparison 验证 bbox、volume、topology 和 subshape identity。
- registry 最多保留 actual-only mesh 等明确 transport 字段，不再允许整个 result object。
- 如果 FreeCAD Python 对该 subelement Link 无法取得 Shape/summary，必须归类为 collector unsupported / 需 native probe；不得恢复 whole-result registry，也不得用当前 cad-core result 补 expected。

### HistoryProbe provenance

`CadCore::TopoNamingStateProbe` 是 CAD Core synthetic DTO probe；FreeCAD Python 不暴露所需 producer history，CAD Core 实现还构造了辅助 Box。它没有 native geometry authority，不能继续把 `results.HistoryProbe` 永久登记为 native intentional divergence。

应：

- 把 mapperHistory DTO 合同迁到同名 `*.expeted.json` 和 focused protocol test。
- 新增机器可读的 fixture-role manifest，将该 case 标记为 `protocol_only / native_oracle_blocked`；collector `fixture_paths()`、expected path 选择、native compare discovery 和统计都消费同一角色，phase collect/check 必须明确报告 skipped reason。
- 迁移步骤在 role 生效后删除旧 collector-owned `.freecad.json` / `.freecad.ledger.json` pair，不能只改名或遗留，focused protocol test 精确读取 `.expeted.json`。
- 保留“待 native C++ probe/exporter”known gap、authority 和删除条件，但放在独立 provenance registry / 文档中，不参与已经退出 native discovery 的 c4m6 release verdict。
- 不用 CAD Core 辅助 Box 反向补 FreeCAD expected。

fixture-role manifest 本身必须 fail closed：

- 选中 phase 的每个 input 恰好有一个 role，禁止 duplicate、orphan 和 stale entry。
- `native` 必须同时存在 `.freecad.json` 与 `.freecad.ledger.json`。
- `protocol_only` 必须存在 `.expeted.json`，且旧 native expected/ledger pair 不得残留。
- `unsupported` 必须有 reason、authority、next action 和 close condition。
- role audit 或 `fixtureRolesSha256` 缺失时 release 为 `invalid`；0 case 检查不能替代 role 完整性审计。

### ReferenceShadow 完整 gate

当前 focused test 只断言 `StableSubList`、`ShadowSub.newName` 和 `ReferenceShadow.stableSubname`。应改为：

- 完整比较 `elementReferenceUpdates` subtree。
- 对存在的 `SubList`、`StableSubList`、`FullSubList`、`ShadowSub`、`ReferenceShadow` 校验 item-local current/recovery cardinality；允许协议明确支持的 `SubList=[] + StableSubList-only` 恢复，不能机械要求所有数组与空 SubList 等长。
- 锁定 `brep`、fingerprint、target/targetId、property、shapeType、indexed、subname、stableSubname。
- 递归断言 `brep` 只允许位于 `ReferenceShadow` 单个旧 subshape snapshot，不进入 `topoNamingState`、results 或完整对象状态。
- c4m6 的 `BREP:single-face-snapshot-only` 只负责完整 update/path/no-leak 合同，不把占位文本冒充可解码几何证据。
- 复用 `tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_reference_shadow_brep_uses_shared_vertex_geometry_not_bbox_fingerprint` 或新增一个有效 BREP focused case：仅当两份语义等价 evidence 都解析到同一当前 subshape 时，断言当前 graph 生成的 shape 相同，并证明没有直接复用 snapshot 建模。

## 最小完整实施批次

### S0：冻结 seam 与 authority

- 保存当前 c4m6 live strict report 作为迁移前 characterization，不把 `out/` 纳入 baseline。
- 记录当前 8 个 diff，但不写入最终 registry。
- 以当前 `AGENTS.md` 冻结 hash/encoding/ownership hard-fail 合同。
- 为 hash、unknown ownership、ReferenceShadow、CompoundLink 和 HistoryProbe 写临时 targeted characterization / mutation probe，记录能捕获目标问题的红灯；永久 regression test 与对应 S2a/S2b/S2c 实现原子落地，S0 不向默认 suite 提交永久 failing test。
- 本步不改 runtime 输出语义。

### S1：机械抽取深模块

- 新建 `tools/freecad_expected_parity/` model、catalog 和纯 `evaluate()`。
- 把现有 canonicalization、keyed-list diff、numeric tolerance、category 归类机械迁入。
- 报告升级为 `cad-core.freecad-expected-parity.v2`，先保持现有 exact diff 行为。
- `compare_freecad_expected.py` 暂时通过 compatibility shim 调用新 module。
- 抽出唯一 `FixtureCatalog`，删除 compare/regenerate 两份 discovery 规则。
- 同步定义内部 `ActualSource` port，并实现 `SnapshotActualSource` / `InMemoryActualSource`；S1 的 `evaluate()` 已可用于 snapshot characterization 和 mutation tests，S4 只增加 live adapter。
- 保持 `c3m1`、`c10m1`、`c12m12`、`c3m5`、`c3m6` 五个 S4 representative phase 的 snapshot discovery、family classification 和 known-gap metadata 不退化；本轮不修其 parity，但共享工具重构必须通过现有 family regression。

本步只迁移 ownership，不同时修改 hash 或 oracle 行为，便于验证 module seam。

### S2：请求校验与 oracle provenance 归位

S2 是一个 milestone，但按三条不同 authority/call chain 拆成可回滚子步骤；每个子步骤自己的 fixture、expected/ledger、实现和 focused test 必须闭合后才进入下一步：

- **S2a request hard fail**：runtime 恢复 document/object hash hard fail，增加 unknown/foreign object ownership hard fail；collector 生成对应 rejected expected + rejected ledger，再用 FreeCADCmd 成对重采；focused test 锁定 exact diagnostics-only envelope。
- **S2b CompoundLink oracle**：collector 为真实 App::Link 采集 native semantic result；若 Python coverage 不足则明确 unsupported/native probe，不允许 whole-result fallback。
- **S2c HistoryProbe provenance**：引入 fixture-role manifest，迁移到 `.expeted.json`，删除旧 native expected/ledger pair，focused protocol test 和 skipped 统计闭合。

ReferenceShadow collector/expected 继续保持原生证据，不手工补字段；其完整 live gate 在 S4 收口。

这是同一 public DTO/request validation 边界的最小完整语义批次，不拆成单 fixture 长期推进。

### S3：精确 policy 与三层 verdict

- 引入 `protocol_divergences.v1.json` 和 registry validator。
- 从 S2 后的新 live report 生成候选，再人工核对 authority、frontend impact、contract test 和 remove condition。
- 删除 `HASH_MISMATCH_CASES`、`LINK_COMPOUND_CASES`、`.endswith(".mesh")` 和整个 `results.subshapes` 放行逻辑。
- 输出 exact/semantic/release 三层状态、accepted/unaccepted count、registry audit 和 preflight。
- protocol divergence 与 green 保持不同状态；known-gap/unsupported 只由外部 phase disposition registry/矩阵表达，不由 classifier 自动生成。

### S4：live source 与完整 runtime gate

- 实现 `LiveCadCoreSource` 和 current freshness；沿用 S1 已有 Snapshot/InMemory adapter。
- live source 在临时目录运行当前 binary，一次执行产生的同一 payload同时用于 diff 和 source evidence。
- ledger preflight 必须调用/复用 strict validator 的真实规则，不能只检查 sidecar 是否存在。
- 比较完整 object set、producer/documentHash/objectHash、subshape payload、完整 elementMap、child maps、mapper history、updates 和 diagnostics。
- `tests.test_topo_naming_state_response` 改用同一个 `evaluate()` seam；保留 round-trip 和 hard-fail 等领域行为断言，删除旧的部分字段 comparator。
- snapshot green 但 live red、binary 缺失、runner 非零、非法 JSON、0 case 都必须阻断。
- parity report 的 `contractTests` 只提供可追溯证据；完整 release closure 仍要求 CI/验收命令实际运行这些 focused tests。

### S5：CLI adapter、生成入口与 deletion test

- `compare_freecad_expected.py` 只保留参数解析、source 选择、报告写出和退出码。
- 保留 `--strict` 作为 report-only 兼容入口：valid red report 仍返回 0，但 0 case / infra invalid 返回非零；后续可新增 `--report` alias。
- 新增 `--live` 与 `--release-gate`；`--release-gate` 自动蕴含 live execution、ledger preflight 和 current freshness，不再要求同时传 `--live`，release `red/invalid/not_evaluated` 返回非零。
- 新增 `--run-contract-tests` 组合入口：从选中 scope 的 registry 精确枚举 dotted unittest ids，验证可加载后用一次 `python3 -m unittest` 执行；缺失、无法加载或失败都返回非零。parity verdict 与 consumer contract tests 仍分别记录，不把 subprocess 结果伪装成 diff。
- `regenerate_cad_core_res.py` 改用 `materialize_current()`，先 JSON 验证后 atomic replace。
- `--write-current` 0 case 返回非零；binary 使用 raw-byte SHA-256，JSON artifact 同时记录 canonical raw digest 与 comparison-profile normalized digest。
- checked-in current 与 live 不一致时是 evidence `invalid`，不是含糊的 stale/red。
- 删除旧 partial comparator、重复 discovery、case-wide classifier 和不再使用的 compatibility shim。

Deletion test：删除两个 CLI adapter 后，module tests 仍能通过 public interface 完成 live/snapshot evaluation；删除旧 c4m6 Python 特判后，registry 仍完整表达所有批准项。

### S6：C13-M5 S5 release gate 与文档收口

- 更新 `docs/接口规定/7-8-11-08-topoNamingState客户端携带状态接口方案.md`，补全 hash/encoding/ownership hard-fail 响应边界。
- 保留 C13-M4 hash hard-fail authority，刷新验证状态；把 ReferenceShadow 的“evidence-only”结论改由完整 gate 证明。
- 撤销 C13-M5 方案、README、S3 和矩阵中的 hash-recompute 结论。
- 在 C13-M5 S5 phase matrix 中登记 `green / protocol_divergence / known_gap / unsupported / red`。
- 更新或新增 comparator/release-gate 工具规定，固定 report v2、0-case、registry 和 exit-code 合同。
- 只有 expected、ledger、live result、checked-in current、report、focused tests 和 registry 全部闭合后，才把本方案重命名为 `7-10-06-25-【已实现】FreeCADExpectedReleaseGate深模块重构方案.md`。

## 优先负向测试

- hash mismatch 新增 result/topo state 时必须失败，不能因 case 名被接受。
- state 包含 graph 不存在的 object 时必须请求级 hard fail。
- registry selector 的 `kind` 从 `extra` 变为 `missing/value/type` 时必须 red。
- 新增未登记 mesh path 必须 red；不能因 `.mesh` 后缀自动放行。
- whole-result extra 的 type、key 或 nested contract 漂移必须 semantic/release red。
- duplicate、ambiguous、stale registry entry 必须 invalid。
- fixture role 缺失、重复、orphan、role/artifact 不一致或 stale entry 必须 invalid，不能通过少发现 case 假绿。
- 0 case discovery 必须 invalid，不能假 green。
- live runner 缺失、非零退出或非法 JSON 不得回退 snapshot。
- live 与 checked-in `cad-core-res` digest 不同必须 evidence invalid。
- raw mapped-name hash 漂移可 normalized exact green；stableSubname 漂移必须 semantic red。
- 删除/增加 topo object、childElementMaps 或 mapperHistory 必须被完整 gate 捕获。
- ReferenceShadow BREP 出现在非允许路径必须失败；两份语义等价 evidence 解析到同一当前 subshape 时，必须得到同一当前 graph 几何，且不得直接复用 snapshot。

## 非目标

- 不删除 `results[].mesh`、正式 response subshapes 或 App::Link 前端结果。
- 不手改 collector-owned `.freecad.json` / `.freecad.ledger.json`。
- 不把 ledger 或 topoNamingState 当作几何输入或服务端 session。
- 不把 synthetic HistoryProbe 几何冒充 native FreeCAD oracle。
- 不在 runtime、adapter 或 comparator 中按 fixture 名称拼业务结果。
- 不扩到 C13-M5 S4 的五个代表 family；先让 c4m6 成为可靠 release-gate 模板。
- 不在本轮重构 ledger v1 为完整 FreeCAD 私有 NamedShape/ElementMap 导出；validator 深化可另开方案。
- 不修改前端消费协议；transport contract 只由现有 adapter tests 和精确 registry 保护。
- 不触碰当前无关 dirty `DESIGN.md` 和框架文档，也不覆盖用户对 `AGENTS.md` 的本地修改。

## 风险与控制

- 风险：把 protocol divergence 当 green，掩盖真实 parity 缺口。控制：exact/semantic/release 三层状态永久分开，外部 phase matrix 的 known gap 不进入通过状态。
- 风险：registry 变成新形式的宽泛豁免。控制：精确五元组、actual contract、单一匹配、stale audit 和负向 mutation tests。
- 风险：hash hard fail 改变现有客户端行为。控制：以当前 `AGENTS.md` 和接口文档为 authority，先锁 diagnostics-only contract，再切 runtime；若要改权威必须另做合同决策。
- 风险：native expected 和 protocol-only case 再次混放。控制：CompoundLink 补 native summary；HistoryProbe 使用 `.expeted.json` 和独立 test。
- 风险：fixture role 漏项让 native discovery 少比较 case。控制：role manifest 全覆盖审计、artifact 互斥规则和 RunEvidence hash；role invalid 直接阻断。
- 风险：live gate 过慢。控制：c4m6 是小批次，每个 case 只运行一次；source payload 在 exact/semantic/release 三层复用。
- 风险：raw token 或 JSON 格式变化制造 freshness 假失败。控制：raw digest 仅作 provenance，freshness 使用同一 comparison profile 的 normalized digest。
- 风险：registry 引用的 consumer test 只登记未执行。控制：S5 通过 `--run-contract-tests` 精确加载并执行选中 registry 的全部 dotted test id，阶段/重型收口都必跑。
- 风险：重生成中途留下半写文件。控制：临时文件 JSON 校验成功后 atomic replace。
- 风险：报告 schema 迁移破坏现有调用。控制：v2 期间保留兼容 shim，S5 后删除并在工具规定记录新合同。
- 风险：focused test 与 module test 重复。控制：interface 是 test surface；新 gate 稳定后删除旧 partial comparison helpers，而不是继续叠层。

## 验收命令

### 本轮短跑

每个实施 step 只运行与该 step 对应的一组：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core

# comparator / registry step
python3 -m unittest tests.test_freecad_expected_public_parity

# runtime request-validation / live gate step
cmake --build build --target cad-core cad_core_ffi
python3 -m unittest tests.test_topo_naming_state_response

# collector / ledger step
python3 -m unittest discover \
  -s tests/内部账本完整性验收 \
  -p 'test_freecad_expected_ledger_integrity.py'
```

对应 step 结束后只检查本轮文件：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- \
  cad-core/tools \
  cad-core/src/runtime/topo_naming_state.cpp \
  cad-core/src/runtime/recompute.cpp \
  cad-core/src/runtime/reference_resolution.cpp \
  cad-core/src/runtime/reference_lifecycle.cpp \
  cad-core/src/runtime/element_reference_update.cpp \
  cad-core/src/app/link.cpp \
  cad-core/src/part/topo_shape.cpp \
  cad-core/src/part/topo_shape_expansion.cpp \
  cad-core/tests/test_freecad_expected_public_parity.py \
  cad-core/tests/test_topo_naming_state_response.py \
  cad-core/tests/test_p5_sketch.py \
  cad-core/fixtures/c4m6 \
  docs/接口规定/7-8-11-08-topoNamingState客户端携带状态接口方案.md \
  docs/工具规定 \
  docs/CADCore13.0/C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次 \
  docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次 \
  docs/重构/7-10-06-25-FreeCADExpectedReleaseGate深模块重构方案.md
```

### 阶段回归

只在 S2-S5 完成后执行 c4m6 闭环：

```bash
cd /Users/li/Chili3DProject/FreeCAD
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd \
  python3 cad-core/tools/collect_freecad_expected.py \
  --phase c4m6 \
  --skip-unsupported \
  --emit-ledger \
  --validate-ledger

python3 cad-core/tools/validate_freecad_expected_ledger.py \
  --phase c4m6 \
  --strict

cd cad-core
cmake --build build --target cad-core cad_core_ffi
python3 tools/compare_freecad_expected.py --phase c4m6 --write-current
python3 tools/compare_freecad_expected.py --phase c4m6 --strict --live
python3 tools/compare_freecad_expected.py --phase c4m6 --release-gate
python3 tools/compare_freecad_expected.py --phase c4m6 --run-contract-tests
python3 -m unittest \
  tests.test_freecad_expected_public_parity \
  tests.test_topo_naming_state_response
python3 -m unittest \
  tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_reference_shadow_brep_uses_shared_vertex_geometry_not_bbox_fingerprint
```

阶段回归必须核对 collector 的 processed/skipped/failed 统计、report 的非零 case 数、registry audit 和 current freshness，不能只看 exit code。
`tests.test_freecad_expected_public_parity` 必须继续覆盖五个 S4 representative phase 的 snapshot discovery/classification 兼容性；这是共享工具迁移回归，不表示本轮实现这些 phase 的 parity。

### 重型收口

仅在 S6 / C13-M5 S5 收口时执行，不跑全量 FreeCAD build 或全部 phase：

```bash
cd /Users/li/Chili3DProject/FreeCAD
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd \
  python3 cad-core/tools/collect_freecad_expected.py \
  --phase c4m6 \
  --check \
  --skip-unsupported

python3 cad-core/tools/validate_freecad_expected_ledger.py \
  --phase c4m6 \
  --strict

(cd cad-core && cmake --build build --target cad-core cad_core_ffi)
(cd cad-core && \
  python3 tools/compare_freecad_expected.py --phase c4m6 --release-gate)
(cd cad-core && \
  python3 tools/compare_freecad_expected.py --phase c4m6 --run-contract-tests)

python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py \
  docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次/工作步骤细分 \
  --format markdown

awk -F '\t' \
  'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' \
  docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次/矩阵/*.tsv

git diff --check -- \
  cad-core/tools \
  cad-core/src/runtime/topo_naming_state.cpp \
  cad-core/src/runtime/recompute.cpp \
  cad-core/src/runtime/reference_resolution.cpp \
  cad-core/src/runtime/reference_lifecycle.cpp \
  cad-core/src/runtime/element_reference_update.cpp \
  cad-core/src/app/link.cpp \
  cad-core/src/part/topo_shape.cpp \
  cad-core/src/part/topo_shape_expansion.cpp \
  cad-core/tests/test_freecad_expected_public_parity.py \
  cad-core/tests/test_topo_naming_state_response.py \
  cad-core/tests/test_p5_sketch.py \
  cad-core/tests/内部账本完整性验收/test_freecad_expected_ledger_integrity.py \
  cad-core/fixtures/c4m6 \
  docs/接口规定/7-8-11-08-topoNamingState客户端携带状态接口方案.md \
  docs/工具规定 \
  docs/CADCore13.0/C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次 \
  docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次 \
  docs/重构/7-10-06-25-FreeCADExpectedReleaseGate深模块重构方案.md
```

## 完成条件与推荐顺序

推荐严格按 S0 → S1 → S2 → S3 → S4 → S5 → S6 顺序实施。先建立 seam，再修权威与 oracle，随后冻结精确 registry，最后接 live release gate；不要在旧 broad classifier 上继续叠加例外。

完成条件：

- c4m6 request validation 覆盖 schema、producer、hash、encoding 和 object ownership。
- CompoundLink 有 native semantic result authority；HistoryProbe provenance 已分离。
- ReferenceShadow 完整字段、cardinality、BREP 唯一路径和 evidence-only 行为都有 live test。
- report v2 同时保留 exact、semantic、release 状态。
- 正式 registry 无通配、无 stale、无 duplicate、无未消费 entry。
- fixture-role manifest 全量覆盖选中 input，artifact role audit 通过且 hash 进入 RunEvidence。
- 选中 registry 的全部 `contractTests` 可加载并实际执行通过。
- release gate 基于当前 binary，能以 evidence invalid 拒绝 stale `cad-core-res`、0 case 和任何 infra/config 错误，并以 red 拒绝任何 unaccepted semantic diff。
- `compare_freecad_expected.py` 与 `regenerate_cad_core_res.py` 退化为薄 CLI adapter，旧 partial helper 和 broad acceptance 已删除。
- C13-M5 S5、接口规定、工具规定和相关矩阵与 live 行为一致。

达到以上条件并完成分层验收后，再把本文件按原时间前缀重命名为 `7-10-06-25-【已实现】FreeCADExpectedReleaseGate深模块重构方案.md`。
