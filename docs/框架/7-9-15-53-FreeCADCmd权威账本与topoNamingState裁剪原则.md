# FreeCADCmd expected 账本闭包验收原则

## 结论

`fixtures/<phase>/expected/*.freecad.json` 是对外协议 expected；同名 `fixtures/<phase>/expected/*.freecad.ledger.json` 是 FreeCADCmd / native oracle 生成的权威账本 sidecar。

当前裁剪原则是：不要把完整内部账本塞回 `.freecad.json`，也不要在对外 expected 里新增 `oracleMetadata`、`topologyInventory`、`referenceLedger` 这类厚块；`.freecad.json` 只保留前端协议需要的 public `topoNamingState` 投影，完整证明放在同名 `.freecad.ledger.json`。

validator 的职责变成证明两件事：

- `.freecad.json` 与 `.freecad.ledger.json` 通过 hash 和 case 元数据绑定。
- `.freecad.json` 中发布的 `topoNamingState` 可以由 ledger 的对象、事件、投影、覆盖和 round-trip 证据解释。

collector 的数据方向固定为：FreeCADCmd 先产生同次 capture，再分别生成 public expected 与 ledger。capture 明确分为 `rawTopoNamingState`、`publishedTopoNamingState` 和 `resolvedReferenceBindings`：属性写入成功先标为 assigned，recompute 后还必须由 FreeCAD `getSubObject` / `Shape.getElement` 或 raw topo 证据确认 resolved，才允许进入 capture。事件只接受 raw 状态与该成功绑定，公开投影来自 published 状态。accepted ledger 不允许从已经落盘的 expected 反推；capture 与 public `topoNamingState` 不一致时采集直接失败。

因此，验收重点是检查 sidecar 权威账本与 public 投影之间不断链：

- `topoNamingState.objects[*].subshapes`
- `topoNamingState.objects[*].elementMap.entries`
- `topoNamingState.objects[*].childElementMaps`
- `topoNamingState.objects[*].mapperHistory`
- `elementReferenceUpdates[*].StableSubList`
- `diagnostics` 对 split / deleted / ambiguous 的解释
- `*.freecad.ledger.json` 的 `inputReferences`
- `*.freecad.ledger.json` 的 `objects`
- `*.freecad.ledger.json` 的 `events`
- `*.freecad.ledger.json` 的 `projection`
- `*.freecad.ledger.json` 的 `coverage`
- `*.freecad.ledger.json` 的 `roundTrip`
- `*.freecad.ledger.json` 的 `fixture.expectedPayloadHash` 与 `fixture.topoNamingStateHash`

当前 checked-in native expected 要求每个 `*.freecad.json` 都有同名 sidecar；缺少 `*.freecad.ledger.json` 是 hard fail。

## 验收入口

独立 validator 入口：

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
python3 tools/validate_freecad_expected_ledger.py --phase c4m6 --strict
```

对全部 expected 执行 strict 闭包门禁：

```bash
python3 tools/validate_freecad_expected_ledger.py --all --strict
```

如果要同时证明 checked-in expected 能由当前 FreeCADCmd collector 复现，单独运行 native collector check：

```bash
cd ~/Chili3DProject/FreeCAD
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd \
  python3 cad-core/tools/collect_freecad_expected.py \
  --phase c4m6 \
  --check \
  --check-ledger \
  --validate-ledger \
  --skip-unsupported
