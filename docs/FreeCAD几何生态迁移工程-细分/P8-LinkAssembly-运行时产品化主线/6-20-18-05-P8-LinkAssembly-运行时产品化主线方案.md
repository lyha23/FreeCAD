# P8 Link / Assembly 运行时产品化主线方案

本文是 `docs/FreeCAD几何生态迁移工程-细分` 下的新 P8 大功能模块方案。它不继续推进单个 JointType、单个 Datum 或单个 fixture 的细节修补，而是把当前 P8 未完成的 Link / Assembly / Web runtime 边界合并成一个可排队、可验收、可发布的产品化主线。

## 主线裁决

下一阶段优先实现 `P8-LinkAssembly-运行时产品化主线`。

本主线承接 `docs/CADCore方案/00-CAD-Core抽取方案.md` 中 P8 已完成的基础能力：

- 基础 `App::Link` / `LinkSub` / `LinkGroup` / `LinkElement` display。
- `ElementCount` 折叠数组、`PlacementList` / `ScaleList` / `VisibilityList` 基础消费。
- `ShowElement` 请求内子元素合成与 `documentObjectUpdates` 建议。
- `PropertyXLink*` / `FullSubList` / mapped postfix alias。
- `App::DocumentObjectGroup` plain group 展开。
- Assembly display、Joint 输入元数据、Fixed / Revolute / Cylindrical / Slider / Ball / Distance / Angle request-local placement writeback。

本主线专门补齐仍不应拆成小补丁的 P8 后置边界：

- 完整 FreeCAD Link 账本。
- `ShowElement=true` LinkElement / LinkGroup 持久写回事务生命周期。
- cross-document 文档 hash / postfix / alias 生命周期。
- 多层 LinkSub / LinkGroup / plain group / imported shape ElementMap 链。
- Assembly solver 剩余 JointType 与完整 Joint placement / constraint。
- Worker / WASM / Web adapter 的 schema、resource diagnostics 和 capability 合同冻结。

## 最小完整语义批次

本主线的最小完整语义批次不是“先修一个 ShowElement case”或“再补一个 JointType”。正确批次是：

1. 以 `DocumentObject graph` 为唯一真实数据，建立 Link ledger、child ownership、writeback proposal、cross-document reference、subname retag 和 adapter contract 的共同边界。
2. 先冻结 Link / LinkSub / LinkGroup 的事务生命周期，再让 Assembly solver 消费稳定的 component identity、subshape reference 和 placement chain。
3. Worker / WASM / Web adapter 只消费同一套 core schema，不承载 Link、Assembly 或 topo naming 业务语义。
4. 每个 supported claim 必须同时有 FreeCAD source authority、cad-core 落点、fixture/oracle、focused test、capability/docs 和下一次 request graph 应用后的稳定性证明。

## 当前基线

| 方向 | 当前状态 | 本主线处理 |
| --- | --- | --- |
| Link display | 已有基础 display、scale、subshape、group、ElementCount、ShowElement request-local 子元素建议 | 升级到完整账本和持久事务生命周期 |
| Link writeback | 已有 `documentObjectUpdates` 建议，但不代表完整 FreeCAD transaction ledger | 明确 owner / child / delete / reclaim / copy-on-change / touched / placement list 优先级 |
| Cross-document | 已有 `PropertyXLink*`、外部文档 missing / pending / hash mismatch fixtures 的基础表达 | 补齐 document hash、postfix alias、reload、rename、mapped name 生命周期 |
| 多层 LinkSub | 已有 object / label / target prefix 和部分 nested case | 补齐多层 LinkSub、plain group、LinkGroup、imported shape ElementMap 联合链 |
| Assembly solver | 已有 request-local real Ondsel 子集和代表 JointType | 在 Link graph 稳定后扩展剩余 JointType、underconstrained / contradictory diagnostics 和 full placement chain |
| Worker / WASM / Web | 已有 adapter 子集和资源 diagnostic 材料，但总览仍把产品化合同列为未完成 | 冻结前端可消费的 request / response / diagnostics / capability / binary payload contract |

## FreeCAD 依据入口

