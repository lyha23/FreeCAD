# FGM-N2 Producer Trace 首处分叉比较器方案

> 使用边界：N2 只在 public/ledger 行为无法对齐且需要定位内部原因，或任务明确要求 producer 审计时运行。它的退出码和 `different/invalid` 只属于 trace diagnostic lane，不进入 public/ledger differences、firstFailure、semanticStatus 或 releaseStatus。

## 目标

新增用户所需入口：

```text
/Users/li/Chili3DProject/FreeCAD/cad-core/tools/
  compare_element_map_producer_trace.py
```

它只读比较：

```text
expected/<case>.freecad.producer-trace.json
cad-core-res/<case>.cad-core.producer-trace.json
```

两侧完全对齐返回 0；发现第一处分叉立即形成一个结构化诊断并返回 1；输入缺失、schema/闭包无效或没有可比较切片返回 2。这些退出码不代表 public/ledger parity。比较器不修改 fixture、expected、ledger、CAD Core response、release verdict 或任何 trace。

N2 的前置条件是 FGM-N1 已完成 recorder、全部 required slices、默认 publisher 和闭包验证。比较器不能用来掩盖 CAD Core trace 缺切片。

## 工具形状

根脚本只做参数解析和报告输出，复杂逻辑放进一个深 Python 模块：

```text
cad-core/tools/compare_element_map_producer_trace.py
cad-core/tools/element_map_producer_trace/
  __init__.py
  model.py
  validate.py
  projection.py
  compare.py
  report.py
```

对调用者只暴露：

```python
validate_trace(payload) -> ValidatedTrace
compare_traces(expected, actual) -> ComparisonResult
```

现有 `collect_freecad_expected.py::validate_producer_trace()` 应迁入并扩展到这个模块，collector、CAD Core actual publisher tests 与 comparator 共同消费同一闭包规则，避免三套 validator 漂移。collector 负责 public/ledger authority，也可以附带生成 trace 诊断 sidecar；比较器没有写 expected 的权限。

## CLI

支持按 case 或显式路径两种形式：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core

python3 tools/compare_element_map_producer_trace.py \
  --phase c4m6 \
  --case topo-state-body-tip-stable-recovery \
  --report /tmp/topo-state-body-tip-stable-recovery.first-divergence.json

python3 tools/compare_element_map_producer_trace.py \
  --expected fixtures/c4m6/expected/topo-state-body-tip-stable-recovery.freecad.producer-trace.json \
  --actual fixtures/c4m6/cad-core-res/topo-state-body-tip-stable-recovery.cad-core.producer-trace.json \
  --report /tmp/body-tip.first-divergence.json
```

默认路径由 `--phase/--case` 唯一推导。`--report` 省略时只输出 stdout/stderr，不在 checked-in fixture 树写报告。禁止提供“忽略所有 SID/Tag”“接受差异”“自动更新 expected”之类选项。

## 比较不是 JSON 文本 diff

native 与 CAD Core 的本地 sequence、scope ID、objectTag 和 snapshot hash 可能不同，但它们表达的 transaction、scope 树、对象关系和 producer 状态必须同构。算法固定分为七层；上层未对齐时不继续用下游 raw token 制造噪音。

### 1. 双侧独立闭包验证

先各自验证 transaction、scope、event、snapshot、SID、target、child range、mapper 与 object index，并校验 trace provenance 绑定的 input/response hash。任一侧不闭合或与对应 artifact 错配都返回 2，而不是报告业务分叉。

### 2. 建立对象和 identity 双射

- object 以 `object name + semantic type + graph role` 对齐，数字 ID/Tag 只保留审计。
- shape/map/hasher/mapper identity 以 `producer scope path + shape role + input ordinal + create/copy/share/reset lifecycle` 对齐。
- 同一侧出现一对多或生命周期不闭合立即报告 identity mapping failure。

### 3. transaction 对齐

按 transaction ordinal、targets 的对象双射、outcome 与是否为空对齐。不能只选最后一次 transaction；第二次空 recompute 也必须作为结构事实存在。

### 4. scope 树对齐

scope join key：

```text
(transaction ordinal,
 parent semantic path,
 mapped object,
 producerKind,
 stage,
 sibling occurrence ordinal)
```

本地 `scopeSequence` 仅用于恢复各自树，不作 join key。缺 scope、额外 scope、parent 不同或 Body 内出现上游 producer replay，应在这里成为第一处分叉。

### 5. scope 内 event 顺序对齐

在已匹配 scope 内，按原始 event 顺序比较。事件语义 key 为：

```text
(slice,
 source shape role / IndexedName,
 target shape role / IndexedName,
 candidate ordinal,
 occurrence ordinal)
