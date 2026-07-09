# FreeCADCmd expected ledger 工具规定

## 目标

`cad-core/fixtures/<phase>/expected/*.freecad.json` 是 FreeCADCmd 生成的 native oracle expected。

从现在开始，涉及 `topoNamingState` 的 FreeCADCmd expected 不能只看 `*.freecad.json` 表面内容。每个 expected 应配套同名 sidecar：

```text
foo.freecad.json
foo.freecad.ledger.json
```

其中：

- `foo.freecad.json` 是对外协议 expected。
- `foo.freecad.ledger.json` 是 FreeCADCmd 同一次生成过程中写出的账本证明。
- validator 的职责是证明 expected 可以被 ledger 解释，并且输入引用都有结论。

不要手工补 ledger。ledger 必须由 `collect_freecad_expected.py --emit-ledger` 在生成 expected 的同一次 FreeCADCmd 运行里写出。

## 工具入口

生成 expected 与 ledger：

```bash
cd /Users/li/Chili3DProject/FreeCAD
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd \
python3 cad-core/tools/collect_freecad_expected.py \
  cad-core/fixtures/c4m6/topo-state-first-recompute-empty.json \
  --out cad-core/out/topo-state-first-recompute-empty.freecad.json \
  --emit-ledger \
  --validate-ledger
```

按 phase 生成：

```bash
cd /Users/li/Chili3DProject/FreeCAD
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd \
python3 cad-core/tools/collect_freecad_expected.py \
  --phase c4m6 \
  --skip-unsupported \
  --emit-ledger \
  --validate-ledger
```

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

`collect_freecad_expected.py --emit-ledger` 会在 expected 旁边写：

```text
cad-core/fixtures/<phase>/expected/<case>.freecad.json
cad-core/fixtures/<phase>/expected/<case>.freecad.ledger.json
```

如果使用 `--out`，ledger 会写到同目录同名前缀：

```text
cad-core/out/<case>.freecad.json
cad-core/out/<case>.freecad.ledger.json
```

`--validate-ledger` 会在写出后立刻调用 validator。validator 失败时，本次 expected/ledger 不能作为权威基线收口。

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

## 当前已经验证过的情况

本轮已用真实 FreeCADCmd 重采：

```text
cad-core/fixtures/c4m6
```

结果：

- `processed=9 skipped=0 failed=0`
- 9 个 `*.freecad.json` 均已生成同名 `*.freecad.ledger.json`
- 7 个 accepted ledger 的 `roundTrip.status` 为 `passed`
- 2 个 rejected ledger 对应 schema / producer hard fail expected
- `validate_freecad_expected_ledger.py --phase c4m6 --strict` 通过 9/9

## 已暴露并修复的问题

第一次启用 `--emit-ledger --validate-ledger` 重采时，以下 case 被 round-trip 拦下：

```text
topo-state-body-tip-stable-recovery
topo-state-link-compound-child-maps
topo-state-reference-shadow-brep
```

失败现象：

```text
roundTrip.status must be passed
```

ledger 的 round-trip diagnostic 指出类似问题：

```text
topoNamingState object Sketch cannot resolve StableSubList g1;SKT;FAC
topoNamingState object Compound cannot resolve StableSubList Compound/ChildBoxA.#f:1;BOX,F
```

含义是：

第一轮 expected 的 `topoNamingState` 没有保留输入引用恢复必须用到的旧 stable token。第二轮把第一轮 `topoNamingState` 原样回传时，`StableSubList` 无法恢复。

这不是 validator 误报，而是 ledger 工具暴露出的 expected 权威性缺口：expected 看起来有 `topoNamingState`，但它不能证明“前端保存整包再回传后引用还能恢复”。

修复方式：

collector 在生成 response `topoNamingState` 时，不再只发布当前 recompute 新生成的对象状态；对于 fixture 输入引用仍需要的 stable token，会把输入 `topoNamingState` 中对应 object 的 `elementMap` / `childElementMaps` / `mapperHistory` 证据合并回输出 state。

修复后：

```bash
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd \
python3 cad-core/tools/collect_freecad_expected.py \
  --phase c4m6 \
  --skip-unsupported \
  --emit-ledger \
  --validate-ledger
```

通过，且独立 validator 也通过。

## 使用原则

不要用“字段存在”证明账本完整。

正确判断顺序是：

1. FreeCADCmd 生成 expected。
2. 同一次运行生成 ledger。
3. validator 验 expected 与 ledger 是否绑定。
4. validator 验输入引用是否都有事件结论。
5. validator 验 projection 是否解释对象裁剪。
6. accepted ledger 必须 round-trip passed。

只有这条链路通过，`*.freecad.json` 才能作为权威 FreeCADCmd expected。

如果某个 expected 变小了，不能直接认为是正确裁剪。必须看 ledger：

- 发布对象是否有 `projection.publishedObjects`。
- 未发布对象是否有 `projection.droppedObjects`。
- 输入引用是否在 `coverage.coveredInputReferenceIds`。
- round-trip 是否通过。

## 当前工具边界

当前 ledger v1 基于 `collect_freecad_expected.py` 在 FreeCADCmd Python 层能拿到的 DTO 生成，包括 `topoNamingState.objects`、`elementMap`、`childElementMaps`、`mapperHistory`、`elementReferenceUpdates` 和 round-trip 结果。

它已经能阻止“expected 变少但引用不能回传恢复”的问题进入权威基线。

它还不是完整 FreeCAD C++ 私有 `NamedShape / ElementMap / MapperHistory` 内部账本导出器。后续如果要证明更深层的 FreeCAD 内部账本完整性，需要在 FreeCADCmd collector 里继续补充更接近 native 内部状态的证据来源。
