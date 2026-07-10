# CAD Core 根输入 Fixture 到 FreeCADCmd 公共语义等价框架方案

## 结论

本仓库的长期目标固定为：对同一份 `cad-core/fixtures/<phase>/<case>.json` 根输入，分别运行 FreeCADCmd collector 和当前 CAD Core binary；以同次 FreeCADCmd 采集生成、可复现且账本闭合的 `expected/<case>.freecad.json` 与 `expected/<case>.freecad.ledger.json` 为权威，要求 CAD Core 与 FreeCADCmd 的**公共语义投影一致**。CAD Core 面向产品端额外发布的 `mesh`、传输元数据、`binaryPayloads`、`documentObjectUpdates` 等字段不要求在 FreeCAD expected 中逐字段出现，但必须由独立产品合同验证。

形式化地说，对每个 **FreeCAD public-root native fixture** `f`：

```text
FreeCADCmd(f) -> expected(f) + ledger(f)
CAD Core(f)   -> actual(f)

目标完成 =
  expected/ledger 来源可复现
  + fixture/expected/ledger 哈希与语义闭包有效
  + semanticCompare(
      expectedProjection(expected(f)),
      actualProjection(actual(f)),
      comparisonProfile
    ) = green
  + productExtensionContracts(actual(f)) 全部通过
```

这里的“输出等价”不是 JSON 字节、字段集合或原始字符串逐项相同，而是版本化 comparison profile 投影、规范化后的公共几何、拓扑、引用、诊断和稳定身份语义一致。FreeCAD expected 声明的公共语义字段在 CAD Core 中缺失或含义不一致仍是 red；CAD Core 独有字段只要不篡改公共语义，并通过自己的产品合同，就不构成 FreeCAD parity 失败。

`CadCore::...Probe`（例如 `CadCore::C3M1TopologyProbe`）不属于这里的 `f`，并且永久不会进入 native。它们是 CAD Core 为验证抽取后的 `NamedShape`、`ElementMap`、`MapperHistory`、`ShapeFix` 等低层语义而准备的内部输入，由专用 probe binary / focused test 直接运行。FreeCAD 源码可以有对应的低层 API，但 FreeCADCmd 不认识这个自定义 `DocumentObject` 类型；collector 拒绝它是输入边界的正常结果，不是“FreeCAD 没有这项能力”。本项目不修改 FreeCAD 代码，也不把这些 probe 改写成 FreeCAD public-root 输入，因此它们不生成 native expected/ledger。

必须区分两个结论：

- `semanticStatus=green`、`productContractStatus=green | not_applicable` 且 live `releaseGatePassed=true`，才表示达到了本方案所说的公共语义等价；当前 report schema 尚无 product status 时，还必须确认 registry `actualContract` 校验和 `--run-contract-tests` 都通过。`releaseStatus` 可以是 `green` 或经过精确合同保护的 `protocol_divergence`。
- `exactStatus=green` 仅表示当前 comparison profile 纳入的比较面零 diff；它不证明原始字段集合或已分流的产品扩展一致，是更强但非必需的辅助信号。
- 当前工具里的 `protocol_divergence` 不是“随便允许字段不同”，而是差异必须精确到 path、有 actual contract、consumer test、authority 和关闭/替换条件；未登记或合同不成立的差异仍是 red。它是现阶段承接产品扩展/表现差异的过渡 verdict，长期应由明确 semantic projector 与独立 product contract 直接表达。

禁止为了让比较变绿而修改 expected 追随 CAD Core 当前输出。证据充分的公共语义 red 应修 CAD Core；只有证据证明 collector/oracle 或 semantic projector/comparator contract 错误时，才修对应证据或比较层并重新验收。

## 当前基线

以下数字来自 2026-07-10 当前磁盘上的 fixture tree 与 `fixture_roles.v1.json`，只用于说明起点；以后必须从 live tree 重新统计，不能把数字固化成接口：

| 项目 | 当前数量 | 含义 |
| --- | ---: | --- |
| phase 目录 | 52 | 其中 `c7m1` 当前为空目录 |
| 根输入 fixture | 775 | `fixtures/<phase>/*.json` |
| native | 475 | 有同名 `.freecad.json` 与 `.freecad.ledger.json` |
| protocol-only | 3 | 只有人工 `.expeted.json` 协议合同，不是 native oracle |
| unsupported | 297 | 仍有明确 blocker，不能计为 parity 已完成；其中 `c3m1` 的 9 个 `CadCore::C3M1TopologyProbe` 属于内部 probe，分类后从 FreeCAD-applicable native backlog 单列排除 |
| CAD Core current snapshot | 775 | 磁盘存量；只有 475 个 native current 进入现行 parity/freshness gate |

475 个 native expected 与 475 个 ledger 当前一一配对；其中 470 个是 accepted response，5 个是合法的 request-level rejected response。rejected fixture 仍然是 native oracle：它必须返回匹配的 diagnostics、空结果/更新边界和 rejected ledger，不能被归为 unsupported。

当前全 corpus snapshot strict 诊断中，475 个 native case 只有 5 个 exact green、470 个 red，共报告 62,308 个 diff，另有 300 个 protocol-only/unsupported case 被单列跳过。这个数字混合了公共语义差异、允许的表现差异、CAD Core 产品扩展和 expected 中的比较/取证元数据；它只能作为待分类的 snapshot backlog，不能直接等同为 62,308 个业务缺口，也不能证明当前 binary 的 live release 状态。

