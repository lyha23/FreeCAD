# FreeCADCmd expected 账本闭包验收原则

## 结论

`fixtures/<phase>/expected/*.freecad.json` 是对外协议 expected；同名 `.freecad.ledger.json` 是 FreeCADCmd 的 provenance sidecar；`.freecad.producer-trace.json` 是原生 ElementMap producer 的只读过程证据。

裁剪原则是：不要把 ledger 或 producer trace 塞回 `.freecad.json`。public expected 只保留前端协议需要的投影；ledger 解释 public/provenance 闭包；trace 解释 SID、ElementMap、mapper 和 feature stage 的生产过程。

validator 的职责变成证明两件事：

- `.freecad.json` 与 `.freecad.ledger.json` 通过 hash 和 case 元数据绑定。
- `.freecad.json` 中发布的 `topoNamingState` 可以由 ledger 的对象、事件、投影、覆盖和 round-trip 证据解释。

collector 的数据方向固定为：FreeCADCmd 先产生同次 capture，再分别生成 public expected 与 ledger。capture 明确分为 `rawTopoNamingState`、`publishedTopoNamingState` 和 `resolvedReferenceBindings`：属性写入成功先标为 assigned，recompute 后还必须由 FreeCAD `getSubObject` / `Shape.getElement` 或 raw topo 证据确认 resolved，才允许进入 capture。事件只接受 raw 状态与该成功绑定，公开投影来自 published 状态。accepted ledger 不允许从已经落盘的 expected 反推；capture 与 public `topoNamingState` 不一致时采集直接失败。

producer trace 在原生 Document 关闭前通过 `drainElementMapProducerTrace()` 一次性取出。recorder 是只读旁路；trace 不参与 shape、StringHasher 分配、ElementMap 决策、`topoNamingState` 或 CAD Core 输入。

因此，public/ledger 验收重点是检查权威账本与 public 投影之间不断链：

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

producer trace 是按需参考的独立诊断 sidecar。只有 public/ledger 无法对齐且需要定位原因，或任务明确要求 producer 审计时，才检查其 transaction/scope/checkpoint；缺 trace 不改变 public/ledger release verdict，不能以空 trace 或从 public output 反推的事件补齐。

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
python3 cad-core/tools/collect_freecad_expected.py \
  --phase c4m6 \
  --check \
  --validate-ledger \
  --skip-unsupported
```

collector 默认使用 `/Users/li/Chili3DProject/FreeCAD2/build/relwithdebinfo/bin/FreeCADCmd`。`--check` 默认比较 regenerated public expected 与 ledger；`--validate-ledger` 验证本次内存结果。trace 仅在 public/ledger 差异定位或显式 producer 审计中单独验证，不是普通 replay 的通过条件。

`--emit-ledger` 与 `--check-ledger` 仅为兼容参数。独立 validator 只读 checked-in public/ledger；collector replay 才重新运行 FreeCADCmd。普通 phase replay 不因 trace 缺失而 fail closed。

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

现在保留四个清楚的层次：

- `collect_freecad_expected.py --check`：证明 public 与 ledger 可复现，并验证已有 trace 的结构闭包。
- `validate_freecad_expected_ledger.py`：证明 checked-in `.freecad.json` 与 `.freecad.ledger.json` 的权威账本闭包成立。
- producer trace 审计：按 transaction/scope/checkpoint/snapshot 找 CAD Core 与 FreeCAD 的 first divergence，不把 trace 变成 public comparator。
- 其它 coverage 测试：证明 expected corpus 覆盖了哪些业务形态。

不要把这些证据混成更厚的对外 schema。`.freecad.json` 保持 public 投影，ledger 承担 provenance，producer trace 承担内部生产过程；CAD Core response 只按 public 语义裁决。

静态 ledger validator 不能仅凭 `producer.name=FreeCADCmd` 证明外部进程真的运行过；native 来源由 live `--check` 补足。ledger v1 仍是 Python/public DTO 证据，原生私有 producer 过程只由 trace 描述。

输入 fixture 为兼容回放而携带的 `mapperHistory` 可以继续出现在 public `topoNamingState`，但不能被 ledger 重新标记成本次 FreeCADCmd 产生的 native event；native event 只从 `rawTopoNamingState` 提取。

2026-07-12 live corpus 有 480 对 `.freecad.json` / `.freecad.ledger.json`：470 accepted、10 rejected；480 个 native fixture 也均已有通过闭包校验的 producer trace。10 个 request-level rejected 用例由原生 documentless checkpoint 记录稳定 reason，并以 cancel transaction 闭合。

同日用默认 FreeCAD2 binary replay Body/Tip case，public 与 trace 生成成功，但 ledger check 因 producer revision 从 `20260519` 漂到 `46970` 返回 1。当前只能称 artifact 存在，不能称新 producer 基线已可复现。
