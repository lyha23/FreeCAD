# cad-core fixture case FreeCADCmd expected 覆盖扩容实施方案

> - 初版日期：2026-07-15
> - 范围收窄：2026-07-16
> - 当前状态：已完成（2026-07-17）；保留模块 coverage、E2-E5、全量复现和 strict ledger 均闭合，native authority 从 480 扩容到 558
> - 地基提交：`/Users/li/Chili3DProject/FreeCAD` `2d0e4b8d36`
> - 地基报告：`/Users/li/Chili3DProject/FreeCAD/cad-core/tools/freecad_expected_parity/reports/README.md`
> - 方案目录：`/Users/li/Chili3DProject/FreeCAD2/docs/fixture-case`
> - 实施仓库：`/Users/li/Chili3DProject/FreeCAD`
> - 权威 fixture 根：`/Users/li/Chili3DProject/FreeCAD/cad-core/fixtures`
> - 唯一 native expected producer：受控 `FreeCADCmd`
> - 发行版入口：`/Users/li/.cargo/bin/FreeCADCmd`（必须解析到 `/Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd`）
> - collector：`/Users/li/Chili3DProject/FreeCAD/cad-core/tools/collect_freecad_expected.py`
> - 覆盖范围权威：`/Users/li/Chili3DProject/FreeCAD2/docs/减法实现/FreeCAD2-无QtApp减枝实施方案.md` 的最终保留闭包
> - 最终报告：`/Users/li/Chili3DProject/FreeCAD/cad-core/tools/freecad_expected_parity/reports/README.md`
> - 最终 producer：`/Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd`，SHA-256 `391b638fa65bd761d55291be0a8e7ac22bd4d5ba40ccbd9b14621209a402181a`

## 1. 决策

本方案只解决一件事：

> 给定 cad-core fixture，由受控 FreeCADCmd 稳定生成 native public expected 和同次 ledger，并证明这些产物可重复、来源可追溯。

本方案的目标是**覆盖剪枝之后保留下来的全部模块**，不是只建设 collector 地基、复现现有 480 个 native authority，或挑少量代表 case 做演示。模块范围以 `docs/减法实现/FreeCAD2-无QtApp减枝实施方案.md` 的最终保留边界为唯一权威。

完成时必须同时满足：

- `nativeAfter > nativeBaseline`，当前 `nativeBaseline = 480`；
- 剪枝后每个保留模块都有完整的 fixture/capability inventory 和模块级覆盖 verdict；
- 所有归属于保留模块且具有 FreeCAD native public root 的 fixture 都取得 native authority；
- 保留模块范围内的 `collector_general_gap`、`not_investigated` 和 `native_eligible_without_authority` 全部清零；
- 所有新增 native case 均完成 promotion 前 staging 独立双跑和 promotion 后 checked-in 复验。

`nativeAfter > 480` 只是必要条件，不是充分条件。只要仍有一个剪枝后保留模块未覆盖、仍有一个 native-eligible fixture 没有 authority，或 native 数量仍为 480，本方案就只能报告“collector 地基/局部模块完成，整体覆盖扩容未完成”。

固定执行关系为：

```text
cad-core fixture input
  -> collect_freecad_expected.py
  -> FreeCADCmd
  -> <case>.freecad.json
  -> <case>.freecad.ledger.json
```

`FreeCADCmd` 是 native expected 的唯一裁判。legacy `cad-core`、Rust FFI、`freecad-kernel-v2` 和 Web runtime 都不是本方案的执行对象，也不能反向生成 expected。

## 2. 当前基线

截至 2026-07-15，权威 fixture 库的只读库存为：

| 项目 | 数量 | 本方案中的含义 |
|---|---:|---|
| fixture input | 775 | `cad-core/fixtures/<phase>/<case>.json` |
| `native` role | 480 | 已有同名 public expected + ledger |
| `protocol_only` role | 14 | 不由 FreeCADCmd native expected 裁决 |
| `unsupported` role | 281 | 当前只表示尚无 native expected + ledger，不等于产品不支持 |

`480/14/281` 是 oracle 资产库存，不是产品支持率。扩容目标是增加经过 FreeCADCmd 证明的 native authority，而不是把所有 fixture 强行改成 native。

### 2.1 剪枝后必须覆盖的固定闭包