当前 native expected 中没有 `known_gap`。即使以后 collector 发布解释性 `known_gap`，它也不能自动跳过比较或计为通过。

当前 divergence registry 只批准 c4m6 的 5 个精确 transport 差异：4 个 `mesh` path 和 1 个 `subshapes` path。只要公共语义无未接受 diff、这些 entry 的 actual contract 与 consumer test 都成立，c4m6 可以达到本方案要求的 semantic parity；它是否 exact green 只作为附加报告。后续 projector/product contract 落地后，永久产品字段应退出 divergence registry。

## 权威层级与 artifact 职责

| Artifact | 职责 | 是否权威 | 纪律 |
| --- | --- | --- | --- |
| `fixtures/<phase>/<case>.json` | 单次无状态 recompute 的根输入：DocumentObject graph、target、参数和可选旧 `topoNamingState` | 输入事实 | 同一文件必须分别喂给 FreeCADCmd 与 CAD Core |
| `expected/<case>.freecad.json` | FreeCADCmd collector 生成的 public response oracle | native 结果权威 | collector-owned，不手改 |
| `expected/<case>.freecad.ledger.json` | 同次 capture 的 producer、hash、对象、事件、projection、coverage、round-trip 证明 | native provenance 权威 | 缺失或不闭合即 hard fail |
| `expected/<case>.expeted.json` | 人工 CAD Core 协议/诊断合同 | 仅 protocol-only 合同 | 不得冒充 FreeCADCmd 输出，不进入 native parity verdict |
| `cad-core-res/<case>.cad-core.json` | 当前 CAD Core 输出快照 | 否 | 只能由 live binary 生成并逐文件原子替换；现行 gate 只生成/审计 native current，不能反推 expected |
| `cad-core/tests/*probe` 及其 probe fixture | 抽取语义的内部回归证据 | 永久不是 native oracle | 由专用 probe binary、focused tests 和 FreeCAD 源码依据验收；不生成、不要求 `.freecad.json` / `.freecad.ledger.json`；项目不通过修改 FreeCAD 或重写 probe 把它们纳入 native |
| `cad-core/out/` | 临时报告、裁决和本地采集结果 | 否 | 不作为 checked-in baseline |
| `fixture_roles.v1.json` | native / protocol-only / unsupported 唯一机器角色来源 | corpus 分类权威 | 禁止再从文件后缀、`fixtureCategory` 或启发式建立第二套角色系统 |
| `protocol_divergences.v1.json` | 当前工具精确批准的表现/产品协议差异 | 过渡性受控例外登记 | 当前 schema 必须带 exact selector、actual contract、consumer test 和删除条件；永久产品字段后续迁入独立产品合同 |

FreeCAD `src/` 是业务语义和调用链权威；FreeCADCmd 同次生成并通过 replay/ledger 门禁的 expected pair 是公开结果验收权威。两者分工不同，不能拿当前 CAD Core 输出、Web 输出或人工 fixture 反推 FreeCAD 语义。

## 禁止的数据反向流

```text
root fixture + fixture role
  ├─ FreeCADCmd collector
  │    └─ public expected + authority ledger
  └─ CAD Core live binary
       └─ actual public response

expected + ledger preflight + actual
  └─ freecad_expected_parity
       ├─ semantic green + product contract green + live gate：本目标完成
       ├─ exact green：更强的可选信号
       ├─ protocol divergence：产品扩展受精确合同保护
       └─ red / invalid：回到证据或 FreeCAD 调用链修复
```

不允许出现以下反向箭头：

- `cad-core-res -> expected`
- `actual response -> collector business logic`
- `已落盘 expected -> FreeCAD native ledger event`
- `fixture name -> CAD Core feature branch`
- `parity comparator -> runtime 输出修剪`

只有确认根输入表达错误时才修改 input fixture；只有确认 collector 不符合 FreeCAD 源码或原生运行结果时才修改 collector 并用 FreeCADCmd 成对重生 expected/ledger。普通 parity red 不授权修改 oracle。

同次 collector 可以使用刚生成的 public payload 建立 expected hash、projection 与 round-trip 关联；禁止的是从 checked-in、手改或 CAD Core 反推的 expected 补造 producer/history event。

## 公共语义等价的比较口径

唯一比较规则由 `cad-core/tools/freecad_expected_parity/` 深模块维护。外部只跨两个 interface：

```python
evaluate(EvaluationRequest) -> ParityReport
materialize_current(MaterializeRequest) -> GenerationReport
```

`compare_freecad_expected.py`、`regenerate_cad_core_res.py` 和 unittest 都只是 adapter 或调用方，不得复制 discovery、normalization、diff、registry、freshness 或退出码规则。删除这个深模块时，这些复杂度会重新散落到所有调用方，因此该 seam 必须保持单一。

parity 深模块必须先把双方数据分成三类，再作比较：

| 数据类别 | 例子 | 验收方式 |
| --- | --- | --- |
| FreeCAD 公共语义 | `results` 中的几何/拓扑摘要、`diagnostics`、`elementReferenceUpdates`、公开 `topoNamingState`、`mappedName.canonical` | expected 与 actual 的语义投影必须一致；缺失或语义不同即 red |
| CAD Core 产品扩展 | `mesh`、前端传输元数据、`binaryPayloads`、`documentObjectUpdates` | 不要求 FreeCAD expected 具有同名字段；用版本化产品 schema/contract 与 consumer tests 单独验收；当前 registry 只作过渡承载 |
| Oracle provenance | FreeCAD/OCCT 版本、native probe、solver/marker 取证、raw events、round-trip | 放在 ledger/sidecar 验证来源，不要求 CAD Core runtime 发布 |