```

`--check` 比较 regenerated public expected；`--check-ledger` 比较 regenerated ledger 的 producer / objects / events / projection / coverage / round-trip 语义；与 `--check-ledger` 同用时，`--validate-ledger` 验证的是本次内存生成结果，而不是磁盘上的旧 sidecar。不要把 validator 和 collector 复现混成一个入口：前者只验证 checked-in expected 与 sidecar ledger 的闭包，后者才负责重新调用 FreeCADCmd 证明两份 artifact 可复现。

## sidecar 最小结构

`*.freecad.ledger.json` 至少应提供这些权威证据：

- `schema = freecad-toponaming-ledger/v1`
- `producer.name = FreeCADCmd`，并记录 FreeCAD / OCCT / collector 版本
- `fixture.phase`、`fixture.case`、`fixture.inputHash`
- `fixture.expectedPayloadHash`
- `fixture.topoNamingStateHash`
- `inputReferences`
- `objects`
- `events`
- `projection.publishedObjects`
- `projection.droppedObjects`
- `coverage.coveredInputReferenceIds`
- `coverage.uncoveredInputReferenceIds`
- `roundTrip.status`
- `roundTrip.inputTopoNamingStateHash`

## 这个脚本证明什么

它只读 `fixtures/<phase>/expected/*.freecad.json` 和同名 `*.freecad.ledger.json`，不依赖 `cad-core` runtime，也不读取 `cad-core` 内部 `named_shapes`。

它证明的是 checked-in expected artifact 与 FreeCADCmd 权威账本 sidecar 闭合：

- 每个 `.freecad.json` 都有同名 `.freecad.ledger.json`。
- ledger schema 必须是 `freecad-toponaming-ledger/v1`。
- ledger producer 必须是 `FreeCADCmd`。
- strict 模式下 producer 需要带 `freecadVersion`、`occtVersion`、`scriptVersion`。
- `fixture.expectedPayloadHash` 必须匹配 `.freecad.json` 的规范化 hash。
- 如果 expected 带 `topoNamingState`，`fixture.topoNamingStateHash` 必须匹配对应子树 hash。
- `fixture.phase`、`fixture.case` 必须匹配 expected 路径，`fixture.inputHash` 必须匹配同 phase 的输入 fixture。
- 输入 fixture 自带的 `topoNamingState` 必须通过与 collector 相同的 schema、producer、`documentHash`、object owner、`objectHash` 前置校验；无效输入只能绑定对应 diagnostics-only rejected ledger，不能继续宣称 accepted。
- ledger `inputReferences` 必须与输入 fixture 中独立抽取的 `StableSubList` 引用逐项一致，id 必须唯一。
- accepted ledger 必须声明 `inputReferences`、`objects`、`events`、`coverage`、`projection`、`roundTrip`。
- rejected ledger 必须用 `rejection.diagnosticCodes` 覆盖 expected diagnostics。
- event 只能引用已声明的 input reference；terminal event 集、coverage 集必须精确一致，`coverage.uncoveredInputReferenceIds` 必须为空。
- event 的 endpoint 还必须与其所声明 input reference 的 owner/target 及 element-map/mapper-history 元素证据逐项匹配；不能把合法 ref id 挂到无关事件上凑 coverage。
- `projection.publishedObjects` 必须与 public `topoNamingState.objects` 的 key 集完全一致，且每个 public object 必须指向同名 ledger object 并覆盖自身。
- public object 的 `objectHash` 必须绑定输入 fixture object；`elementMap`、`childElementMaps`、`mapperHistory` 与 subshape inventory 必须由 ledger evidence 解释。无法由 element map 唯一反查的稳定子形状必须落入 collector 生成的 `subshapeEvidence`，不能只凭公开 subshape 自证。
- projection 的 `sourceEventIds` 必须指向真实 event。
- 相关但未发布的对象必须在 `projection.droppedObjects` 中说明 dropped reason 和证据。
- dropped object 的 `coveredBy` 必须指向真实 published object。
- `roundTrip.results` 必须与 required input references 一一对应；`roundTrip.status=passed` 时每条引用都必须存在于 replay 同次捕获的成功原生属性赋值绑定中。仅在 replay 输出里回显输入 token 不算 resolved。
- expected 发布 `topoNamingState` 时 ledger outcome 必须是 accepted；rejected outcome 不得绕过 objects / events / projection / coverage / round-trip 闭包。

`--strict` 是 phase-level 覆盖要求，适合 `c4m6` 这种专门的 topoNamingState phase。它要求 corpus 至少覆盖：

- hash 绑定
- producer 版本
- sidecar 权威账本字段
- projection / coverage / roundTrip 证据

## 分层关系

现在保留三个清楚的层次：

- `collect_freecad_expected.py --check`：证明 expected 能由 FreeCADCmd / native collector 复现。
- `collect_freecad_expected.py --check --check-ledger`：证明 ledger 也能由同次 FreeCADCmd capture 复现，而不是只验证旧 sidecar 自洽；FreeCAD / OCCT producer 版本漂移也是失败。
- `validate_freecad_expected_ledger.py`：证明 checked-in `.freecad.json` 与 `.freecad.ledger.json` 的权威账本闭包成立。
- 其它 coverage 测试：证明 expected corpus 覆盖了哪些业务形态。

不要把这三件事混成一个更厚的对外协议 schema。当前最小有效目标是：`.freecad.json` 继续保持裁剪后的 public topoNamingState 投影，`.freecad.ledger.json` 承担 FreeCADCmd 权威内部账本证明，validator 把两者绑定成可验证门禁。

静态 validator 不能仅凭 `producer.name=FreeCADCmd` 证明某个外部进程真的运行过；native 来源必须由 live `--check --check-ledger` 门禁补足。ledger v1 的证据边界是 FreeCADCmd Python 与 public DTO 能导出的对象、映射和历史，不应夸大为 FreeCAD C++ 私有 `NamedShape` / `ElementMap` / MapperHistory 的完整导出。

输入 fixture 为兼容回放而携带的 `mapperHistory` 可以继续出现在 public `topoNamingState`，但不能被 ledger 重新标记成本次 FreeCADCmd 产生的 native event；native event 只从 `rawTopoNamingState` 提取。

当前验收基线：全库 475 对 `.freecad.json` / `.freecad.ledger.json` 通过 `--all --strict`；c4m6 live gate 为 `processed=9 skipped=1 failed=0`，其中跳过的 `topo-state-mapper-history-events.expeted.json` 是明确的 protocol-only contract，不属于 native ledger discovery。

本次闭包修复同时清理了 69 个 accepted fixture 的过期输入 `documentHash`，其中 3 个还需要同步 object-level `objectHash`；全部通过 FreeCADCmd 重生，且迁移前后的 accepted/rejected outcome 保持不变。

所有 152 个带 `inputReferences` 的 native sidecar 已按 raw-event / resolved-binding 规则由 FreeCADCmd 重生，并完成第二次 `--check --check-ledger --validate-ledger` replay：`152/152` 通过、outcome 零漂移。无法由 element map 唯一反查的 `subshapeEvidence` 只从 `rawTopoNamingState` 生成，不从 published 投影回抄。