本方案不自行发明模块范围，固定继承 `docs/减法实现/FreeCAD2-无QtApp减枝实施方案.md` 的最终保留闭包：

| 层级 | 必须覆盖的保留内容 | fixture 覆盖要求 |
|---|---|---|
| 运行时核心 | `FreeCADBase`、`FreeCADApp`、`FreeCADMainCmd`、`FreeCADMainPy` | 覆盖 collector 启动、Python 初始化、文档/对象生命周期、属性写入、recompute、序列化和 topology-state 公共路径 |
| App module | `Material` | 覆盖 fixture schema 中属于 Material 的全部 native-eligible TypeId/property/capability |
| App module | `Part` | 覆盖 Part primitives、Boolean、Compound、数据交换公共入口及 fixture schema 中全部 Part native-eligible capability；`Import` module 删除后保留的 `Part::ImportStep` 仍必须覆盖 |
| App module | `Sketcher` | 覆盖草图几何、约束、外部引用、求解结果和 fixture schema 中全部 Sketcher native-eligible capability |
| App module | `PartDesign` | 覆盖 Body、Pad、Pocket、Revolution/Groove、dress-up、transformed、Loft/Pipe 以及 fixture schema 中全部 PartDesign native-eligible capability |
| App module | `Mesh` | 覆盖 Mesh App 公共对象、属性、序列化及 fixture schema 中全部 Mesh native-eligible capability |
| App module | `Spreadsheet` | 覆盖 Spreadsheet App 公共对象、单元格/表达式/依赖及 fixture schema 中全部 Spreadsheet native-eligible capability |
| App module | `Assembly` | 覆盖 Assembly App 对象图、约束/求解公共结果、链接关系及 fixture schema 中全部 Assembly native-eligible capability |
| target 依赖 | `OndselSolver` | 不制造脱离 Assembly 的伪 fixture；通过 Assembly 约束和求解案例覆盖其实际保留调用链，并在 coverage manifest 中单独给出 verdict |
| 仅保留的 Python/data | 经剪枝方案确认可 headless 运行的 Help/AddonManager 等入口 | 若没有 CAD native public root，不强造 `.freecad.json`；必须在同一 coverage manifest 中登记为 `non_cad_smoke`，附 import/smoke 证据，不能从总覆盖表中消失 |

FEM、CAM、BIM、Draft、Import、Measure 和 TechDraw 已被剪枝方案永久删除，不是本方案扩容目标，也不能为了让 fixture 变绿而重新加入构建。fixture 若仍依赖这些被删模块，必须迁移为保留模块的 public 形态或证据化标记为超出最终产品边界。

### 2.2 “模块全部覆盖”的可机读定义

必须新增并签入：

```text
cad-core/tools/freecad_expected_parity/reports/retained_module_fixture_coverage.v1.json
```

该报告必须从剪枝后 live target/runtime closure、fixture input、role manifest 和 authority 产物重建，不得手写模块结论。至少包含：

- `retainedClosureSource`：剪枝方案路径、固定模块列表和用于验证的 source/build identity；
- `modules[]`：每个核心 target、保留 App module、`OndselSolver` 和保留 Python/data 入口各一行；
- `typeIds`、`properties`、`capabilityFamilies` 和对应 fixture 列表；
- `fixtureCount`、`nativeAuthorityCount`、`protocolOnlyCount`、`evidenceExcludedCount`；
- `collectorGeneralGapCount`、`notInvestigatedCount`、`nativeEligibleWithoutAuthorityCount`；
- `coverageStatus = passed|failed|non_cad_smoke` 和证据路径；
- 多模块 fixture 的全部 owner module 映射，同时保持全局 fixture 去重计数。

模块级 `passed` 必须满足：该模块在 fixture contract 中发现的每一个 native-eligible TypeId/property/capability 都至少有对应 fixture，且相关 fixture 全部具有完整 native public expected + ledger。只有一个“代表 case”不能判定整个模块覆盖完成。

允许 `protocol_only`、`freecad_native_not_expressible` 或 `non_native_fixture` 例外，但必须逐 case 有实际输入/FreeCADCmd 证据，并计入模块报告；不能用这三类标签批量掩盖 collector 缺口。任何 `collector_general_gap`、`not_investigated` 或 native-eligible 无 authority 都使对应模块 `coverageStatus = failed`。

