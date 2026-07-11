# FGM-N1 CAD Core ElementMap 生产者切片探针方案

## 目标

在 `/Users/li/Chili3DProject/FreeCAD/cad-core` 内实现与 FreeCADCmd producer trace 同构的 request-local 探针。修改后的普通 `cad-core recompute` 无需环境变量、日志等级、编译开关、fixture 名或对象特判，就必须记录全部有用切片、完整 checkpoint、异常/取消与闭包证据，并默认在 public output 旁发布独立 sidecar：

```text
<case>.cad-core.json
<case>.cad-core.producer-trace.json
```

本阶段不实现跨 producer 比较算法；它先保证 CAD Core 自己的 trace 是完整、确定、只读、可验证的。只有 N1 全部门禁通过，N2 才能可靠报告第一处分叉。

## 权威与 live 基线

完整规格：

```text
/Users/li/Chili3DProject/FreeCAD2/docs/添加探针/
  7-11-23-05-FreeCADCmd-ElementMap生产者切片探针方案.md
```

实现时还必须直接核对 FreeCAD2 的当前原生落点：

```text
src/App/ElementMapProducerTrace.{h,cpp}
src/App/StringHasher.{h,cpp}
src/App/ElementMap.{h,cpp}
src/App/Document.cpp
src/App/DocumentObject.cpp
src/App/PropertyLinks.cpp
src/App/GeoFeature.cpp
src/Mod/Part/App/TopoShape{,Expansion,Mapper}.{h,cpp}
src/Mod/Part/App/PropertyTopoShape.cpp
src/Mod/Part/App/FaceMaker.cpp
src/Mod/Part/App/WireJoiner.cpp
src/Mod/Sketcher/App/SketchObject.cpp
src/Mod/PartDesign/App/FeatureExtrude.cpp
src/Mod/PartDesign/App/FeatureDressUp.cpp
src/Mod/PartDesign/App/FeatureTransformed.cpp
src/Mod/PartDesign/App/Body.cpp
```

2026-07-12 live CAD Core 已具备 request-local `StringHasher`、`NamedShape::elementMapEntries`、entry-local `elementIdRefs`、child maps、`MapperHistory` 和 maker/Body/Sketch 等实现路径，但尚无 recorder、完整 const inspection、默认 actual trace sidecar 与闭包测试。当前 worktree 有大量用户修改；方案编写期间还并发出现了 480 个未跟踪 native producer-trace sidecar。S0 必须重新记录 HEAD/status、验证这些新 artifact，并逐文件复用代码改动，不得按本文快照覆盖当前工作区。

## 不可违反的边界

- trace 始终记录。禁止增加 `CAD_CORE_TRACE`、`--trace`、日志等级、CMake option 或测试专用开关。
- trace 是独立诊断 artifact，不进入普通 response、`topoNamingState`、Shape、BREP、mesh、fixture 输入、public expected 或 C ABI 原响应字段。
- recorder 只保存 POD、字符串和 value-only JSON；不持有 `TopoDS_Shape`、`NamedShape*`、`DocumentObject*`、StringID 引用、业务容器迭代器或裸地址。
- snapshot 只能通过专用 const inspection 读取已有状态，禁止调用会分配 SID、写 ElementMap、扩展 history、缓存、排序回写或改变 OCCT 时序的业务函数。
- 不在 OCCT parallel worker callback 中追加全局事件；maker Build 完成后在单线程消费点按原容器顺序发布 mapper snapshot。
- 不增加 fixture/object/`#id` 路由；每个 guard、early return、fallback、duplicate、异常和取消必须记录稳定 `decision` 与 `reason`。
- 已知随机分支必须由 producer 事件源头声明有限 `nondeterminismClass` 和输入推导的 `stableComparisonKey`，raw 值仍保留；不得让比较器事后把任意差异标成可忽略。
- `cad-core/src/app/element_map.cpp` 当前只是 Sketch Internal* 的 App helper，不是通用 NamedShape ledger。通用 `element_map.*` trace 应观察 `part::NamedShape`/TopoShape 正式生产路径，不能借探针把业务账本搬错层。

## 架构决定：一个深 recorder 模块

### 模块与 seam

新增深模块：

