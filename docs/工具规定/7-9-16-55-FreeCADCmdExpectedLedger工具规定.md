# FreeCADCmd expected ledger 与 producer trace 工具规定

## 目标

`cad-core/fixtures/<phase>/expected/*.freecad.json` 是 FreeCADCmd 生成的 native public oracle。

新的 native collector 对能建立原生 Document trace 的 fixture，一次运行写出三份同名产物：

```text
foo.freecad.json
foo.freecad.ledger.json
foo.freecad.producer-trace.json
```

其中：

- `foo.freecad.json` 是对外协议 expected。
- `foo.freecad.ledger.json` 是 FreeCADCmd 同一次生成过程中写出的账本证明。
- `foo.freecad.producer-trace.json` 是 ElementMap、StringHasher、mapper、feature producer scope/checkpoint 的只读过程证据。
- validator 的职责是证明 expected 可以被 ledger 解释，并且输入引用都有结论。

三份文件都由 collector 持有，禁止手工补写。`--emit-ledger` 和 `--check-ledger` 只保留为兼容参数，不再控制 sidecar 的生成或比较。

public release gate 仍以 `.freecad.json + .freecad.ledger.json` 裁决公共语义和 provenance。producer trace 不进入请求、response、`topoNamingState` 或 shape 构造，只用于实现定位与 first-divergence 调试。

## 与 release gate 的分工

ledger validator 只证明 native expected 与其 provenance sidecar 闭包；它不运行 CAD Core，不能单独证明 current binary、checked-in `cad-core-res` 或前端 transport contract。完整 release gate 以 `docs/工具规定/7-10-08-10-FreeCADExpectedReleaseGate工具规定.md` 为准：它消费 fixture-role manifest、调用 strict ledger preflight、运行 live binary、审计精确 divergence registry 并验证 current freshness。

反过来，release gate 也不能替代 ledger：native role 缺 ledger、accepted/rejected outcome 不闭合或 round-trip 证据错误，release 结果必须是 `invalid`。

producer trace 有独立闭包边界。trace 缺失或损坏不应改写 public parity verdict，但会使 collector replay / producer 对齐任务 hard fail。

## 工具入口

默认 FreeCADCmd 为：

```text
/Users/li/Chili3DProject/FreeCAD2/build/relwithdebinfo/bin/FreeCADCmd
```

安全地在 `/tmp` 生成三侧产物：

```bash
cd /Users/li/Chili3DProject/FreeCAD
mkdir -p /tmp/freecad-expected
python3 cad-core/tools/collect_freecad_expected.py \
  cad-core/fixtures/c4m6/topo-state-body-tip-stable-recovery.json \
  --out /tmp/freecad-expected/topo-state-body-tip-stable-recovery.freecad.json \
  --validate-ledger
```

验证已入库的单 case public、ledger 与 trace 结构：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 cad-core/tools/collect_freecad_expected.py \
  cad-core/fixtures/c4m6/topo-state-body-tip-stable-recovery.json \
  --check \
  --validate-ledger
```

`--freecadcmd <path>` 可显式覆盖默认二进制。不要再依赖 `FREECADCMD` 环境变量控制本仓库的标准 collector。

`--check` 重新生成并比较 public expected 与 ledger，同时要求磁盘上存在结构闭合的 producer trace。它不做 trace 文本或事件级语义 diff；trace 的 stage 对齐由专门实现审计完成。

单独验证已有 expected/ledger：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 cad-core/tools/validate_freecad_expected_ledger.py \
  cad-core/fixtures/c4m6/expected/*.freecad.json \
  --strict
```

按 phase 验证：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 cad-core/tools/validate_freecad_expected_ledger.py \
  --phase c4m6 \
  --strict
```

配套单测入口：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 -m unittest discover \
  -s cad-core/tests/内部账本完整性验收 \
  -p 'test_freecad_expected_ledger_integrity.py'
```

测试文件：

```text
cad-core/tests/内部账本完整性验收/test_freecad_expected_ledger_integrity.py
```

这个测试不是 FreeCADCmd 原生采集测试，不启动 FreeCAD。它验证 Python 工具层的账本规则，包括：

- 合法 accepted ledger 能被 validator 接受。
- 缺 sidecar 会 hard fail。
- expected hash mismatch 会 hard fail。
- `inputReferences` 即使不是 required，也必须有 event 结论。
- collector 的 `build_freecad_expected_ledger()` 能生成 accepted ledger。
- rejected ledger 不要求 `topoNamingState` / round-trip，但必须有 rejection diagnostics。

## collector 产物

`collect_freecad_expected.py` 会在 expected 旁边写：

```text
cad-core/fixtures/<phase>/expected/<case>.freecad.json
cad-core/fixtures/<phase>/expected/<case>.freecad.ledger.json
cad-core/fixtures/<phase>/expected/<case>.freecad.producer-trace.json
```

如果使用 `--out`，两个 sidecar 会写到同目录同名前缀：

```text
cad-core/out/<case>.freecad.json
cad-core/out/<case>.freecad.ledger.json
cad-core/out/<case>.freecad.producer-trace.json
```