## 3. 权威资产

### 3.1 输入

```text
cad-core/fixtures/<phase>/<case>.json
```

fixture 是 collector 输入。输入必须表达 FreeCAD public API 能建立的对象图和属性，不能依靠 fixture 名称触发特殊逻辑。

### 3.2 public expected

```text
cad-core/fixtures/<phase>/expected/<case>.freecad.json
```

- 只能由受控 FreeCADCmd collector 生成；
- 是 CAD 几何、对象图和公开 topology-state 语义的 native authority；
- 禁止手工编辑；
- 不要求跨 FreeCAD revision 逐字节相等，以规范化 public semantic equality 为准。

### 3.3 ledger

```text
cad-core/fixtures/<phase>/expected/<case>.freecad.ledger.json
```

- 必须与 public expected 在同一次 FreeCADCmd 执行中生成；
- 必须结构合法、hash 闭包有效、内部自洽；
- 是 provenance/内部命名收据，不是 runtime 请求或响应；
- revision、内部 hash 或 producer metadata 漂移必须单独报告，不能反向判定 public expected 失败。

缺少同名 ledger 的 `*.freecad.json` 不得标记为完整 native authority。

### 3.4 非权威资产

以下内容不能生成或替代 native expected：

- `cad-core-res/*.cad-core.json`；
- `cad-rs-res/*`；
- legacy `cad-core` binary 输出；
- Rust FFI 输出；
- `freecad-kernel-v2` 输出；
- Web/backend response；
- 历史 `*.expeted.json`；
- 手工拼装 JSON。

producer trace 默认关闭。发行版 FreeCADCmd 没有本项目探针、因而不生成 `<case>.freecad.producer-trace.json`，属于正常状态；collector 必须记录 `producerTraceStatus = not_evaluated` 和 `reason = disabled-by-request`，不得因此跳过、降级或阻塞 public expected、ledger、repeat、promotion 与最终 Gate。

只有 public expected 或 ledger 出现分叉时，才允许临时使用带探针的 FreeCAD2 诊断二进制调查首个 producer 分叉。该诊断二进制只能把 trace 和相关报告写入 `/tmp` 或 `cad-core/out/`，不得生成、覆盖或 promotion 权威 public expected/ledger，也不得替代发行版 FreeCADCmd 的 producer 身份。

权威 fixture 根中既有的 `*.freecad.producer-trace.json` 视为历史诊断资产：不要求发行版复现，不是 `native` role 的组成部分，不计入 promotion 五项产物和 coverage 完成条件。新增 native case 可以没有 producer trace；任何报告即使展示历史 trace SHA，也必须明确其不属于本次发行版采集结果和 native authority。

## 4. 本方案明确不做什么

本方案不实施、不验证：

- `compare_freecad_expected.py` 的 candidate/runtime parity；
- `--actual-source live`、`rust-ffi` 或 `freecad-kernel-v2`；
- native-candidate、legacy compatibility 或 runtime release Gate；
- CAD Core、Rust 或 FreeCAD2 runtime 的语义修复；
- Web protocol、HTTP/C ABI、owner-thread、lifecycle、stress 或生产切换；
- capability contract、runtime publication parity 或产品 coverage 百分比；
- `cad-web-background` 97 个独有路径的 runtime 迁入；
- 因 current runtime 与 expected 不同而修改 expected；
- 用 bbox、面积、拓扑遍历顺序或 fixture 名称猜测 FreeCAD 语义。

例如，legacy CAD Core 在 Compound child maps、mapper history 或 mesh 字段上与 FreeCAD expected 不同，属于其他 runtime 实现任务，不属于本方案。

这里“不做产品 coverage 百分比”不表示可以漏掉剪枝后保留模块。本方案做的是固定闭包内的 fixture/native-authority 全覆盖，不宣称覆盖 FreeCAD 在 fixture contract 之外的所有历史 API。

## 5. role 边界

`cad-core/tools/freecad_expected_parity/fixture_roles.v1.json` 在本方案中只回答：

> 这个 fixture 是否具有完整、可复现的 FreeCADCmd native public expected + ledger？

role 含义固定为：