```text
cad-core/include/cad_core/app/element_map_producer_trace.h
cad-core/src/app/element_map_producer_trace.cpp
```

它的外部 interface 保持很小：

```cpp
class ElementMapProducerTrace {
public:
    Transaction beginTransaction(TransactionDescriptor);
    Scope scope(ScopeDescriptor);
    void record(EventValue);
    SnapshotId checkpoint(SnapshotValue);
    ValidationResult validate() const;
    TraceDocument drain(ProducerMetadata, ObjectIndex);
};
```

复杂的 sequence、scope stack、parent、snapshot 内容哈希/去重、identity、闭包、自校验和 drain 释放全部隐藏在实现中。producer 调用点只负责提交当下已经存在的值；不得在每个 feature 中复制 sequence、JSON 序列化或闭包逻辑。

`Transaction` 与 `Scope` 必须是 move-only RAII 类型，构造时保存 `std::uncaught_exceptions()`，析构时正常闭合或在异常计数增加时自动标记 exception；外层 catch 再补稳定 category/detail。显式 `exception()`、`cancel()`、`abort()` 记录 outcome 和 detail。非 LIFO scope、重入和 transaction 关闭时仍有开放 scope 都是 trace failure 事件，最终 validator 必须 hard fail 或明确 abort，不能静默纠正。

### 所有权与调用链

CAD Core 的 `app::Document` 是解析后的值对象，不具备原生 FreeCAD Document 生命周期。因此等价所有权放在一次 request/recompute run：

1. `runtime::recompute` 在任何 topo-state preflight 之前创建 recorder。
2. `ComputeContext` 持有同一个 request-local recorder，并在创建 `StringHasher` 时把非 owning trace handle 关联给它。
3. `StringHasher`、NamedShape/TopoShape producer、Sketch、PartDesign feature 都向同一个 recorder 写值事件。
4. parsed request 即使在 topo-state preflight、依赖、引用、executor 或 OCCT 阶段失败，也关闭一个 failure/abort transaction 并可发布 sidecar。
5. 纯 JSON 语法错误发生在 Document graph 尚未形成之前，继续作为 adapter parse error；它不是 ElementMap producer transaction。
6. core run 返回 `response + frozen trace` 两份 artifact；CLI adapter 只负责把二者写到不同文件。

recorder 本身仍支持 multi-transaction buffer，供一次测试/collector inspection session 在 drain 前记录正常、空、异常或取消 transaction；但这不建立后端 session。跨 transaction 只能保留 recorder 的 POD 证据，`ComputeContext`、Shape、NamedShape、StringHasher 和 Document graph 仍按无状态请求规则重新创建，绝不能把第一次几何作为第二次建模输入。普通 CLI 的一个 parsed request 通常只产生一个 transaction。

为消除当前 CLI 与 `runtime::recompute()` 重复的 preflight/recompute 分支，建议在 `runtime/recompute.{h,cpp}` 增加一个统一结果：

```cpp
struct RecomputeArtifacts {
    nlohmann::json response;
    nlohmann::json producerTrace;
    ComputeContext context; // CLI shape export 继续消费同一次 run，不重算
};
```

实际实现可以用 move-only holder 减少复制，但必须保证 CLI export、public response 和 trace 来自同一次执行。现有 `recompute()`/C ABI 仍只返回 public response；不得把 `producerTrace` 塞入旧 response。C ABI 的独立 trace publisher 可在后续增加新函数，本阶段不修改既有响应合同。

### trace handle 传播

- `ComputeContext` 是 request ownership seam。
- `StringHasher` 可保存由 owner 注入、生命周期更短或相等的 non-owning recorder handle；recorder 绝不反向持有 hasher。
- `NamedShape` 已携带共享 `StringHasher`。低层 Part producer优先从同一 request-local hasher 取得 recorder，避免给所有 maker 函数增加平行的 tracing 参数。
- 高层 object/stage scope 仍由 `ComputeContext` 显式建立，不能用 thread-local“最近对象”猜 owner。
- 无法立即解析 owner 的低层 event 保存 `objectTag`、shape role 和 `unresolvedOwner=true`；run 结束时只能用 Document object ID/index 回填。

## schema 与 sidecar

CAD Core 使用与 native trace 相同的顶层结构和 slice vocabulary：