所谓公共语义投影不是简单取两个 JSON 的字段交集。FreeCAD expected 已声明的公共字段是必需语义，CAD Core 不能通过省略字段来缩小投影；只有经接口合同明确认定为产品扩展或 oracle provenance 的字段才从双方公共比较面分流。

当前 comparison profile 已实现的规范化只有：

- JSON object key 顺序和排版不参与比较。
- `diagnostics`、`results`、`subshapes`、`childElementMaps`、`mapperHistory` 按各自稳定 key 比较，而不是按数组物理顺序比较。
- `mappedName.raw` 只对 `:H...`、`:H:...`、`;D...` 运行期片段做有限 canonicalization；其他 raw 差异当前仍可能形成 diff。目标 semantic projector 应不跨 producer 比较 raw，而严格比较对应 entry 的 `mappedName.canonical`。raw 仍必须分别通过 FreeCAD ledger 与 CAD Core codec 的 producer-local `raw -> canonical` 完整性校验，canonical collision 继续显式进入 MapperHistory/diagnostics；不能把“不跨 producer 比 raw”变成接受无效 raw。canonical 用于公开比较证据和稳定 diff，不能替代原生 mapped-name 解析或引用恢复输入。
- geometry numeric path 使用 `1e-6` 容差。
- native expected 未声明时，当前 comparison profile 全局忽略顶层 actual extra `binaryPayloads` 和 `documentObjectUpdates`；长期应给这些产品扩展补独立合同，而不是只靠 broad ignore。

comparison profile 是版本化验收合同。修改 profile 必须有 FreeCAD/协议依据、focused comparator tests 和对历史报告的影响说明；不能把新增 mismatch 直接加入 ignore 列表。

### 当前 comparison contract 必须先裁决的问题

以下是现有工具/fixture 的已知合同债务，S0 必须先给出 source-backed 结论，否则 semantic projection 仍可能比较错对象：

- `documentObjectUpdates` 与 `binaryPayloads` 属于 CAD Core 产品响应扩展，不进入 FreeCAD native parity projection；当前引擎只把它们整体忽略，没有验证内容，必须补独立 schema/consumer contract，约束 action、object、reason 和 payload 等产品语义。
- 当前部分 native expected 把 `bbox_delta`、`length_delta` 等旧测试容差元数据放在 result 中；旧 focused test 把它们当比较配置，新 exact engine 会把它们当 runtime 字段。应把比较配置移入版本化 comparison profile/sidecar，或明确将其升级为 public 字段，不能维持双重解释。
- Assembly 等 expected 中存在 `native_solver`、`native_marker_oracle`、`wrapper_oracle`、`native_error` 等 oracle-only 候选字段。必须裁决它们是 public response contract 还是 provenance；若属于 provenance，应由 collector 迁入 ledger/sidecar 并成对重生 expected，不能要求 CAD Core 为了过比较发布取证字段。
- comparator 当前只 canonicalize `mappedName.raw` 的部分 `:H` / `;D` 运行期片段，还没有完整落实“raw 可不同、canonical 必须一致”的 sibling-aware 比较；需要按同一 entry 的 canonical evidence 判定，不能靠逐 path 放行任意 raw 差异。
- comparator 目前按 indexed subname 建 key，还没有“几何等价但 Internal* 编号顺序不同”的可执行 matcher；在 matcher 有完整几何/source/history 证明前，不得用文字说明替代门禁。
- `family_metadata.py` 的自动 owner 路由仍含旧 `src/features` / `src/geometry` 路径和粗 phase 分类，只能作为诊断提示。队列 owner 必须由 live diff + FreeCAD 调用链 + 当前同构模块共同确定。
- `materialize_current()` 会先 stage selected cases，再逐文件 atomic replace；它不是跨多个文件的事务。中断后的完整性由 live/current freshness gate 兜底，文档和测试不得宣称“整批原子提交”。

这些裁决允许因为“public expected 混入了非 public 元数据”而修 collector 并重生 oracle；不允许因为 CAD Core 当前缺字段而把真实 FreeCAD public 字段迁走。

### 当前 verdict 与目标补充

| Verdict | 含义 | 能否宣称公共语义已对齐 |
| --- | --- | --- |
| `exactStatus=green` | 当前 comparison profile 下零 diff | 可以，但它是比本方案要求更强的附加结论 |
| `semanticStatus=green` | 公共语义零未接受 diff；其余差异都被精确 registry 消费且 actual contract 成立 | 可以，但完成时还必须通过产品合同与 C++ live release gate |
| `releaseStatus=green` | live source、artifact、freshness、semantic 与 registry gate 全部通过，且没有剩余 diff | 可以 |
| `releaseStatus=protocol_divergence` | 当前 live gate 通过，公共语义一致，剩余 diff 是受合同保护的产品扩展/表现差异 | 可以；这是现行工具的过渡 verdict，不能宣称字段集合或原始 JSON 完全一致 |
| `red` | 有未批准的语义或协议差异 | 不可以，回到实现修复 |
| `invalid` | role、artifact、ledger、runner、JSON、registry 或 current freshness 无效 | 没有可用结论 |
| `not_evaluated` | snapshot 等非 release source 只能形成诊断 | 没有可用结论；零有效 case 直接是 `invalid` |