- `native`：public expected + ledger 已成对生成，并通过复现检查；
- `protocol_only`：该案例没有 FreeCAD native public root，不进入本方案；
- `unsupported`：当前尚未取得 native authority，可能是 collector 缺口、FreeCAD 不表达该状态、输入不是 native 形态，或尚未调查。

禁止把 `unsupported` 自动解释为产品不支持。role 提升为 `native` 时，必须与 input、expected、ledger 和 producer report 同批提交。

## 6. collector 合同

### 6.1 必须支持的选择范围

`collect_freecad_expected.py` 必须支持：

1. 单 fixture；
2. 单 phase；
3. 全部已登记 native fixture；
4. 单 fixture/phase/all-native 的独立重复采集；
5. check-only，不覆盖 checked-in expected；
6. candidate root 与 checked-in fixture 根物理隔离。
7. 尚未 promotion 的 staging fixture 独立双跑，不依赖 checked-in native manifest 或既有 expected/ledger。

checked-in check/repeat 模式显式选择不存在或没有 native authority 的 case 时必须 fail-closed；staging repeat 模式则必须显式选择 staging input，并验证 run-A/run-B 都实际执行该 case。两种模式都不能产生 zero-case 假绿。

### 6.2 最小 producer 身份与可选 provenance

报告必须记录：

- FreeCADCmd 绝对路径；
- FreeCADCmd SHA256；
- FreeCAD/OCCT 版本；
- collector 文件 SHA256；
- producer trace 状态，默认 `not_evaluated`。

报告可选记录：

- source commit 和 dirty 状态摘要；
- build directory、build type、generator 和 CMake cache SHA256；
- 完整参数或可重建命令；
- fixture input、public expected 和 ledger SHA256。

可选 provenance 缺失时只写入 warning，不得改变 public expected、ledger validation 或 repeat 的通过结论。FreeCADCmd 路径、二进制 SHA256、FreeCAD/OCCT 版本和 collector SHA256 属于最小 producer 身份，缺失时必须 fail-closed。核心门禁回答“选中的 case 是否实际执行、public expected 是否相等、ledger 是否合法”；详细 provenance 用于后续追查。

### 6.3 verdict 必须拆开

报告必须分别给出：

- `publicExpectedStatus`：public expected 是否规范化语义相等；
- `ledgerValidationStatus`：ledger 是否结构合法、内部自洽；
- `ledgerDriftStatus`：内部 metadata/hash 是否变化；
- `producerTraceStatus`：默认 `not_evaluated`。

不得把 public expected、ledger drift 和 producer trace 压成一个总布尔值。

## 7. 单 case 标准工作流

### 7.1 staging

新增 input 和临时产物先放在：

```text
cad-core/out/fixture-staging/
  fixtures/<phase>/<case>.json
  fixtures/<phase>/expected/<case>.freecad.json
  fixtures/<phase>/expected/<case>.freecad.ledger.json
  reports/<phase>/<case>/collect.json
  reports/<phase>/<case>/repeat2.json
  candidates/<phase>/<case>/run-a/
  candidates/<phase>/<case>/run-b/
```

在 authority 闭合前，不把 input 提前写入正式 fixture 根，不把 role 提升为 `native`。

### 7.2 第一次采集

```bash
cd /Users/li/Chili3DProject/FreeCAD

python3 cad-core/tools/collect_freecad_expected.py \
  cad-core/out/fixture-staging/fixtures/<phase>/<case>.json \
  --fixtures-root cad-core/out/fixture-staging/fixtures \
  --out cad-core/out/fixture-staging/fixtures/<phase>/expected/<case>.freecad.json \
  --validate-ledger \
  --report cad-core/out/fixture-staging/reports/<phase>/<case>/collect.json \
  --freecadcmd /Users/li/.cargo/bin/FreeCADCmd
```

采集完成后必须确认 public expected 和 ledger 同时存在，且 ledger validation 通过。

### 7.3 promotion 前 staging 独立双跑

collector 必须实现不依赖 checked-in native manifest 的 staging repeat。该模式从同一个 staging input 启动两个独立 FreeCADCmd 进程，并写入两个独立 candidate run directory：

