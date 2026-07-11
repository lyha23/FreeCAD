# FreeCADCmd ElementMap 生产者 Trace 驱动 CAD Core 实现指南

## 一句话结论

`expected/<case>.freecad.producer-trace.json` 不是 CAD Core 的输入、也不是最终 public response。它是同一次原生 FreeCADCmd recompute 产生的**生产过程证据**：用来回答“某个 stable name、SID、ElementMap entry、mapper relation 是在哪个 producer stage 首次产生或改变的”。

实现 CAD Core 时，不要拿最终 `Shape`、`topoNamingState` 或 `.freecad.json` 的一个差异直接在 `runtime/`、adapter 或 fixture 分支补丁。应从 producer trace 找到**最早不同的 transaction / scope / checkpoint**，再把缺失语义放回与 FreeCAD 同构的 `app/`、`sketcher/`、`part/` 或 `part_design/`。

本文件以 C4M6 的真实 native case 为例：

```text
fixtures/c4m6/topo-state-body-tip-stable-recovery.json
expected/topo-state-body-tip-stable-recovery.freecad.producer-trace.json
```

当前 trace 有 1 个闭合 recompute transaction、3,016 个 event、27 个 StringHasher table snapshot、134 个 ledger snapshot 和 1 个 mapper snapshot。event 总数不是工作量清单；实现时只沿相关对象的 parent/child scope 和 checkpoint 前进。

## 三种 expected sidecar 的职责

| 文件 | 回答的问题 | CAD Core 能否读取为建模输入 | 用途 |
| --- | --- | --- | --- |
| `*.freecad.json` | 当前 fixture 的 public response 是什么 | 否；它是 oracle | parity 的 public semantic comparison |
| `*.freecad.ledger.json` | public response、输入 state、引用恢复和同次 FreeCADCmd capture 是否闭合 | 否；它是 provenance | expected/ledger strict validator 与 native replay |
| `*.freecad.producer-trace.json` | 每一个内部 ElementMap / SID / mapper producer 在何时读、写、选择、拒绝、继承 | 否；它是只读诊断 oracle | CAD Core 实现定位、producer 对齐与 first-divergence 调试 |

三者都不进入 `DocumentObject graph`，不进入 `topoNamingState`，不参与 shape 构造。`topoNamingState` 仍是客户端携带的旧引用证据；DocumentObject graph 才是建模事实。

`cad-core/tools/collect_freecad_expected.py` 现在默认运行：

```text
/Users/li/Chili3DProject/FreeCAD2/build/relwithdebinfo/bin/FreeCADCmd
```

对能创建原生 Document 并完成 trace drain 的 native fixture，它默认并列写出同一 stem 的三份产物：

```text
expected/<case>.freecad.json
expected/<case>.freecad.ledger.json
expected/<case>.freecad.producer-trace.json
```

trace 会在 native document 关闭前一次性 drain；若当前 FreeCADCmd 不提供 `Document.drainElementMapProducerTrace()`、trace 的 event/scope/snapshot 闭包无效，采集必须失败。`--emit-ledger` 和 `--check-ledger` 保留为兼容参数，但不再控制是否生成或比较 sidecar。

2026-07-12 live corpus 的 480 个 native fixture 均已有 public、ledger 和通过闭包校验的 producer trace，三侧迁移已完成。

request-level preflight rejection 由原生 Document recorder 的 documentless checkpoint 发布稳定 reason，并以 cancel transaction 闭合；raw `Part::GeometryCurve` helper 发布对应的 raw-shape checkpoint。禁止从 rejected public/ledger 反推事件或写空 trace。

数据方向只能是：

```text
fixture graph + optional old topoNamingState
    ├─ FreeCADCmd ──> public expected + ledger + producer trace（Document trace 可用时）
    └─ CAD Core   ──> current response + CAD Core 内部 trace（后续对齐）

producer trace / public expected ──X──> CAD Core 的 shape、命名或 feature 决策
```

trace 允许我们解释 oracle，不能授权 CAD Core 通过 fixture 名、event 序号或 trace 的最终名字回填结果。

## 文件结构：先读什么

顶层字段如下：

```json
{
  "schemaVersion": "freecad.element-map-producer-trace.v1",
  "producer": {"document": "CadCoreExpected"},
  "transactions": [],
  "objectTagIndex": {},
  "objects": {},
  "events": [],
  "stringTableSnapshots": {},
  "ledgerSnapshots": {},
  "mapperSnapshots": {}
}
```