`known_gap`、unittest skip、snapshot green、单个字段断言通过和 CLI 返回 0 都不能替代上述 live verdict。

目标 verdict 还应显式提供 `productContractStatus=green | red | not_applicable`，并记录 semantic projection schema digest。当前 report schema 尚无这些字段，暂由 registry `actualContract` 与 `--run-contract-tests` 共同承担；S0/S4 应把永久产品扩展迁入独立 product contract，临时兼容例外才继续使用 divergence registry。这些能力继续藏在同一个 `evaluate(EvaluationRequest)` 深模块后面，不新增第二套 comparator 或 CLI 判断逻辑。

当前顶层兼容字段 `status`、`CaseReport.status` 和 `summary.passed/red` 仍按零 diff 的 legacy exact 口径计算，不能作为本方案的完成判断。parity report 应补 per-case semantic verdict；在该字段落地前，单 case 是否 semantic green 必须由其 unaccepted diff 为零、registry actual contract 成立和相关 consumer tests 通过共同证明。

parity engine 还支持 `rust-ffi` live source，但该 source 不走本仓库 C++ CLI 的 checked-in current freshness 规则。本方案关闭 CAD Core C++ fixture 目标时只接受 `sourceKind=live` 的 `build/cad-core` 结论；`rust-ffi` 只作为跨实现/adapter 对照，不能替代 C++ live gate。

### 命名顺序差异

`InternalFaceN`、`InternalEdgeN`、`InternalVertexN` 等只有编号顺序不同、但几何等价且 CAD Core 输出顺序稳定时，不得归类为几何硬失败。比较层应单独报告 `naming_order_difference` 或等价分类，但必须先证明：

- shape kind 与数量一致；
- 几何内容存在唯一或可审计的等价映射；
- source/history 关系没有丢失；
- stable identity 与引用恢复语义没有改变；
- CAD Core 自身顺序可复现。

这类 case 是 semantic non-failure，但必须先把 proof-driven bijection matcher 纳入版本化 comparison profile：按 kind/数量、几何、source/history、stable identity 和引用恢复建立全路径一致映射，并报告 `naming_order_difference`。matcher 尚未实现或证据不完整时保持 red/needs-evidence，不能靠逐 path registry 猜测编号置换。face/edge/vertex 数量、几何、stable subname、引用恢复或 MapperHistory 不一致仍是 hard red，不能借“命名顺序”放行。

## Corpus 角色与最终分母

### 内部语义 probe（明确排除 native）

内部 probe 的判断条件是：自定义 `CadCore::...Probe` 根对象、专用 probe 执行入口，以及输出低层几何/历史账本而非 FreeCAD public response。它们与 FreeCAD 的关系是“实现语义来源相同、输入/证据层不同”：FreeCAD `src/Mod/Part/App` 中的 `TopoShape`、`MapperHistory`、`ShapeFix` 等调用链仍是 CAD Core 对齐依据，但不因此要求 FreeCADCmd 为自定义 probe 生成标准答案。

对这类 case，验收闭环应是：

```text
FreeCAD 源码调用链/字段依据
  + cad-core 专用 probe binary
  + focused semantic tests
  = 内部语义 probe 通过
```

它们永久不进入 native 分母，不进入 native release gate，也不属于“所有 FreeCAD-applicable unsupported 必须转 native”的清单。当前 `fixture_roles.v1.json` 仍只有 `native`、`protocol_only`、`unsupported` 三种机器角色，因此 probe-only 作为报告中的永久概念分类：保留明确的内部 probe/unsupported 原因并从 native backlog 单列排除；不得为了角色数量好看而伪造 native expected，修改 FreeCAD 代码，或把 collector 的拒绝改成成功。

若产品确实需要把某个 probe 暴露为协议合同，应另建 `.expeted.json` 和读取真实 CAD Core 输出的 focused contract，才可按 `protocol_only` 单列；这也不会改变原 probe 的 native 排除状态。原 `CadCore::...Probe` 文件不会因为测试通过而升级为 native，本项目也不通过修改 FreeCAD 代码来改变这一边界。

每个根输入必须且只能有一个 role：

- `native`：FreeCADCmd 可以给出 accepted 或 rejected public oracle；必须有 expected/ledger pair，并进入 native semantic parity。
- `protocol_only`：FreeCAD Python/collector 无法表达，或明确属于 CAD Core 产品扩展；必须有 source-backed 原因和 focused contract，不能宣称 native parity。
- `unsupported`：当前尚未取得 native oracle，也没有批准的 protocol-only 结论；它表示 backlog/blocker，不是 skip 后的成功。

因此，全 corpus 的关闭条件不是“475 个 native 里有多少通过”，而是：

1. 775 个输入始终角色闭合，catalog 无 missing、duplicate、orphan 或 stale entry。
2. 所有 **FreeCAD-applicable** 的 unsupported 都通过批量 collector 支持转为 native；内部语义 probe 不属于 FreeCAD-applicable public-root 输入，按上面的 probe 闭环单独验收，不得强行转 native。
3. 真正无法存在 FreeCADCmd 输出的 case 才能转为 protocol-only，并记录 FreeCAD/source 依据、合同测试和重新裁决条件。
4. 所有 native case 最终 `semanticStatus=green`，并由 C++ live source 获得 `releaseGatePassed=true`。
5. 所有 CAD Core 产品扩展达到 `productContractStatus=green | not_applicable`；当前工具过渡期必须由 actual contract 与 consumer tests 证明。
6. protocol-only case 的 focused contract 全绿，但它们始终从“与 FreeCADCmd 公共语义等价”的分母中单列，不伪装成 native。
7. 内部 probe case 的专用测试和源码依据全绿，并在报告中单列；它们永久不计入 native 完成率，也不构成 native 缺口。