```bash
python3 cad-core/tools/collect_freecad_expected.py \
  cad-core/out/fixture-staging/fixtures/<phase>/<case>.json \
  --fixtures-root cad-core/out/fixture-staging/fixtures \
  --validate-ledger \
  --repeat 2 \
  --candidate-root cad-core/out/fixture-staging/candidates/<phase>/<case> \
  --report cad-core/out/fixture-staging/reports/<phase>/<case>/repeat2.json \
  --freecadcmd /Users/li/.cargo/bin/FreeCADCmd
```

该 CLI 是 E3 必须补齐的 collector 合同；不得用当前只接受 checked-in native authority 的 `--check --repeat 2` 冒充 staging repeat。

两次运行必须使用不同 candidate run directory 和独立 FreeCADCmd 进程。通过条件：

- 两次都实际处理选中的 case；
- public expected 规范化语义相等；
- 两次 ledger 都合法、自洽；
- 没有 missing output、skip 或 collection failure；
- 已记录 FreeCADCmd 路径、FreeCAD/OCCT 版本和 collector SHA；
- producer trace 保持 `not_evaluated`，除非本次明确调查 trace。

### 7.4 promotion

只有上述条件全部满足，才原子写入：

1. input；
2. public expected；
3. ledger；
4. role manifest；
5. collector producer report。

必须实现并统一使用 `cad-core/tools/promote_freecad_fixture_authority.py`。该工具先在事务临时目录完成五项产物、SHA、role 变更、staging repeat 回执和目标路径冲突检查；全部通过后再写入工作树。任一步失败都必须回滚本次写入并保持原 authority inventory 不变。

promotion 不允许留下“input 已进入正式根，但 expected/ledger/role 尚未闭合”的中间状态，也不允许手工复制五项产物代替 promotion 工具。

### 7.5 promotion 后 checked-in 复验

promotion 完成后，才允许对正式 fixture 路径执行现有 checked-in authority repeat：

```bash
python3 cad-core/tools/collect_freecad_expected.py \
  cad-core/fixtures/<phase>/<case>.json \
  --fixtures-root cad-core/fixtures \
  --check \
  --validate-ledger \
  --repeat 2 \
  --candidate-root /tmp/freecad-expected-<phase>-<case>-post-promotion \
  --report /tmp/freecad-expected-<phase>-<case>-post-promotion.json \
  --freecadcmd /Users/li/.cargo/bin/FreeCADCmd
```

该复验用于证明正式 input、expected、ledger 和 role 已闭合，不能替代 promotion 前 staging 双跑。

## 8. collector unsupported 的分类

采集失败必须归入以下一种：

1. **FreeCAD 不表达该 public 状态**：保留 `protocol_only` 或 `unsupported`；
2. **collector 缺少通用 TypeId/property 支持**：补 collector 通用能力和 focused test；
3. **fixture 不是 FreeCAD native 对象图**：不进入 native authority；
4. **本机 producer/build 不可用**：修复 producer 环境，不修改 expected；
5. **语义尚不明确**：保留 `unsupported`，记录调查入口。

只有第 2 类允许修改 collector。禁止按 phase/case 名称增加分支。

## 9. 分阶段实施

### E0：冻结当前 native inventory（已完成）

交付：

- 重建 775 个 input 和 480/14/281 role 分布；
- 检查 orphan expected、orphan ledger、重复 case；
- 生成当前 FreeCADCmd/collector provenance baseline；
- 明确每个非 native case 当前原因，不批量猜测。

扩容前置 Gate 已闭合：从剪枝方案冻结核心四 target、Material、Part、Sketcher、PartDesign、Mesh、Spreadsheet、Assembly、`OndselSolver` 和保留 Python/data 入口，并生成 module owner/capability inventory。最终 inventory 为 777 个输入、558/14/205 role 分布；保留闭包报告去重计入 765 个 fixture。

Gate：库存和 provenance 可由命令重建，zero-case 和缺 ledger 均 fail-closed。

### E1：闭合 collector 工具（已完成）

交付：

- 单 case、phase、all-native 选择；
- check-only candidate root；
- `--repeat 2` 独立进程重采；
- public expected、ledger 分栏 verdict，以及非阻断 provenance warning；
- producer trace 默认关闭；
- focused collector tests。

Gate：代表性单 case 和 phase 的两次重采通过；不调用任何 CAD Core/Rust/v2 actual source。该 Gate 只证明 collector 地基可用，不证明任何保留模块已经全覆盖。