`--validate-ledger` 会在写出后立刻调用 ledger validator。trace 在 drain 后先验证 schema、sequence、snapshot 引用和 scope 闭包；任一门失败，本次三侧产物不能收口。

当前 write-mode 的已知边界是 request-level preflight rejection：这类 fixture 在创建 FreeCAD Document 前就返回 rejected public/ledger，因而没有可 drain 的原生 Document trace。collector 会 hard fail，禁止伪造空 trace。该缺口必须由正式 native request/failure trace 边界解决。

## ledger 类型

ledger 目前有两类 outcome。

### accepted

用于成功发布 `topoNamingState` 的 expected。

必须包含：

- `inputReferences`：从 fixture 的 `StableSubList` / `ReferenceShadow` 等引用字段扫描出来。
- `objects`：账本对象，包括发布对象和相关内部对象。
- `events`：每个输入引用的恢复结论，以及 mapperHistory 事件。
- `projection`：解释哪些对象发布进 `topoNamingState.objects`，哪些对象被裁掉以及为什么。
- `coverage`：证明每个 `inputReferences` 都被 terminal event 覆盖。
- `roundTrip`：用第一轮 response 的 `topoNamingState` 原样回传，再跑一轮，确认 required 引用可恢复。

### rejected

用于 schema / producer 这类 hard fail expected。

这类 expected 本来就不会发布 `topoNamingState`，也不要求 round-trip。ledger 必须记录：

- `outcome: rejected`
- expected diagnostics
- `rejection.diagnosticCodes`
- expected payload hash

## validator 硬规则

`validate_freecad_expected_ledger.py --strict` 会硬卡这些问题：

- expected 缺同名 `.freecad.ledger.json`
- ledger schema 不匹配
- producer 不是 `FreeCADCmd`
- expected payload hash 不匹配
- accepted ledger 的 `topoNamingStateHash` 不匹配
- `inputReferences` 没有被 event 覆盖
- event 的 source / target 无法落到 ledger object 的 before/after elements
- expected 发布的 `topoNamingState.objects` 没有 projection 解释
- relevant object 未发布且没有 droppedObjects 解释
- accepted ledger 的 round-trip 没有 passed
- rejected ledger 没有 diagnostics / rejection evidence

## 当前 corpus 状态

2026-07-12 live tree 有：

```text
480 个 *.freecad.json
480 个 *.freecad.ledger.json
1 个 *.freecad.producer-trace.json
```

480 个 ledger 中 470 个 accepted、10 个 request-level rejected。producer trace 迁移尚未覆盖全部 native case，不能把“public/ledger 已齐全”表述成“三侧产物已全量闭包”。

当前唯一入库 trace 是：

```text
cad-core/fixtures/c4m6/expected/
  topo-state-body-tip-stable-recovery.freecad.producer-trace.json
```

`c4m6` 仍是 9 个 native、1 个 protocol-only。ledger validator 可验证 9 个 pair；trace closure 目前只能对已迁移的 Body/Tip case 给出结论。

2026-07-12 用默认新 FreeCADCmd 对 Body/Tip case 执行 `--check --validate-ledger` 返回 1。canonical ledger 比较唯一差异是 `producer.freecadVersion`：checked-in 为 `1.2.0 revision 20260519`，当前 binary 为 `1.2.0 revision 46970`。

这表示 producer 基线尚未重放闭合，不是 CAD Core parity red。`/tmp/freecad-doc-audit/` 已证明新 binary 可生成三侧产物；正式基线只能由 collector 重生，禁止手改 producer version 放行。

## 使用原则

不要用“字段存在”证明账本完整。

对新三侧采集，正确判断顺序是：

1. FreeCADCmd 生成 public expected。
2. 同一次运行生成 ledger，并在 Document 关闭前 drain producer trace。
3. validator 验 expected 与 ledger 是否绑定。
4. validator 验输入引用是否都有事件结论。
5. validator 验 projection 是否解释对象裁剪。
6. accepted ledger 必须 round-trip passed。
7. trace 必须满足 event、scope、snapshot 和 transaction 闭包。

其中 1、3 至 6 决定 public expected / ledger 是否能作为 release oracle；第 2 步的 trace drain 与第 7 步决定内部 producer 证据是否可用于实现对齐。trace 尚未迁移不撤销既有 ledger 权威，但不能宣称 producer 路径已经闭包。

如果某个 expected 变小了，不能直接认为是正确裁剪。必须看 ledger：

- 发布对象是否有 `projection.publishedObjects`。
- 未发布对象是否有 `projection.droppedObjects`。
- 输入引用是否在 `coverage.coveredInputReferenceIds`。
- round-trip 是否通过。

## 当前工具边界

ledger v1 基于 collector 在 FreeCADCmd Python 层拿到的 DTO，覆盖 `topoNamingState.objects`、`elementMap`、`childElementMaps`、`mapperHistory`、`elementReferenceUpdates` 和 round-trip。

producer trace 来自修改后的原生 FreeCADCmd C++ recorder，覆盖 ledger/StringHasher/mapper snapshot 与 producer stage；它不是 ledger 的替代物。

ledger 证明 public/provenance 闭包，trace 证明内部生产过程。CAD Core 只对齐 public result，并用 trace 定位实现；两者都不得混入普通请求或 response。