### 1. `transactions`：一次重算的边界

每项的 `eventRange` 是闭合 recompute 的 event 序号区间；`outcome` 必须是 `success`、`exception`、`cancel` 或 `abort`。实现或对比先选目标 transaction，再读其 event range，避免把启动期的引用解析 event 当成 feature producer。

本 case 的核心 transaction 从 `document.recompute.begin` 开始，到 `document.recompute.end` 成功结束。若 CAD Core 的目标是 `Body`，仍必须解释其中被依赖的 `Sketch`、`Pad` 和 Body Origin 对象；不能只看 Body 最后一次赋 Shape。

### 2. `events`：顺序、归属和状态跳转

每个 event 都有：

| 字段 | 含义 | 用法 |
| --- | --- | --- |
| `sequence` | 全局严格递增序号 | 找到最早分叉；不能按数组筛选后重新编号 |
| `transactionSequence` | 所属 recompute transaction，`0` 表示 transaction 外事件 | 限定此次请求的工作范围 |
| `scopeSequence` / `parentScopeSequence` | 嵌套 producer scope | 还原调用树，不能只按 `slice` 名全局聚合 |
| `object` / `objectTag` / `producer` | 当前对象与真实 type context | 路由到 CAD Core 的同构模块；Tag 仅用于本次审计 |
| `slice` | 生产阶段名 | 选择应该补的语义，不是最终 feature 名 |
| `decision` / `reason` | selected、rejected、skipped、exception 等决定及原因 | CAD Core 也必须记录同一类拒绝原因，不能静默 fallback |
| `beforeSnapshot` / `afterSnapshot` | event 前后完整状态引用 | 用来证明“第一处状态改变”，不能只看 delta 文本 |
| `fields` | slice 的 value-only 参数 | 阅读 target、SID、property、candidate 或 checkpoint 标签 |

`objects.<Name>.slices` 是 event 序号索引，适合快速跳到对象视角；真正的父子关系仍以 `scopeSequence` / `parentScopeSequence` 为准。

### 3. 三类 snapshot：比较状态而不是猜结果

| 区域 | 完整状态 | 主要用来回答 |
| --- | --- | --- |
| `stringTableSnapshots` | 已分配 SID 的 `data`、`postfix`、flags、ordered `related` SID refs | SID 是否在正确阶段分配，relation 顺序是否被保留 |
| `ledgerSnapshots` | TopoShape tag、subshape counts、ElementMap entries、child map range、entry-local ElementID refs | 某个 ElementMap entry 是否确实由本 stage 写入/保留/retag |
| `mapperSnapshots` | mapper 输入/输出的 value-only 状态 | M/G/D relation、候选与最终命名之间是否已有足够 producer 证据 |

event 不能引用不存在的 snapshot；ledger 的 SID ref 必须能在 StringHasher table 找到。任何丢 event、SID entry 或 checkpoint 的 trace 都应被 publisher validator 拒绝，而不是“尽量发布”。

## 本 case 的真实 producer 主链

下面的序号来自当前 sidecar，主要用于理解层次；以后原生版本变化导致序号移动时，按 `slice + object + scope parent` 定位，不要把数字写进 CAD Core 代码。

```text
transaction 1
├─ Sketch
│  └─ sketch.producer
│     ├─ SketchObject::buildShape
│     ├─ FaceMaker::postBuild
│     ├─ WireJoiner open-wire / face history handoff
│     ├─ InternalShape checkpoint
│     └─ Shape checkpoint
├─ Pad
│  └─ partdesign.extrude
│     ├─ build_extrusion
│     ├─ makeShapeWithElementMap
│     │  ├─ mapper.snapshot
│     │  ├─ preserve / generated candidate collection
│     │  ├─ candidate rejection or selection
│     │  └─ maker.final_checkpoint
│     └─ PropertyTopoShape Shape handoff checkpoint
└─ Body
   └─ partdesign.body_tip
      ├─ read Pad Tip shape + ledger
      ├─ Shape.setValue(tipShape)
      ├─ Body checkpoint
      └─ inherit only; do not replay Pad producer
```

可在 trace 中看到的关键锚点包括：