### E2：复核 unsupported（已完成）

逐项分类为：

- collector 通用缺口；
- FreeCAD native 不表达；
- 非 native fixture；
- 尚未调查。

同一 FreeCAD 调用链、同一 TypeId/property 接口的案例组成一个最小完整语义批次。每批先在 staging 采集，闭合 authority 后再 promotion。

每个 fixture 必须映射到一个或多个 owner module，并标记 `inRetainedClosure`。涉及被删模块的 fixture 不能混入保留模块完成率；同时，涉及保留模块的 fixture 也不能因同时引用被删模块就从清单中静默消失，必须迁移输入或记录明确边界证据。

Gate：所有 unsupported 均有分类和 module owner；报告必须同时输出 `collectorImplementationQueue`、`retainedModuleCollectorImplementationQueue`、`stagingCandidateQueue`、`promotionQueue` 和 `blockedOrReclassified`。`promotionQueue` 为空不代表扩容完成，只要 `retainedModuleCollectorImplementationQueue` 非空就必须继续实施。

最终结果：205 个 unsupported 全部逐项闭合，其中 `freecad_native_not_expressible = 120`、`non_native_fixture = 85`；`collector_general_gap = 0`、`not_investigated = 0`。

### E3：清空通用 collector 能力实施队列（已完成）

当前 inventory 中的 `collector_general_gap` 是扩容工作的实施入口。先按 module owner 过滤出剪枝后保留闭包的 `retainedModuleCollectorImplementationQueue`，再按同一 FreeCAD 调用链、同一 TypeId/property 接口组成依赖闭合批次，逐批补 collector 通用能力和 focused tests；禁止按 phase/case 名称增加特判。

每批完成后必须重新生成 inventory：能表达 native public root 的 case 进入 `stagingCandidateQueue`；经实际 FreeCADCmd 证明不成立的 case，携带证据进入 `blockedOrReclassified`。

Gate：`retainedModuleCollectorImplementationQueue = 0`；保留模块范围内 `notInvestigatedCount = 0`、`nativeEligibleWithoutAuthorityCount = 0`；每个入队项都有 promotion 或证据化重分类结论。被删模块的 gap 不阻塞本方案，但必须保留为 out-of-scope inventory，不能伪装成已解决。

最终结果：通用 TypeId/property、Sketcher constraints、history-only ledger、SubShapeBinder 属性顺序、Material、Spreadsheet、事务化 promotion/revocation 和 non-CAD smoke 能力均已落地；三个 Gate 计数全部为 0。

### E4：按业务优先级 staging、promotion 并扩容 native expected（已完成）

必须逐模块推进，不允许只做 Part/Sketcher/PartDesign 后停止：

1. 运行时核心：FreeCADBase、FreeCADApp、FreeCADMainCmd、FreeCADMainPy；
2. Material；
3. Part：primitives、Boolean、Compound、保留的数据交换入口及其他已登记 Part capability；
4. Sketcher：几何、约束、外部引用和求解结果；
5. PartDesign：Body、Pad、Pocket、Revolution/Groove、dress-up/transformed、Loft/Pipe 及其他已登记 PartDesign capability；
6. Mesh；
7. Spreadsheet；
8. Assembly 与其实际调用的 OndselSolver 求解路径；
9. 保留的 Help/AddonManager 等 headless Python/data 入口，以 `non_cad_smoke` 收口。

这里的“扩容”只表示增加 FreeCADCmd native authority，不包含实现或修复任何 candidate runtime。

每个批次必须依次通过 staging 首采、staging repeat 2、事务化 promotion 和 promotion 后 checked-in repeat 2。每完成一个模块都生成模块级 receipt，但只有全部保留模块的 `coverageStatus` 闭合后才能完成 E4。禁止因 `nextNativeCandidates` 或 `promotionQueue` 暂时为空，或某几个大模块已经通过，就把 E4 报告为完成。

Gate：`nativeAfter > 480`；核心四 target、七个 App module、`OndselSolver` 和保留 Python/data 入口逐项出现在 `retained_module_fixture_coverage.v1.json`；所有 CAD module 为 `passed`，非 CAD 入口为 `non_cad_smoke`；所有 promotion case 五项产物闭合，且新增 case 的 staging/post-promotion 回执全部通过。