CAD Core 的 `runtime` 继续发布面向产品端的完整 response，不需要为了模仿 `.freecad.json` 删除 `mesh` 或其他产品字段。公共语义 projection seam 放在 `freecad_expected_parity` 深模块中：它负责从 expected 与 actual 提取、规范化同一语义面；产品独有字段继续保留在 actual，并由独立产品 schema/contract 与 consumer tests 验收，当前 registry 只作迁移期桥接。CLI 只运行和序列化正式 response，不承担字段修剪。禁止通过简单字段交集、whole-object ignore 或 adapter 级删除来制造 semantic green。

## CAD Core 模块落点

同一根输入必须沿当前 FreeCAD 同构框架重建，adapter 不承担业务补偿：

```text
adapters: JSON/C ABI 转换
    -> app: DocumentObject、properties、links、copy-on-change
    -> graph: 依赖、target、cycle、recompute plan
    -> sketcher / part / part_design / mesh / assembly: FreeCAD 模块业务语义
    -> part: OCCT shape、NamedShape、ElementMap、MapperHistory、FaceMaker、WireJoiner、ShapeFix
    -> topo: 跨模块 stable identity 与 mapped-name codec
    -> runtime: request-local 调度、diagnostics、引用更新与 public response 投影
    -> adapters: 序列化，不修正结果
```

差异修复按以下映射落位：

| Diff 类型 | FreeCAD 依据 | CAD Core 主要落点 |
| --- | --- | --- |
| 对象创建、属性、Link/LinkSub | `src/App/Document*`、`Property*.cpp`、`PropertyLinks.cpp` | `app/`，依赖顺序才进入 `graph/` |
| Part container 与 PartDesign Body/Tip/feature chain | `src/Mod/Part/App/BodyBase.cpp`、`src/Mod/PartDesign/App/Body.cpp` | 通用容器语义在 `part/`，Body/Tip 与 PartDesign 控制流在 `part_design/` |
| recompute target、依赖、循环与调度 | `Document*`、`DocumentObject*` | `graph/` + `runtime/` |
| Sketch 原始 Shape、InternalShape、external geometry | `src/Mod/Sketcher/App` | `sketcher/`；低层 FaceMaker/WireJoiner/history 在 `part/` |
| TopoShape、布尔、FaceMaker、WireJoiner、ShapeFix、导入导出 | `src/Mod/Part/App` | `part/` |
| Body、Pad、Pocket、Hole、DressUp、Pattern、Transform、Datum | `src/Mod/PartDesign/App` | `part_design/`，共享 OCCT/history 继续下沉 `part/` |
| NamedShape、ElementMap、maker history | `PropertyTopoShape*`、`TopoShape*`、`TopoShapeMapper*` | `part/` 生产/传播，`topo/` 编码，`runtime/` 发布 |
| mesh/assembly | 对应 FreeCAD module | `mesh/` / `assembly/` |
| public DTO 差异 | `docs/接口规定` 与 collector public projection | `runtime/`；`adapters/` 只转换 |

每次实质语义修复前必须记录 FreeCAD 源文件绝对路径、类/函数、关键字段或短句、调用顺序和上述 cad-core 落点。新增公开语义类型、executor、mapper/history 规则时，在相邻 C++ 注释保留同样依据。

## topoNamingState 专项约束

`DocumentObject graph` 是唯一建模事实。请求中的 `topoNamingState` 是不可信、客户端携带的旧引用证据；它不是 server cache、session、旧 shape 或 BREP 建模输入。`ReferenceShadow.brep` 只允许保存被引用单个 subshape 的恢复证据。

native parity 必须同时约束：

- 输入 state 的 schema、producer、document/object hash、encoding 与 owner 前置校验；不兼容输入 hard fail，不返回新 state。
- request-local `NamedShape` 的 subshape inventory。
- `ElementMap` 只发布唯一恢复且 target 当前存在的 entry。
- split、deleted、ambiguous、merge/collision 等进入 `MapperHistory` 与 diagnostics，不静默覆盖。
- `childElementMaps`、owner-qualified path、`StableSubList`、`FullSubList`、Shadow/Reference evidence 保持 item-local 对齐。
- runtime publisher 只投影 `part/` 已有账本，不创造 feature 语义。

同一个 `FaceN` 字符串在两次 recompute 中仍存在，不足以证明 parity。还必须比较 shape/subshape 数量、bbox/几何、source/history、element map 和引用恢复结果。拓扑命名差异优先沿 `part` 的 producer/history 账本定位，禁止在 `sketcher`、`runtime` 或 adapter 按 fixture 名、几何类型、split 顺序或 source index 猜输出。

## 差异裁决顺序

每个 red/invalid case 固定按以下顺序处理，不能直接跳到高层输出修补：