| 真实锚点 | 表示的 FreeCAD 行为 | CAD Core 应落实的位置 |
| --- | --- | --- |
| `Sketch / sketch.producer / build_shape` | geometry token、InternalShape、face/open wire 的生产入口 | `cad-core/src/sketcher/sketch_object.cpp`；FaceMaker/WireJoiner 调到 `part/` |
| `face_maker.lifecycle`、`wire_joiner.lifecycle` | closed face、split/history 或 open-wire 结果的交接 | `cad-core/src/part/face_maker.cpp`、WireJoiner 对应 `part/` 实现 |
| `Pad / partdesign.extrude / build_extrusion` | Profile→Pad 的特征控制流 | `cad-core/src/part_design/feature_extrude.cpp` 与 `feature_pad.cpp` |
| `maker.begin` → `mapper.snapshot` → `maker.generated` / `maker.select` → `maker.final_checkpoint` | mapper relation 先产生，ElementMap 后选择并写入 | `cad-core/src/part/topo_shape.cpp`、mapper/history 所在 `part/`；不能在 runtime 伪造 final name |
| `property_shape.set_value_checkpoint` | Shape property handoff 后 ledger 的边界 | property/NamedShape 传播路径，不能只复制 OCCT shape |
| `Body / partdesign.body_tip / tip_ledger_only` | Body 继承 Tip 当前 ledger，不重新执行 Pad | `cad-core/src/part_design/body.cpp` |

这个 case 中 Body scope 的 `tip=Pad`、`tip_ledger_only`、`tip_inherited_without_replay` 是很重要的负面约束：如果 CAD Core 在 `Body::execute()` 再次运行 Pad/mapper，最终 Shape 即使暂时相同，SID、ElementMap、history 与后续引用恢复也会漂移。

## 实现 CAD Core 的固定工作法

### Step 1：先定义此次要对齐的 producer 边界

从 `.freecad.json` / `.freecad.ledger.json` 确认 public contract，从 trace 选择一个完整 producer scope。例如本 case 不应定义成“让 Body 的 Face1 出现”，而应定义成：

```text
Sketch 的 InternalShape/Face evidence
  -> Pad 的 FeatureExtrude + mapper ElementMap producer
  -> PropertyTopoShape handoff
  -> Body 仅继承 Pad Tip 的 ledger
```

若目标 scope 同时缺 Sketch 与 Pad 证据，先完成更靠前的 Sketch scope；不要以 Body 的 final checkpoint 反推 Sketch token。

### Step 2：把 FreeCAD scope 映射到同构模块，而不是 JSON adapter

| Trace 首个分叉 slice | 优先检查 CAD Core | 不应放置修复的位置 |
| --- | --- | --- |
| `reference.resolve` / `reference.update` | `app/` 的 Link/Property 语义，随后才是 `runtime/` 聚合 | adapter JSON 特判、fixture 名判断 |
| `sketch.producer` | `sketcher/`；底层 face/wire/history 归 `part/` | `runtime/recompute.cpp` 直接构造 InternalFace |
| `face_maker.lifecycle` / `wire_joiner.lifecycle` | `part/face_maker.*`、WireJoiner、TopoShape history | Pad/Pocket 的输出补丁 |
| `hasher.*` / `mapped_name.parse` / `element_map.*` | StringHasher、NamedShape/ElementMap 的正式 request-local存储 | 仅在 response 中 canonicalize 字符串 |
| `mapper.*` / `maker.*` / `refine.*` | `part/` 的 TopoShape、mapper/history、ShapeFix | `part_design/` 或 runtime 重写 mapped name |
| `property_shape.*` | shape property 和 NamedShape/ElementMap handoff | 只复制 `TopoDS_Shape` |
| `partdesign.extrude` / `dressup` / `pattern` | `part_design/feature_*.cpp` | `Body` 重放 feature |
| `partdesign.body_tip` | `part_design/body.cpp` 的 Tip 继承 | adapter 或 `runtime/` 再运行上游 producer |

每个语义改动相邻 C++ 注释都应注明实际 FreeCAD 文件、类/函数和关键调用。例如本 case 至少应追到：

```text
/Users/li/Chili3DProject/FreeCAD2/src/Mod/Sketcher/App/SketchObject.cpp::buildShape()
/Users/li/Chili3DProject/FreeCAD2/src/Mod/Part/App/FaceMaker.cpp::postBuild()
/Users/li/Chili3DProject/FreeCAD2/src/Mod/Part/App/WireJoiner.cpp
/Users/li/Chili3DProject/FreeCAD2/src/Mod/Part/App/TopoShapeExpansion.cpp::makeShapeWithElementMap()
/Users/li/Chili3DProject/FreeCAD2/src/Mod/PartDesign/App/FeatureExtrude.cpp
/Users/li/Chili3DProject/FreeCAD2/src/Mod/PartDesign/App/Body.cpp::execute()
```