最终结果：`nativeAfter = 558`，相对 480 基线净扩容 78。FreeCADBase、FreeCADApp、FreeCADMainCmd、FreeCADMainPy、Material、Part、Sketcher、PartDesign、Mesh、Spreadsheet、Assembly 和 `OndselSolver` 全部为 `passed`；Help、AddonManager 以真实 FreeCADCmd `non_cad_smoke` 回执通过。新增 authority 均经过 staging 首采、staging repeat 2、事务化 promotion 和 checked-in repeat 2；最终 campaign 发现的不稳定 offset2d case 已事务化撤销，没有把不稳定 authority 留在 native 集合中。

### E5：扩容后全量复现收口（已完成）

交付：

- 全部 native cases 的 public expected 重采报告；
- 全部 ledger validation 报告；
- producer binary/collector provenance；
- role inventory；
- `retained_module_fixture_coverage.v1.json` 及逐模块 receipts；
- collector 使用说明。

Gate：以扩容后的 role manifest 重建 inventory；`nativeAfter > nativeBaseline`；剪枝后保留闭包的模块清单与 coverage report 完全一致；所有保留模块 fixture/capability 均闭合；所有 native case 都可由受控 FreeCADCmd 重建；public expected 语义相等；ledger 全部合法、自洽；`retainedModuleCollectorImplementationQueue = 0`；报告中没有 zero-case 假绿。

最终结果：`all-native-check.v1.json` 由两个独立 FreeCADCmd 进程各执行 558/558，0 failure、0 public semantic difference、0 run-to-run ledger drift、0 variation；`ledger-strict-validation.v1.json` 为 558/558 valid、0 error。`ledgerDriftStatus = drifted` 仅表示候选 ledger 相对部分历史签入 ledger 的 collector/tool hash 或内部证据变化；由于两个独立 run 间 ledger 一致、strict validation 全过且 public expected 语义相等，它是诊断项，不阻断本方案 Gate。

## 10. 测试和 CI

### PR 快速层

- collector 参数和选择范围单测；
- manifest/orphan/ledger 静态检查；
- 剪枝方案固定闭包与 module coverage manifest 一致性检查；
- fixture module owner/typeId/property/capability 映射完整性检查；
- candidate root 不得位于 checked-in fixture 根；
- single-case repeat 只比较选中 case；
- staging repeat 不依赖 checked-in expected/ledger，并严格比较 run-A/run-B；
- zero-case、缺 expected、缺 ledger、非法 JSON fail-closed；
- promotion 五项产物缺一、目标冲突或回执不完整时事务回滚；
- `git diff --check`。

### 合并层

- 受影响 case/phase 的 FreeCADCmd check；
- 新增 case 的 staging repeat 2 和 promotion 后 repeat 2；
- ledger strict validation；
- 最小 producer 身份记录检查；可选 provenance 缺失只报告 warning。

### 夜间层

- all-native FreeCADCmd check；
- 全量 ledger validation；
- 分批 repeatability campaign。
- `nativeAfter > 480`、`retainedModuleCollectorImplementationQueue = 0` 和全部保留模块 coverage verdict 检查。

CI 不运行 legacy cad-core、Rust FFI 或 freecad-kernel-v2 parity。

## 11. 实施与收口回执

### 11.1 已完成的 collector 地基包

提交 `2d0e4b8d36` 只完成 collector 地基：

1. 冻结 775/480/14/281 inventory；
2. 修通单 case、phase、all-native 的 check-only；
3. 修通单 case/phase/all-native 的 `--repeat 2`；
4. 必须报告 FreeCADCmd 路径、FreeCAD/OCCT 版本和 collector SHA；source/build、完整命令及产物 SHA 可选记录，缺失只 warning；
5. 分开 public expected、ledger validation、ledger drift、producer trace verdict；
6. 为 Box、Sketch、Body/Pad、Link 和 topology-state 各选一个代表 case 做独立重采；
7. 输出 collector unsupported 分类清单和下一批 native authority 候选。

首包不得修改或提交：

- `compare_freecad_expected.py`；
- `freecad_expected_parity/engine.py`、`sources.py`、`registry.py`；
- Rust FFI/v2 actual-source tests；
- runtime C++/Rust 实现；
- runtime e2e release Gate。