1. **Corpus 合法性**：role 是否唯一，input/expected/ledger/protocol artifact 是否符合角色；0 case、orphan、stale 均先修 catalog。
2. **Oracle 闭包**：expected/ledger hash、producer、projection、coverage、round-trip 和 rejected diagnostics 是否有效。
3. **Native replay**：用同一 FreeCAD/LibPack/OCCT 重新运行 collector，确认 checked-in pair 可复现。
4. **Current/live 证据**：由 case evaluation 检查 native current 是否存在、JSON 是否有效、live runner 是否成功和 normalized freshness 是否一致；这些不是 catalog 角色审计的职责。
5. **环境归因**：若只有 OCCT/BREP/bbox/data-exchange 漂移，先对齐基线；无法对齐时写入 `docs/temp/`，不改 expected、不放宽断言。
6. **根输入语义**：确认 object graph、recompute target、link envelope 和前态 state 自洽。未来的 input validator 必须消费 `fixture_roles.v1.json`，不得再按 `fixtureCategory` 猜角色。
7. **FreeCAD 调用链**：读取 `src/`，确定 geometry、property、history、recovery 与 publication 的真实顺序。
8. **CAD Core 低层运行态**：先补 `part` 的 OCCT/history/NamedShape/ElementMap 或 `app` 的 property/link，再切高层 executor。
9. **Runtime 投影**：只在底层证据齐全后发布 diagnostics、updates 与新 `topoNamingState`。
10. **差异分流**：先裁决为公共语义、允许表现差异、CAD Core 产品扩展或临时兼容例外。公共语义进入 projector/matcher；永久产品扩展进入独立 schema/consumer contract；只有临时例外进入 divergence registry，并要求 authority 与 `removeWhen`。
11. **删除 fallback**：批次变绿后删除被正式流程替代的 synthetic name、pruning、geometry guess 和旧 fallback。

一旦出现输出端 pruning 越来越多、同一路径反复补规则、或靠 source/split 几何形态猜 ownership，应立即停止扩充规则，回到 FreeCAD 内部账本/状态机。

## 分阶段实施

### S0：锁定 corpus 与输入门禁

- 以 `fixture_roles.v1.json` 作为唯一 role source，保持 775 个根输入完整覆盖。
- 审计 missing/duplicate/orphan/stale role 与角色不允许的 artifact。
- 补足根输入静态 validator：对象 Name/ID、recompute target、link 引用、state owner/hash 和禁止的 output 字段；validator 读取 role manifest，不新增第二套 `fixtureContract` 分类。
- 完成 comparison contract 裁决：公共语义、产品扩展、比较容差元数据、oracle-only provenance 和命名顺序分类各自只有一个归属。
- 实现明确的 semantic projection：expected 声明的公共语义是 required；`mappedName.raw` 可不同但 sibling `mappedName.canonical` 必须一致；CAD Core extra 不进入 native equality，但必须进入产品合同。
- 建立独立 product-extension schema/contract，替换无合同的 broad ignore；当前 exact registry 只桥接尚未迁移的 case-specific 差异，尤其补齐 `documentObjectUpdates` 与 `binaryPayloads` 的产品验收。
- 修正自动 family/owner metadata 的旧路径与粗分类；在此之前它只做提示，不自动决定实现落点。
- 产出 live baseline report，按 phase、family、`semantic green/red`、`needs-evidence/projection-decision`、product contract 和落点聚合；exact diff 只作辅助统计，snapshot 不给 release 结论。

### S1：关闭 native oracle 缺口

- 按共享 FreeCAD 调用链批量处理 FreeCAD-applicable 的 unsupported，不按单 fixture 零散推进；先把 `CadCore::...Probe` 这类内部 probe 从 native backlog 中分类排除。
- collector 缺能力时先实现 FreeCAD 原生构造/属性写入/采集路径，再用同次运行成对生成 expected 与 ledger。
- 每批至少覆盖正常、边界/失败、引用或历史传播等代表场景；原生调用链分叉、无法采集或语义不清时才拆批，并记录下一批范围。
- accepted 与 rejected native case 都进入相同 authority/replay 门禁。

### S2：建立 family parity 队列

- 用 live CAD Core 对每个 native case 生成 actual；diff 统一由 parity engine 分类。
- 按共享低层 seam 聚类，而不是按文件名排队：`app/link`、`part/history/topo`、`sketcher`、`part_design`、`part data exchange`、`mesh/assembly`。
- 优先关闭能同时消除多个高层 fixture 差异的 `part`/`app` 深模块缺口，再处理 feature-specific 控制流。
- 每个队列项写明 FreeCAD 调用链、代表 case 集、expected categories、cad-core 落点、focused tests、协议/能力文档和完成门禁。

### S3：按最小完整语义批次实现

每批固定执行：

1. 读 FreeCAD 并记录调用链。
2. 补共享 OCCT/history/property/link 运行态。
3. 补正式核心 interface 与语义单测。
4. 切换 `sketcher` / `part` / `part_design` 等主路径。
5. 在 `runtime` 发布完整产品 response，并保证其中的 FreeCAD 公共语义可被 parity projection 稳定提取。
6. 通过 `materialize_current()` 刷新本批 `cad-core-res`；单文件原子替换，多 case 完整性由后续 freshness gate 验证。
7. 跑 case、phase、consumer contract 门禁。
8. 删除 fallback，更新 capability/状态文档。

一个 fixture 达到 per-case semantic green 只能证明一个 case；只有同调用链的代表 case 集都达到 semantic green、产品合同通过且语义单测一起通过，才能关闭该批次。不得用 legacy case `status=red` 否定已被证明的 semantic parity，也不得用 legacy `status=green` 替代 live/contract 证据。

### S4：semantic 收口与协议分流