### Step 3：比较 checkpoint，找“第一处状态差”

不要先比较最终 event 或最终 `.freecad.json`。对同一个 CAD Core scope，按下列顺序比对：

1. scope 是否进入，parent scope 是否相同；
2. `beforeSnapshot` 是否表示相同的上游状态；
3. 第一条改变 table / ledger / mapper 的 event 是什么；
4. 该 event 的 `decision` / `reason` 是否一致；
5. `afterSnapshot` 的 entry、SID refs、child range、mapper relation 是否一致；
6. scope 的 final checkpoint 是否一致；
7. 最后才看 public `topoNamingState`、reference updates 与 mesh。

推荐记录一个实现 ticket 的最小证据表：

| 项 | FreeCAD trace | CAD Core trace / state | 结论 |
| --- | --- | --- | --- |
| producer scope | `Pad / partdesign.extrude` | `FeatureExtrude` scope | 是否进入同一业务分支 |
| first divergent event | `mapper.generated` | 缺失 / relation 不同 | 要补 mapper 还是 choice rule |
| before checkpoint | ledger/table ID | CAD Core value snapshot | 上游还是本 stage 引入差异 |
| decision + reason | `selected / sorted_name_key` | 当前决策 | 禁止把 rejected/selected 混成一条 map |
| final checkpoint | ledger + mapper + table | CAD Core 对应状态 | producer 是否真正闭合 |
| public projection | `topoNamingState` / updates | CAD Core response | 只能在底层证据齐全后处理 |

### Step 4：维护两种名字，不能混用

- 原生 `mappedName.raw`、`StringID`、`ElementIDRefs`、Tag 与 child map range 是 producer-local证据。它们在 trace 中必须按顺序保留，供审计和 mapper/ElementMap 闭包使用。
- `mappedName.canonical` 是跨 producer / expected comparison 的稳定比较键；它不能替代原生 raw token 的解析输入，也不能由 `stableSubname` 或 display path 伪造。
- object Tag、TopoShape Tag 以及 raw token 内运行期 `:H...` 片段可以在跨进程比较时做明确的 canonical projection，但 sidecar 必须保留原始值。不要因这些运行期值变化删除 SID ref、child map 或 mapper evidence。

因此，CAD Core 的目标不是让字符串字节恰好像这一次 FreeCADCmd 一样，而是同时满足：

```text
producer-local raw/table/ledger 闭合
  + 同一 entry 的 canonical identity 一致
  + source/target/history 关系一致
  + public topoNamingState 与引用恢复一致
```

### Step 5：先补低层，再发布 runtime

固定实现次序如下：

```text
StringHasher / ElementMap entry-local refs
  -> TopoShape / mapper history / child maps
  -> Sketch + FaceMaker + WireJoiner
  -> FeatureExtrude / DressUp / Pattern
  -> PropertyTopoShape handoff
  -> Body Tip inheritance
  -> runtime topoNamingState projection + reference updates
  -> adapter serialization
```

如果 trace 的 `maker.candidate.reject` 或 `maker.select` 已经不同，不能先在 `runtime/topo_naming_state.cpp` 选择一个“看起来对”的 stableSubname。若 mapper snapshot 已不同，先修 mapper relation / input ordering；若 mapper 一致而 property handoff 后 ledger 才不同，修 shape property 与 NamedShape copy/retag；若 Body scope 才不同，检查 Tip alias 是否重放 producer。

## 对本 Body Tip case 的最小实现拆分

建议以以下四个可独立验收的实现项推进，而不是一次性比 3,016 event：

1. **Sketch producer**：从四条 LineSegment 得到与 native 相同的 InternalShape / FaceMaker / WireJoiner evidence；验证 `sketch.producer` final checkpoint 的 element inventory 和 entry-local refs。
2. **Pad producer**：`FeatureExtrude` 消费正确 Profile，`part/` 先产生 mapper M/G evidence，再选择并写入 Pad ElementMap；验证 `mapper.snapshot`、`maker.final_checkpoint` 与 table checkpoint。
3. **Shape property handoff**：Pad 的 Shape/NamedShape/ElementMap 经 property 传播、retag 与 child map 保持不丢失；验证 `property_shape.set_value_checkpoint`。
4. **Body Tip**：Body 读取 Pad Tip 的已经完成的 shape ledger，赋自己的 Shape 并发布 Body checkpoint；验证只出现 inheritance，没有 Pad producer 的嵌套 replay。