| 语义 | FreeCAD 源码候选 | 本主线使用方式 |
| --- | --- | --- |
| Link 基础属性与 child cache | `/Users/li/Chili3DProject/FreeCAD/src/App/Link.cpp`、`src/App/Link.h` | `_ChildCache`、`_LinkOwner`、`_LinkTouched`、CopyOnChange、ShowElement 和 child ownership 的 source authority |
| Link property / label / element reference | `/Users/li/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp`、`src/App/PropertyLinks.h` | `PropertyLink*`、`PropertyXLink*`、label references、element references 和 external document 状态 |
| DocumentObject group | `/Users/li/Chili3DProject/FreeCAD/src/App/DocumentObjectGroup.cpp` | plain group / group child 展开与 LinkGroup child routing |
| Assembly object / solver | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp` | Joint dispatch、marker placement、placement writeback 和 solver lifecycle |
| Assembly link / group | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyLink.cpp`、`JointGroup.cpp` | Assembly display graph、group ownership 和 JointGroup diagnostics |
| Assembly helpers | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp`、`AssemblyUtils.h` | DistanceType、JCS current value、placement scalar 和 remaining JointType 分类 |

S1 必须把上述候选继续精确到类、函数、关键字段和短句，再允许实现步骤引用为 source authority。

## cad-core 落点

| 层 | 代码落点 | 本主线职责 |
| --- | --- | --- |
| app / document | `cad-core/include/cad_core/app/link*.h`、`cad-core/src/app/link.cpp`、`property_links.cpp` | Link ledger、child ownership、subname routing、cross-document link diagnostics |
| runtime | `cad-core/include/cad_core/runtime`、`cad-core/src/runtime` | `documentObjectUpdates`、`elementReferenceUpdates`、diagnostics、capability 聚合，不持久保存 graph |
| topo / part | `cad-core/src/part`、`cad-core/src/topo` | imported shape ElementMap、mapped postfix、source-prefixed stable key 和 Link retag history |
| assembly | `cad-core/include/cad_core/assembly`、`cad-core/src/assembly` | AssemblyLink、AssemblyObject、JointGroup、Joint solver DTO、placement writeback |
| adapters | `cad-core/src/adapters/c_api/c_api.cpp`、CLI / Worker / WASM adapter | schema / capability / diagnostics 透传，不新增建模语义 |
| fixtures / tests | `cad-core/fixtures/p8`、`cad-core/fixtures/c3m2`、`cad-core/fixtures/c3m6`、`cad-core/fixtures/c4m5`、`cad-core/tests` | oracle、expected、focused tests 和发布能力证明 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 主线总入口 | `6-20-18-05-P8-LinkAssembly-运行时产品化主线总入口.md` | 后续执行入口和队列索引 |
| 工作步骤总入口 | `工作步骤细分/6-20-18-05-【已实现】P8-LinkAssemblyRuntime工作步骤总入口.md` | S0-S6 执行顺序，索引文件已建立 |
| source candidates | `矩阵/p8_link_assembly_runtime_source_candidates.tsv` | FreeCAD / cad-core 源码候选 |
| scope review | `矩阵/p8_link_assembly_runtime_scope_review_matrix.tsv` | 最小完整语义批次与支持声明 |
| blocker queue | `矩阵/p8_link_assembly_runtime_blocker_queue.tsv` | 发布前必须关闭的 blocker |
| backend gap classification | `矩阵/p8_link_assembly_runtime_backend_gap_classification.tsv` | backendGap / oracleFirst / releaseGate / nonGoal 聚合 |
| non goal registry | `矩阵/p8_link_assembly_runtime_non_goal_registry.tsv` | 不进入本主线或不得发布的边界 |

## 实施顺序

1. S0 live 基线与旧队列裁决：复核 `00-CAD-Core抽取方案.md`、C4-M5、旧 P8 Assembly / Marker / Distance / JointType 子包和当前 fixtures，确认哪些是 baseline、哪些是 stale queue、哪些仍是后置 gap。
2. S1 FreeCAD source authority：把 Link / PropertyLinks / AssemblyObject / AssemblyUtils / JointGroup 的候选精确到类、函数、字段、关键短句和 cad-core 落点。
3. S2 Link ledger 与 ShowElement 持久事务：实现或补齐 owner / child / sync / delete / reclaim / copy-on-change / touched / placement list 优先级和 writeback proposal。
4. S3 cross-document hash / postfix / alias 生命周期：补齐 missing / unloaded / pending / hash mismatch / reload / rename / mapped postfix 诊断与更新建议。
5. S4 多层 LinkSub 与 imported ElementMap：补齐 nested LinkSub、LinkGroup、plain group、imported shape ElementMap、source-prefixed stable key 和 retag history。
6. S5 Assembly solver 扩展：在稳定 Link graph 上推进剩余 JointType、underconstrained / contradictory diagnostics、full placement chain 和 writeback stress。
7. S6 Worker / WASM / Web 合同冻结：统一 CLI / C ABI / Worker / WASM / Web schema、capabilities、resource limits、binary payload metadata 和 docs 发布。

## 非目标

- 不实现 FreeCAD GUI、TaskPanel、ViewProvider、drag session 或跨请求 Assembly solver session。
- 不把 BREP、NamedShape、ElementMap、mesh、MBD solver state 或 Worker cache 作为长期后端状态保存。
- 不在 adapter、前端或输出层补 Link、topo naming、Assembly solver 业务语义。
- 不把 representative fallback、FreeCAD TODO/default branch 或 unsupported diagnostic 发布为 supported。
- 不从 cad-core 当前输出倒推 FreeCAD expected。
- 不继续扩展单 fixture 特判、bbox 猜测、输出顺序猜测或几何类型排序补丁。

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线/工作步骤细分 --format markdown
```

阶段回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_adapters tests.test_expected_fixtures
```

重型收口：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_mvp tests.test_diagnostics tests.test_feature_flows tests.test_adapters tests.test_p5_sketch tests.test_p6_topology tests.test_p7_features tests.test_p8_features tests.test_expected_fixtures
```