- 所有公共语义的未接受 diff 必须归零；expected required 字段缺失、几何/拓扑/引用/diagnostics 或 `mappedName.canonical` 不一致不得注册成产品扩展。
- 允许的表现差异由版本化 projector/matcher 明确表达；CAD Core 永久产品扩展移出 native diff，进入独立 product schema/consumer tests，extra field 不要求从正式 response 删除。
- 现有 divergence registry 逐条迁移：永久产品字段转 product contract，临时兼容例外保留 exact entry 与 `removeWhen`，共享语义差异退回 red。
- `evaluate()` / release gate 聚合 semantic projection audit 与 product contract audit；`--run-contract-tests` 不再只依赖 divergence entry 才能发现永久产品合同。
- 只有 FreeCAD `src/` 调用链或原生 FreeCADCmd/probe 证明 collector 漏掉了真实 FreeCAD public 字段时，才修 collector 并重生 oracle。产品接口权威只能定义 CAD Core extra contract，不能单独授权反改 oracle。
- registry entry 必须精确消费一次；stale、duplicate、ambiguous 或 whole-object 规则均 invalid。
- native case 达到 `semanticStatus=green`、`productContractStatus=green | not_applicable` 且 live `releaseGatePassed=true` 即可关闭本方案目标；当前 report schema 未发布 product status 时，以 registry actual contract + contract tests 作为过渡证明。`exactStatus` 继续报告，但不作为 blocker。

### S5：全库 release gate

- 全 native expected/ledger strict 闭包。
- touched phase 通过 live FreeCADCmd replay；正式收口按所有含 native role 的 phase 重放。
- 全 native case 由当前 binary live 执行，current snapshot freshness 一致，`semanticStatus=green` 且 release gate 通过。
- 全部 product/consumer contract tests 通过，命名等价 matcher 无 unresolved/ambiguous case，protocol-only 单独报告。
- 所有 FreeCAD-applicable case 必须已转 native；已经证明不可能有 FreeCADCmd public oracle 的 case 必须转为 protocol-only，或经独立清理裁决移出 root corpus，不能继续以 unsupported 结案。内部语义 probe 永久作为独立 probe lane 报告并通过专用测试，不要求也不允许进入 native。

## 分层验收命令

以下 CLI interface 保持不变；S0/S4 只深化 `evaluate()` 内部的 semantic projection 与 product contract，不另造验收命令。当前 report schema 尚未发布 `productContractStatus` 时，release gate 仍通过精确 registry + `--run-contract-tests` 过渡验收产品扩展。判断结果看 `semanticStatus` 与 `releaseGatePassed`，不能仅因 `exactStatus=red` 判定本方案失败。

### 本轮短跑

普通实现只跑本批相关 target/case，不默认跑全量 FreeCAD：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build --target cad-core
python3 tools/compare_freecad_expected.py \
  --phase <phase> --case <case> --write-current
python3 tools/compare_freecad_expected.py \
  --phase <phase> --case <case> --release-gate --run-contract-tests
git diff --check
```

如果本批修改 collector/expected，增加单 case FreeCADCmd 临时采集与 ledger 验证；不要直接覆盖正式 expected 后才判断 collector 是否正确。

### 阶段回归

```bash
cd /Users/li/Chili3DProject/FreeCAD
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd \
  python3 cad-core/tools/collect_freecad_expected.py \
    --phase <phase> \
    --check \
    --check-ledger \
    --validate-ledger \
    --skip-unsupported

python3 cad-core/tools/validate_freecad_expected_ledger.py \
  --phase <phase> --strict

cd cad-core
python3 tools/compare_freecad_expected.py --phase <phase> --write-current
python3 tools/compare_freecad_expected.py \
  --phase <phase> --release-gate --run-contract-tests
```

### 重型收口

只在阶段/全库收口执行：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 cad-core/tools/validate_freecad_expected_ledger.py --all --strict

cd cad-core
cmake --build build --target cad-core
python3 tools/compare_freecad_expected.py --write-current
python3 tools/compare_freecad_expected.py \
  --release-gate --run-contract-tests
```

当 parity 仍是 semantic red 时，`--run-contract-tests` 会 fail closed 为 `not_run_unaccepted_parity`；这是预期行为，不是命令错误。

全库 FreeCADCmd replay 应按 role manifest 实时发现所有含 native role 的 phase，逐 phase运行 collector `--check --check-ledger --validate-ledger --skip-unsupported`；不维护固定 phase 列表。

## 完成条件

一个语义批次只有同时满足以下条件才可标 `【已实现】`：

- 根输入 role 与 artifacts 合法。
- FreeCADCmd expected/ledger pair 可复现且 strict 闭包。
- 当前 CAD Core binary 对本批所有 native case live 执行成功。
- 公共语义 required fields、几何、拓扑、引用、diagnostics 与 `mappedName.canonical` 一致，`semanticStatus=green`。
- `productContractStatus=green | not_applicable`；当前工具尚未发布该字段时，所有产品扩展必须由 registry actual contract 和 `--run-contract-tests` 过渡证明。
- live `releaseGatePassed=true`；当前 `releaseStatus=protocol_divergence` 可以通过，但每个剩余 diff 都必须是过渡期精确登记并通过 consumer contract 的表现差异或 CAD Core 产品扩展，不能包含公共语义差异。
- `exactStatus` 作为附加指标记录，不要求字段集合、`mappedName.raw` 或原始 JSON 完全一致。
- 失败路径 diagnostics 与 rejected/no-new-state 边界一致。
- `cad-core-res` 由同一 live binary 生成、逐文件替换且全批 freshness 通过。
- focused 语义测试、consumer contract 和 phase gate 通过；内部 probe 另须通过其专用 probe binary/test，但不需要 native expected/ledger。
- 没有手改 expected、fixture-name branch、输出修剪、宽泛 ignore 或新增未标注 fallback。
- FreeCAD 依据、cad-core 落点和剩余风险已记录；只有临时 divergence、fallback 或兼容例外必须记录删除条件。