每项都要至少有一个 CAD Core focused unit/integration test：assert 具体的 inventory、map/history relation、scope closure 或 reference recovery，而不是只 assert `volume`、`FaceN` 数量或最后 JSON 非空。

## 诊断与验收命令

先快速浏览 trace，不手改其内容：

```bash
export TRACE=/Users/li/Chili3DProject/FreeCAD/cad-core/fixtures/c4m6/expected/topo-state-body-tip-stable-recovery.freecad.producer-trace.json

python3 - <<'PY'
import json, os
t = json.load(open(os.environ["TRACE"]))
print(t["transactions"])
for event in t["events"]:
    if event["slice"] in {
        "sketch.producer", "partdesign.extrude", "mapper.snapshot",
        "maker.final_checkpoint", "partdesign.body_tip",
    }:
        print(event["sequence"], event["object"], event["slice"],
              event["decision"], event["reason"])
PY
```

在 CAD Core 修改后，按层验证：

```bash
# 0. 重新运行原生 collector，并检查 public/ledger 与已入库 trace 闭包。
cd /Users/li/Chili3DProject/FreeCAD
python3 cad-core/tools/collect_freecad_expected.py \
  cad-core/fixtures/c4m6/topo-state-body-tip-stable-recovery.json \
  --check --validate-ledger

cd /Users/li/Chili3DProject/FreeCAD/cad-core

# 1. 当前实现与 topoNamingState 的 focused 行为。
python3 -m unittest tests.test_topo_naming_state_response

# 2. 生成/比较该 native case 的当前 CAD Core 输出；实际命令参数以现有工具 --help 为准。
python3 tools/compare_freecad_expected.py \
  --phase c4m6 \
  --case topo-state-body-tip-stable-recovery \
  --release-gate --run-contract-tests

# 3. public expected 与 authority ledger 的既有闭包。
python3 tools/validate_freecad_expected_ledger.py --phase c4m6 --strict
```

producer trace 的 validator 与 public expected ledger validator 是两套门禁：前者验证 event/scope/transaction/snapshot/SID 闭包，后者验证 fixture、public response、reference recovery 与 provenance 闭包。两者都通过，才说明“内部 producer 证据”与“对外 topoNamingState”没有断链。

## 不该做的事

- 不把 `producer-trace.json` 回传给 CAD Core 当作 state、cache 或 geometry input。
- 不按 fixture 名、object ID、raw Tag、event sequence、`FaceN` 或本 case 的 `g1;SKT;...` token 写分支。
- 不把 raw mapped name 的运行期 hash 差异直接认定为 geometry semantics 差异，也不因此忽略 SID refs、child ranges 或 mapper relations。
- 不为了让最后 `topoNamingState` 相等，在 runtime/adapter 伪造 ElementMap、mapperHistory 或 Body Tip 结果。
- 不用 Body 重放 Pad/Pocket/DressUp producer；Body 是 Tip ledger 的继承者。
- 不手改 `.freecad.json`、`.freecad.ledger.json` 或 `.freecad.producer-trace.json` 来让测试变绿；oracle 只能由相应 native collector 重生。

## 完成一个 trace 驱动实现项的定义

一个 CAD Core 实现项只有同时满足以下条件才可关闭：

1. 指出了 FreeCAD 源文件、函数、scope 和 CAD Core 同构落点；
2. CAD Core 在该 scope 前的 input state 与后续 checkpoint 都可审计；
3. 第一处分叉已在正确低层修复，而不是由 runtime/adapter 盖住；
4. ElementMap 的 entry-local SID refs、child maps、mapper history 与 rejection reason 没有被丢弃或降级猜测；
5. Body / Pattern / DressUp 等高层 scope 不重放已经完成的上游 producer；
6. focused test、C4M6 public parity、ledger strict validator 和相关 producer-trace closure 验证都通过；
7. 未修改 native expected 来追随 CAD Core 当前输出。

这套方法的目标是把“最终 Face 名不同”的大问题收敛成可验证的小问题：**哪一个 producer scope 的哪一个 checkpoint 首次不同，以及该语义应落在 CAD Core 的哪个同构模块。**