```

raw `#SID`、mapped name、Tag 和 snapshot hash不能作为 join key。若左右当前位置不同，只允许有限 look-ahead 判断“expected event missing”或“actual extra event”，仍报告最早位置；禁止用全局 LCS 把前面的缺事件跳过去。

### 6. decision、fields 与 snapshot 比较

匹配事件后依次比较：

1. `decision` / stable `reason`；
2. source/target、operation、type、candidate sort fields；
3. 解引用后的 before snapshot canonical payload；
4. event value fields；
5. 解引用后的 after snapshot canonical payload；
6. raw SID/mapped-name/ordered refs 与本地 allocation order 的严格审计。

snapshot ID 只做本文件完整性 fast path。hash 不同必须展开到第一个 JSON pointer/path，例如：

```text
/ledger/entries/Face/Face3/entries/0/elementIdRefs/0
```

### 7. 第一处分叉与 downstream drift 折叠

一旦找到第一处状态或 decision 分叉，主报告停止。后续 SID/name 级联差异只统计为 `downstreamDriftCount`，可通过报告 JSON 的 evidence 索引展开，但不能淹没根因。

## canonical projection

详细矩阵见 `矩阵/producer_trace_comparison_projection.tsv`。

| 字段 | 处理方式 |
| --- | --- |
| sequence/transaction/scope 数字 | 各自验证闭包，再投影为 ordinal 和 scope semantic path；顺序、数量、父子关系严格 |
| snapshot ID/hash | 各自验证 hash；比较解引用后的 canonical payload |
| document 临时名/build/path/version | provenance 校验，不作为业务相等条件 |
| object ID/objectTag | 通过对象双射比较引用关系、符号与角色；原值保留 |
| mapped name 中的 object Tag、master/input/child tag | 通过同一 Tag 双射后严格比较；不能整段删除 |
| producer C++ TypeId/类名 | 映射到共同 `producerKind/stage`；原值保留 provenance |
| exception type/message | 映射到稳定 error category/reason；outcome/stage/partial-write 严格 |
| 明确声明的随机 suffix | 只对该 event 使用 producer 给出的 `nondeterminismClass + stableComparisonKey`；raw 仍保留 |
| JSON object key 顺序 | 忽略 |
| 所有语义数组 | 严格保序，包括 entry 链、related refs、mapper targets、候选、Originals 和 child ranges |

必须特别锁住：raw `#SID[:index]`、raw mapped name 和 ordered SID refs不能作为事件 join key，但结构对齐后仍是严格 producer-local parity 证据。不能把它们普遍 canonicalize 掉，否则 StringHasher 分配或 entry-local refs 的第一处漂移永远不会报出。

同样禁止归一化：IndexedName、M/G/D relation、source/target 顺序、candidate ordinal、decision/reason、operation/postfix、PrefixID/PrefixIDIndex、K/tuple、U/L、child range 与 hasher allocation order。

## 分叉分类与报告

稳定分类至少包括：

```text
invalid_expected_trace
invalid_actual_trace
transaction_missing_or_extra
transaction_outcome_mismatch
scope_missing_or_extra
scope_parent_mismatch
producer_replay
event_missing_or_extra
decision_mismatch
reason_mismatch
before_snapshot_mismatch
field_mismatch
after_snapshot_mismatch
sid_allocation_mismatch
ordered_refs_mismatch
target_inventory_mismatch
child_range_mismatch
mapper_relation_mismatch
final_checkpoint_missing
```

人类输出示例：

```text
FIRST_DIVERGENCE decision_mismatch

scope:
  transaction[1] > Pad:partdesign.extrude > makeShapeWithElementMap

expected:
  sequence: 1287
  slice: maker.select
  decision: selected
  target: Face3

actual:
  sequence: 914
  slice: maker.candidate.reject
  decision: rejected
  reason: missing_generated_relation

before snapshot: aligned
first differing path:
  /mapper/Generated/Face2/0
expected: Face3
actual: missing

downstream drift: 146 events collapsed
```

机器报告至少包含：schema、status、classification、scope path、expected/actual event identity、first JSON path、expected/actual value、before/after alignment、source fixture、trace hashes、downstream count、建议 owner module。owner module 只能由 slice-to-module 静态表得到，不能按 fixture 名猜。

## 实施步骤

### S0：前置门禁