```json
{
  "schemaVersion": "freecad.element-map-producer-trace.v1",
  "producer": {
    "name": "CADCore",
    "document": "...",
    "build": "...",
    "inputSha256": "...",
    "responseSha256": "..."
  },
  "transactions": [],
  "objectTagIndex": {},
  "objects": {},
  "events": [],
  "stringTableSnapshots": {},
  "ledgerSnapshots": {},
  "mapperSnapshots": {}
}
```

沿用该 schema 名是为了表示“实现同一份 comparison contract”，实际生产者由 `producer.name` 区分。不得把 CAD Core event 写进 native expected 文件。

CLI 输出路径规则固定为：

- `foo.cad-core.json` → `foo.cad-core.producer-trace.json`；
- 其他 `foo.json` → `foo.cad-core.producer-trace.json`。

不提供路径开关。测试或临时运行要把 `--output` 本身指向 `/tmp`，两个 artifact 自然并列写入 `/tmp`。

publisher 在写盘前先冻结并 validate trace；验证、序列化、临时文件写入或 rename 任一步失败，CLI 返回非零并删除本轮临时 trace。正式 output 与 trace 必须由同一次 run 生成，不能失败后用上一次 sidecar 冒充。

`inputSha256` 和 `responseSha256` 绑定同一次 parsed request 与 public response；哈希对象是 sorted-key/stable-separator 的 canonical JSON，不是受 pretty-print 影响的文件字节。它们只存在于 trace provenance，不进入 public response。N2 必须先验证这个绑定再比较过程。native trace 与 `.freecad.ledger.json` 的 trace hash/recompute sequence 绑定仍由 native collector 负责。

## event、snapshot 与 identity

### event

每条 event 至少包含：

```text
sequence
transactionSequence
scopeSequence / parentScopeSequence
object / objectTag / producer
slice
decision / reason
beforeSnapshot / afterSnapshot
fields
```

`fields` 保存 source/target IndexedName、raw/canonical mapped name、ordered SID refs、operation、candidate ordinal、guard、tag role、shape role等 value-only 参数。event 发生时立即复制，不允许 drain 时重新查询业务对象。

### snapshot

必须实现三类完整快照：

| snapshot | 必须包含 |
| --- | --- |
| StringHasher | `lastId`、每个 ID 的 data/postfix/flags、PrefixID/PrefixIDIndex、ordered related refs、mapped-name lookup 元数据 |
| ledger | owner/role、producer Tag、shape type、V/E/F IndexedName 顺序、按 IndexedName 分组的 ordered entries、每条 entry 自己的 refs、reverse lookup 标记、child ranges、mapper/history、未命名元素、collision |
| mapper | source/output shape role、完整 Generated/Modified/Deleted adjacency、target 顺序、split/merge/ambiguous 与 query 结果 |

snapshot ID 使用 `kind:sha256:<canonical-payload>` 内容寻址。哈希只用于去重和本文件完整性；不能替代 payload。每个 event 的 before/after 引用必须存在。

### const inspection

新增或扩展的 inspection interface 只返回值：

```text
app::StringHasher::inspectProducerTraceState() const
part::inspectNamedShapeLedger(const NamedShape&)
part::inspectMapperHistory(...)
part::inspectShapeInventory(const TopoDS_Shape&)
```

这些函数不得调用 `getId()`、`hashMappedName()`、`recordElementMapEntry()`、history expansion 或任何会更改缓存/顺序的方法。对 `TopoDS_Shape` 只读取 type、orientation、V/E/F inventory 和当前 IndexedName 顺序，不保存 shape。

当前 `StringHasher::ids_` 是 lookup/alias map，无法在 drain 时可靠反推出每个 SID 的原始 data/postfix/flags 与分配顺序。实现必须在真实 allocation/insert 点同步维护一份按 SID 单调追加的 inspection POD table；业务 lookup 继续使用原结构，snapshot 只读 POD table，禁止从最终 mapped name 猜回 StringID。

### identity

recorder 为第一次看到的 hasher、ledger/map、shape role 和 mapper 分配 monotonic trace identity，并记录 `create/copy/share/reset/drop` 关系。identity 来自调用点提供的生命周期事件，不得以地址或 `TopoDS_Shape::HashCode()` 生成。跨 FreeCAD/CAD Core 比较时由 N2 根据 scope/role/input ordinal 建双射。