全库目标完成还要求：所有 FreeCAD-applicable 根输入都已经是 native，所有 native case semantic green 且 live release gate 通过，所有产品扩展合同通过，protocol-only 明确单列，内部 probe 永久单独报告且专用测试全绿，catalog/ledger/registry/current freshness 全部有效；内部 probe 不影响 native 完成率。

## 风险与防线

- **OCCT/FreeCAD 版本漂移**：collector 与 CAD Core 尽量使用同一 FreeCAD/LibPack/OCCT 基线；其他环境只做兼容性 smoke test。
- **过度 projection/normalization**：comparison profile 只能分流真正的产品扩展或消除非语义噪声；expected required 字段不能因 actual 缺失而从投影中消失，每次新增规则都要证明不会掩盖拓扑、引用或诊断变化。
- **collector coverage 被误当 CAD Core gap**：先 replay oracle，再判断实现差异。
- **同名拓扑掩盖几何漂移**：不能只比 `FaceN` 字符串；必须保留 geometry/subshape/history 证据。
- **产品扩展边界失控**：合法、被消费且有独立合同的永久产品扩展不是 parity debt；风险在于它泄漏进公共语义、没有 consumer contract，或被用来掩盖 FreeCAD 几何/拓扑/引用缺口。divergence registry 只保留临时例外。
- **exact diff 膨胀**：产品字段、producer-specific raw token 或 DTO 布局差异可能制造大量非语义 diff；exact 统计不能直接驱动实现优先级。
- **partial test 假绿**：旧 `fixture_expected.py` 的局部断言、`known_gap` skip 或 legacy output 不能作为全 corpus release gate。
- **current snapshot 过期**：release gate 必须运行 live binary，并校验同一 actual 与 checked-in current 的 normalized digest。
- **多 case 刷新中断**：`materialize_current()` 不是跨文件事务；任何中断后都重新跑 selected scope freshness，不把部分刷新视为完成。

## 非目标

- 不把 `.freecad.json` 扩成 FreeCAD C++ 私有 `NamedShape` / `ElementMap` / MapperHistory 的完整导出；内部证明继续放 ledger。
- 不把 `topoNamingState`、ledger 或 `ReferenceShadow.brep` 当服务端 session 或完整几何输入。
- 不要求双方 JSON 字节、完整字段集合、空白、对象 key 顺序、`mappedName.raw` 或已批准的产品扩展一致；公共语义和 `mappedName.canonical` 仍必须一致。
- 不把完整 Sketcher constraint solver、GUI、Workbench、ViewProvider 或 Web session 纳入 cad-core 迁移目标；现有 solver-facing 状态/diagnostics 和 Assembly module fixture 仍按各自已声明协议验收。
- 不默认执行全量 FreeCAD build；验证按短跑、阶段回归、重型收口分层。
- 不从旧 `mvp`、固定 phase 数或 README 历史数量推断当前 corpus。

## 与现有文档和工具的关系

- 输入、FreeCAD expected、ledger、CAD Core 完整响应以及“公共语义一致但产品字段可额外存在”的基本关系，以 [输入输出约定](输入输出约定.md) 为本方案的直接解释依据。
- 长期无状态边界继续以 [CAD Core 抽取方案](../CADCore方案/00-CAD-Core抽取方案.md) 为准；模块落点以当前 [CADCore-FreeCAD 源码同构框架](06-04-23-01-CADCore-FreeCAD源码同构框架调整方案.md) 和 live tree 为准。
- `topoNamingState` DTO、信任边界和前端整包替换语义继续以 [topoNamingState 客户端携带状态接口方案](../接口规定/7-8-11-08-topoNamingState客户端携带状态接口方案.md) 为准。
- expected/ledger 结构、replay 与 validator 细节继续以 [FreeCADCmd 权威账本与 topoNamingState 裁剪原则](7-9-15-53-FreeCADCmd权威账本与topoNamingState裁剪原则.md) 和 [FreeCADCmd Expected Ledger 工具规定](../工具规定/7-9-16-55-FreeCADCmdExpectedLedger工具规定.md) 为准。
- role manifest、parity verdict、divergence registry、current freshness 与 CLI 继续以 [FreeCAD Expected Release Gate 工具规定](../工具规定/7-10-08-10-FreeCADExpectedReleaseGate工具规定.md) 为准。
- `docs/框架/expected/` 下的早期 input/output 完整性文档只作为设计背景；其中 fixture 角色启发式、旧脚本路径或 validator/collector 合并建议与当前工具规定冲突时，以当前 role manifest、ledger 工具和 release gate 为准。

本文件只固定“所有根输入如何收敛到 FreeCADCmd 公共语义等价，同时保留并独立验收 CAD Core 产品扩展”的目标、权威、seam、裁决顺序和完成条件，不复制 ledger schema、DTO 字段或 CLI 的全部实现细节。