- 证明 N1 required slice matrix 完整，代表 actual traces 闭合。
- 为选中 case 收集同次 native trace，不能手写或从 public expected 反推。
- 冻结 comparison projection 与 producerKind/stage 映射。

### S1：共享 model 与完整 validator

- 把 native collector 的最小 validator 深化为共享模块。
- 增加 transaction range、scope tree、object index、content hash、SID at-time、target、child、mapper、final checkpoint closure。
- mutation tests 删除任一闭包实体都必须返回 2。

### S2：canonical view 与 identity 双射

- 构建 object、Tag、trace identity、producerKind、scope semantic path。
- canonical view 保留所有 raw 审计字段，不覆写原 payload。
- 对不确定分支只接受 producer 源头声明的 class/key。

### S3：有序对齐与首分叉 engine

- 逐 transaction、scope、event、before/decision/fields/after 对齐。
- 实现有限 look-ahead 的 missing/extra 分类，禁止全局 LCS 隐藏根因。
- snapshot diff 返回确定的最小 JSON path。

### S4：CLI、报告和 runner 集成

- 实现根脚本、stdout 文本和可选 `/tmp` JSON report。
- 在 `compare_freecad_expected.py` 的诊断报告中只附 first-divergence 链接/摘要，不改变 public semantic status 或 release gate passed。
- missing/invalid trace 显式 `traceStatus=invalid|missing`，不能变成 public mismatch。

### S5：代表 case 收口

- Body Tip、Chamfer、Fillet、LinearPattern、Pad/Pocket、self/inter-edge split + open wire 都能稳定定位第一处分叉或证明对齐。
- 同一 case 两次比较结论确定。
- 人为注入 event/decision/SID/checkpoint 差异时，分类和 first path 精确。

## 测试

新增：

```text
cad-core/tests/test_element_map_producer_trace_validation.py
cad-core/tests/test_element_map_producer_trace_projection.py
cad-core/tests/test_compare_element_map_producer_trace.py
```

至少覆盖：

- 完全相等；
- 本地 sequence/Tag/hash 不同但语义双射相等；
- missing/extra scope 和 event；
- decision 先于 raw SID 级联漂移；
- before 已不同与本 event 才改变的区分；
- ordered refs 交换必须失败；
- mapper target/candidate/child range 顺序改变必须失败；
- Body 内 producer replay；
- nondeterminism 未声明不得忽略；
- invalid expected/actual 与 public mismatch 分类分离。

## 验收命令

### 本轮短跑

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest \
  tests.test_element_map_producer_trace_validation \
  tests.test_element_map_producer_trace_projection \
  tests.test_compare_element_map_producer_trace
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- cad-core docs/FreeCAD几何生态迁移工程-New
```

### 阶段回归

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/compare_element_map_producer_trace.py \
  --phase c4m6 --case topo-state-body-tip-stable-recovery \
  --report /tmp/body-tip.first-divergence.json
```

再按 N1 fixture matrix 扩 Chamfer、Fillet、Pattern、Pad/Pocket 与 split/open-wire。每个 case 必须同时确认 native/actual trace hashes、比较 status 和第一处分叉 owner，不只看进程退出码。

### 重型收口

- 两侧各自同 case 连跑两次，canonical view 与比较结论确定；
- 对 event、SID、checkpoint、scope、decision 做 mutation，验证 hard fail/分类；
- public parity report 在有无 trace 时语义 verdict 不变，只有 trace diagnostic 状态变化；
- 不运行全量 FreeCAD CI。

## 完成判定与非目标

只有代表链路都能报告稳定、可操作的最早 scope/checkpoint/field path，且没有通过宽松 canonicalization 隐藏 raw 证据，N2 才可标记 `【已实现】`。

本阶段不自动修 CAD Core、不改 expected、不接受差异、不把 trace 升格为 release oracle、不按最终 FaceN 或 fixture 名决定 owner，也不把后续全部 diff 展开成噪音列表。

## Live 实施审计（2026-07-12，未收口）