## 必须覆盖的切片

完整逐项矩阵见 `矩阵/producer_trace_slice_matrix.tsv`。以下组全部是 N1 完成范围，不能只实现前几组：

| 组 | CAD Core 主要落点 | 要证明的事实 |
| --- | --- | --- |
| 调度/引用 | `runtime/recompute.cpp`、`graph/recompute_plan.cpp`、`app/link.cpp`、reference resolution | target/dirty/order、真实 object execute、解析前后 owner 与 failure |
| StringHasher | `app/string_hasher.{h,cpp}` | 普通/映射字符串命中与分配、PrefixID、ordered refs、table checkpoint |
| ElementMap/ledger | `part/topo_shape.cpp`、必要的 `app/element_map.cpp` helper | find/findAll、encode/write/erase/reset/drop/history、entry-local refs、child map |
| TopoShape/mapper/maker | `part/topo_shape.cpp`、`topo_shape_expansion.cpp`、`topo_shape_mapper.cpp` | raw M/G/D 先于消费，preserve、singular M/G、全部 reject/select/K/U/L 与 final checkpoint |
| Part helpers | `part/face_maker.cpp`、`wire_joiner.cpp`、Boolean/Refine/ShapeFix | split/open wire/history 与 mapper handoff，而非最终几何猜测 |
| Sketch | `sketcher/sketch_object*.cpp`、`sketch_internal_result.*` | g<ID>;SKT、Shape/InternalShape、FaceMaker/WireJoiner 与 slot 顺序 |
| Property/PartDesign | `part/property_topo_shape.cpp`、`feature_extrude.cpp`、`feature_dress_up.cpp`、`feature_transformed.cpp`、`body.cpp` | setValue/retag/slot、Pad/Pocket、DressUp、Pattern、Body Tip 只继承 |
| failure/cancel | 所有 scope | guard、early return、异常、取消、partial write 和最后 checkpoint |

尤其必须锁住：

- preserve 使用 `findAll()` 的有序 entry 链；
- M/G 使用 incoming shape 的 singular mapped name 和该 entry 的 refs；
- raw mapper snapshot 在命名消费之前发布；
- 每个 target 的候选、排序字段、拒绝原因和胜者都存在；
- K/K0/K00、multi-source tuple、parallel/coplanar、delayed 第二轮、U/L 不得折叠；
- Body Tip 只发布 tip ledger inheritance/slot handoff，不得重放 Pad/Pocket/DressUp producer。

## 闭包 validator

validator 是 recorder 深模块的一部分，Python 工具再做独立复核。任一失败必须 hard fail：

1. event sequence 连续，transaction range 精确、无重叠/空洞；
2. scope/transaction 正常闭合或明确 exception/cancel/abort，parent 存在且无环；
3. `objects[*].slices` 能回指唯一全局 event，objectTag 最终有 object/type 双射；
4. before/after 与 nested snapshot 引用存在，内容 hash 与 canonical payload 一致；
5. 每个 SID ref 在该事件时点已经分配，ordered entry-local refs 未被 raw-name 全局 relation 覆盖；
6. resolved target 存在于对应 output inventory；child offset/count/range 不越界；
7. mapper source/target 能回到对应 shape snapshot；
8. producer 正常、异常或取消关闭前有最后完整 checkpoint，并声明 partial write；
9. drain 后已发布记录释放，第二次 drain 为空；
10. 人为删除 event、SID、snapshot、checkpoint 或 scope end 必须失败。
11. `inputSha256` / `responseSha256` 与本次实际 artifact 不一致必须失败，不能比较错配 run。

## 实施步骤

### S0：live baseline 与 contract 冻结

- 重新记录 HEAD、log、`status --short -uall`；保护全部用户改动。
- 对照当前 native JSON，冻结字段类型、slice vocabulary、stable reason 枚举和 sidecar 命名。
- 盘点当前 StringHasher/NamedShape/mapper/PartDesign live 改动；禁止按旧 HEAD 重写。
- 明确 C4N-S3 与 N1 不并行修改相同文件。

### S1：recorder、run ownership 与最小闭包