若工作区已存在这些越界 WIP，必须保护并与本包提交隔离，不能为让本包变绿而混入。

该提交证明 480 个既有 native authority 可复现，并完成 unsupported 分类，但没有增加 native 数量，因此不能作为本方案完成回执。

### 11.2 E2-E5 最终收口回执

最终实施已完成：

1. `fixture_authority_inventory.v1.json` 重建 777 个 input 和 558/14/205 role，所有 gap/未调查/无 authority 队列清零；
2. `retained_module_fixture_coverage.v1.json` 对 765 个保留闭包 fixture 建立 owner、TypeId/property/capability 和证据映射，所有 CAD module 通过；
3. 通用 collector 能力、staging repeat、事务化 promotion/revocation、strict ledger 和真实 non-CAD smoke 均有 focused tests；
4. Material 与 Spreadsheet 各新增一个真实 native fixture，Help 与 AddonManager 使用结构化 smoke 回执，不伪造 CAD expected；
5. `all-native-check.v1.json` 和 `ledger-strict-validation.v1.json` 完成扩容后全量复现；
6. 最终相关测试为 120 项全部通过，且没有运行 legacy cad-core、Rust FFI、freecad-kernel-v2 或 Web runtime parity；收尾 review 新增的 promotion SHA 绑定和 producer-report coverage 硬门禁均有 RED→GREEN 回归测试。

## 12. 完成标准

以下条件同时满足，本方案才完成：

1. 覆盖范围与剪枝方案完全一致：FreeCADBase、FreeCADApp、FreeCADMainCmd、FreeCADMainPy，Material、Part、Sketcher、PartDesign、Mesh、Spreadsheet、Assembly，`OndselSolver`，以及剪枝后保留的 headless Python/data 入口一个不少；
2. `retained_module_fixture_coverage.v1.json` 可从 live closure、fixture 和 authority 资产重建，所有条目都有 module owner、TypeId/property/capability 和证据；
3. 七个 App module 和运行时核心的 `coverageStatus = passed`，`OndselSolver` 通过 Assembly 实际求解路径覆盖，非 CAD Python/data 入口为 `non_cad_smoke`；
4. 保留模块范围内 `retainedModuleCollectorImplementationQueue = 0`、`notInvestigatedCount = 0`、`nativeEligibleWithoutAuthorityCount = 0`；
5. `nativeAfter > nativeBaseline`，当前 `nativeBaseline = 480`；该数量条件不能代替逐模块覆盖条件；
6. 所有具有 FreeCAD native public root 的保留模块 fixture 都是 `native`，并有同名 public expected + ledger；
7. 允许保留的非 native 例外全部逐 case 有实际证据，不能由批量静态猜测产生；
8. public expected 只能由受控 FreeCADCmd collector 生成；
9. expected/ledger 禁止手改，且有包含 FreeCADCmd SHA256 的最小 producer provenance；
10. 每个新增 native case 都通过 promotion 前 staging repeat 2 和 promotion 后 checked-in repeat 2；
11. promotion 通过统一工具事务化闭合 input、public expected、ledger、role manifest 和 producer report；
12. public expected、ledger validation、ledger drift、producer trace verdict 分开；producer trace 默认 `not_evaluated`；
13. explicit zero-case、missing expected、missing ledger 和 collection failure 均 fail-closed；
14. FEM、CAM、BIM、Draft、Import、Measure、TechDraw 不重新进入构建或覆盖范围；保留的 `Part::ImportStep` 按 Part capability 覆盖；
15. 不运行、不修复、不验收 legacy cad-core、Rust FFI、freecad-kernel-v2 或 Web runtime；
16. checked-in 报告能重建扩容后的 native inventory、逐模块 coverage、producer 身份和复现结论；任何保留模块未闭合或 native 数量仍为 480 时，不得宣称方案完成。

最终交付不是“证明某个 runtime 与 FreeCAD 一致”，而是：

> 为剪枝后保留的全部 FreeCAD2 模块建立可机读、逐模块闭合的 cad-core fixture native authority 覆盖；由 FreeCADCmd 独立生产、可复现、可追溯，并将 native authority 从 480 个基线实际扩容。

截至 2026-07-17，上述 16 条完成标准已全部满足，本方案状态为“已完成”。