- live HEAD 为 `05f14b6b0c`；本轮保护了开始时全部用户修改、删除项、N1 文档改名和 trace/expected，没有 reset、restore、clean、暂存、提交或写入 checked-in fixture artifact。
- shared validator、document-graph/object/Tag/SID/shape/map/hasher/mapper identity projection、transaction/scope/event first-divergence engine、CAD Core 默认 CLI、JSON/text report 和 public parity observation 已实现。scope/event 对齐保持 root/nested event 原始交错顺序，只做 `LOOK_AHEAD=3` 的顺序前瞻；未知 producer 名不会按子串宽松归一化。
- 用户指定的 validator/projection/comparator focused 共 43 项通过；加 native collector binding 回归共 48 项通过；N1 actual publisher/CLI 共享 validator 26 项通过。`traceStatus=aligned|different|missing` 的 2 项定向测试证明 trace 观察不改变 `exactStatus`、`semanticStatus`、`releaseStatus` 或 `releaseGatePassed`。
- collector 对新采 native trace 写入 `snapshotPayloadHashAlgorithm=canonical-json-sha256-v1`，并给每个已解析 snapshot 写 `canonicalPayloadSha256` 后再调用共享 validator。8 个 `/tmp` 代表 native trace 的 2029/2029 个 snapshot 均通过该校验；checked-in artifact 未改写。
- 8 个 N1 代表 family 均用同次 `/tmp` request-bound native response/trace 与重新运行的 CAD Core response/trace 配对，双方 schema、request/response hash、transaction/scope/event/SID/checkpoint 闭包先独立通过，再稳定 exit 1：Body Tip、Pad/Pocket、Chamfer、Fillet、open wire、self-intersection、inter-edge split 为 `scope_missing_or_extra`，LinearPattern 为 `target_inventory_mismatch`。报告 owner 由静态 producer/slice 表给出；其中 open wire 首处分叉 owner 为 `part/face_maker`，Body Tip 为 `part/topo_shape`。
- Body Tip 两次 stdout 与 JSON 报告均逐字节一致；JSON SHA-256 为 `70306ccfd3e8a93ee664467a7cc453b336d1508bd0568a9f028371f97a22e9fd`。real actual trace 的 event、SID、checkpoint、scope、decision 五类 `/tmp` mutation 均 exit 2；checkpoint 稳定分类为 `final_checkpoint_missing`，其余为 `invalid_actual_trace`。合法 decision、scope、event、snapshot、ordered-ref/mapper/child-range 变异的 exit 1 分类由 focused tests 覆盖。
- 默认 `--phase c4m6 --case topo-state-body-tip-stable-recovery` 薄 CLI 正确推导 `cad-core-res` actual，因 actual sidecar 缺失返回 exit 2 / `missing_actual_trace`；显式路径比较等价。主要报告位于 `/tmp/body-tip.first-divergence.json`、`/tmp/fgmn2-*.first-divergence.json`、`/tmp/body-tip.determinism-{a,b}.first-divergence.json` 与 `/tmp/fgmn2-body-tip.mutation-*.report.json`。
- 完整 `tests.test_freecad_expected_public_parity` 当前 21/22；本轮引入的 fake-runner 深 validator 回归已修复，唯一剩余失败仍是既有 `c3m1` snapshot 的 `topoNamingState.producer.freecadVersion` public semantic drift。

方案仍不得重命名为 `【已实现】`，原因有四项：

1. fixture tree 的 480 个 native trace 都已有 request/response hash binding，但没有 `snapshotPayloadHashAlgorithm` 或 `canonicalPayloadSha256`；C++ 原始 `sha256` 绑定的是 drain 前原始 JSON bytes，collector 解析并重排后无法从现有文件重算。因此它们只能按 legacy closure 读取，不能满足本方案要求的独立 snapshot content-hash 证明；
2. fixture tree 没有任何 `cad-core-res/*.cad-core.producer-trace.json`，所以纯 `--phase/--case` 没有正式 actual pair；
3. FreeCAD2 当前 native mapper snapshot 只有 `counts/isNull/ledger/shapeType/tag`，没有 raw M/G/D source-target adjacency；native `childMaps` 只有 `indexed/tag/offset/count/postfix/elementIdRefs`，没有 parent output inventory 或 nested snapshot reference。共享 validator 不能凭空验证或比较这些缺失关系，故 S1 的“两侧 child/mapper 同强度闭包”仍未满足；
4. 完整 public parity 模块的既有 `c3m1` snapshot 仍把 `topoNamingState.producer.freecadVersion`（expected `revision 46970`、current `revision 20260519`）判为未接受 public semantic diff；该失败与 trace observation 字段无关。

关闭上述门禁需要先扩展 FreeCAD2 probe snapshot contract，使 mapper/child 数据足以自证 inventory、nested linkage 与 raw M/G/D，再单独授权 artifact 更新：用已修正 collector 成对重采 native expected/ledger/trace，并由 N1 publisher 原子物化同 run CAD Core response/trace。随后重跑纯 phase/case、8 个代表 family、确定性、mutation 与完整 public parity。完成前 blocker queue 保持 S0/S1/S5 blocked，S2-S4 closed。