- 新增 `app::ElementMapProducerTrace`、RAII transaction/scope、sequence/parent/object index、snapshot hash/dedup、identity、reentrancy 与 drain。
- `ComputeContext`/统一 recompute run 从 preflight 前持有 recorder。
- 先用纯 C++ focused tests证明 multi-transaction、空 transaction、exception/cancel、drain 释放。

### S2：调度、引用、StringHasher 与 ElementMap

- 接 `document.recompute`、object execute、blocked/skipped/unsupported/OCCT failure、reference resolve/update。
- 为 StringHasher 增加完整 const inspection，并覆盖所有影响后续 ID 的普通/映射分配。
- 在正式 NamedShape ledger 路径接 find/findAll、encode/write、erase/reset/drop/history、child map；保持每条 entry 自己的 ordered refs。

### S3：TopoShape、raw mapper 与 maker 状态机

- 接 set/canMap/copy/map 与对应完整 checkpoint。
- 在 OCCT Build 后、命名消费前发布完整 raw M/G/D mapper snapshot。
- 接 preserve、M/G、全部 guard/reject、NameKey sort、K/tuple、parallel/coplanar、delayed、U/L、final checkpoint。
- 接 Boolean、Common、Refine、ShapeFix 与失败回退。

### S4：Sketch、FaceMaker 与 WireJoiner

- 接 geometry token、construction/external filtering、raw Shape、InternalShape、profile handoff。
- FaceMaker 记录 pre-split/splitter/combo/namesUsed；WireJoiner 记录 split/merge/superEdge、open/result wires 和最终 history adjacency。
- self-intersection、inter-edge split、bounded face 与 open wire 必须分别有证据，不能在 sketch executor 猜 ownership。

### S5：Property、Extrude、DressUp、Pattern 与 Body

- 接 PropertyTopoShape setValue/reTag/copy child maps 与所有 shape slot。
- 接 Pad/Pocket 的 raw prism、AddSubShape、Boolean、refine、final Shape。
- 接 Chamfer/Fillet、Pattern/Mirror/MultiTransform 的 original/instance/boolean/slot 顺序。
- 接 Body Tip ledger-only inheritance；guard/failure/cancel 全部发布稳定 reason。

### S6：默认 publisher 与双层 validator

- CLI 对每次 parsed recompute 默认写 `.cad-core.producer-trace.json`，不增加开关。
- 新建可复用 Python trace validator，供 collector、比较器和 mutation tests 共用，避免多个半闭包实现。
- 更新 `regenerate_cad_core_res.py`、live source runner 与测试临时目录清理，使额外 sidecar 不污染 public discovery。
- trace write/validate 失败时 CLI non-zero；不得保留旧 sidecar。

### S7：确定性、无副作用与代表链路收口

- 两次相同 run 的 transaction/scope/event/checkpoint/decision 确定；明确 canonical projection 之外不得漂移。
- 在 recorder inspection session 中连续关闭两个 transaction，第二个无 target/无 producer event 时仍保留第一个 transaction；不得借此跨请求保留几何或业务账本。
- trace 前后 public response、StringHasher allocation、NamedShape/ElementMap 与 mapper 顺序一致。
- 用 Body Tip、Pad/Pocket、Chamfer、Fillet、LinearPattern、self/inter-edge split + open wire 证明完整父子 scope 与切片覆盖。

## 建议代码落点

完整文件矩阵见 `矩阵/producer_trace_implementation_matrix.tsv`。核心新增/修改范围预计为：

```text
cad-core/include/cad_core/app/element_map_producer_trace.h
cad-core/src/app/element_map_producer_trace.cpp
cad-core/include/cad_core/app/string_hasher.h
cad-core/src/app/string_hasher.cpp
cad-core/include/cad_core/runtime/compute_context.h
cad-core/include/cad_core/runtime/recompute.h
cad-core/src/runtime/recompute.cpp
cad-core/include/cad_core/part/topo_shape.h
cad-core/src/part/topo_shape.cpp
cad-core/src/part/topo_shape_expansion.cpp
cad-core/src/part/topo_shape_mapper.cpp
cad-core/src/part/property_topo_shape.cpp
cad-core/src/part/face_maker.cpp
cad-core/src/part/wire_joiner.cpp
cad-core/src/sketcher/sketch_object*.cpp
cad-core/src/part_design/feature_extrude.cpp
cad-core/src/part_design/feature_dress_up.cpp
cad-core/src/part_design/feature_transformed.cpp
cad-core/src/part_design/body.cpp
cad-core/src/adapters/cli/cli.cpp
cad-core/CMakeLists.txt
```

`runtime/topo_naming_state.cpp` 不承接 producer event，只用于证明 trace 不进入 public projection。

## 测试与验收

### focused C++/Python tests

建议新增：

```text
cad-core/tests/element_map_producer_trace_probe.cpp
cad-core/tests/test_element_map_producer_trace.py
cad-core/tests/test_element_map_producer_trace_cli.py
cad-core/tests/test_compare_element_map_producer_trace.py   # N2 开始实现
```

必须证明：sequence、parent、object index、transaction range、entry-local refs、SID table、mapper snapshot、checkpoint、exception/cancel/partial write、reentrancy、drain、mutation hard fail 和无副作用。

### 本轮短跑

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build --target cad-core cad-core-element-map-producer-trace-probe
./build/cad-core-element-map-producer-trace-probe
python3 -m unittest \
  tests.test_element_map_producer_trace \
  tests.test_element_map_producer_trace_cli
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- cad-core docs/FreeCAD几何生态迁移工程-New
```

### 阶段回归

把输出写 `/tmp`，至少运行：

```text
c4m6/topo-state-body-tip-stable-recovery
p2/rect-pad-pocket
c3m5/chamfer-distance-angle-edge
c3m5/fillet-face-selection-history
c3m5/linear-pattern-multi-original-link-retag
p5/sketch-open-wire-internal-empty
c4m3/sketch-external-internal-self-intersection-bowtie
c4m3/sketch-external-internal-split-dangling-mixed
```

检查非空 sidecar、关键 slice/checkpoint、父子 scope、稳定 reason 和 closure。上列 native case 的 trace 当前已出现在 live worktree，可用于 N1 跨 producer smoke；但它们尚未被 Git 跟踪，必须先逐案验证 closure、同次 artifact 绑定和确定性，不能只因文件存在就升级为正式比较权威。

### 重型收口

- 同一代表链路连续跑两次，比较 canonical trace view 的确定性；
- 人为删除 event、SID、snapshot、checkpoint、scope end，validator 全部 hard fail；
- 探针前后 public output、ledger snapshot、SID allocation 与 ElementMap 逐项一致；
- parsed request 的 preflight rejection、dependency skip、executor exception 与 OCCT failure 均有闭合 trace；
- 不运行全量 FreeCAD CI。

## 完成判定

N1 只有同时满足以下条件才可重命名为 `【已实现】`：

1. recorder 不是骨架，`producer_trace_slice_matrix.tsv` 全部 required 行已接线或以权威证据证明 not-applicable；
2. CLI 默认生成独立、非空、闭合的 CAD Core sidecar，无任何开关；
3. mapper-before-consumption、entry-local refs、candidate reject/select、K/U/L、property handoff 和 Body inheritance 都可从 trace 直接证明；
4. failure/cancel、multi-transaction、第二次空 recompute、drain 释放与 mutation tests 通过；
5. 探针不改变 public response、业务账本、StringHasher 或 OCCT 执行顺序；
6. 未修改 native expected 追随 CAD Core，未在 runtime/adapter/fixture 中补造业务事件；
7. N2 可以只通过 trace interface 消费产物，不需要读取 CAD Core 内存或业务对象。

## 风险与非目标

- live worktree 当前出现了 480 个未跟踪 native trace，但批量文件存在不等于全部 producer family 已验证；N1 先证明 CAD Core trace 完整，N2 再逐 family 验证/准入 native oracle。
- 当前 StringHasher/TopoShape live 改动较多，probe insertion 很容易意外改变分配顺序；必须先做 const inspection 与无副作用对照。
- trace 可能很大；只允许内容哈希去重和 drain 释放，不允许丢 event、裁 snapshot 字段或采样。
- 本阶段不实现 public parity 修复、不自动接受差异、不修改 release verdict、不实现 CAD Core 侧 trace writer 以外的几何语义迁移。
