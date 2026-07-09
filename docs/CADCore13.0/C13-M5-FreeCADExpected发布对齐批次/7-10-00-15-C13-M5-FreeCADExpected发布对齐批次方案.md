# C13-M5 FreeCADExpected 发布对齐批次方案

## 背景

C13-M1 到 C13-M4 已经让 `topoNamingState` 能进入 cad-core response，并用 `c4m6` 证明了 public projection、child path projection 和 ledger sidecar 的边界。但当前仍存在一个更大的发布问题：`cad-core` 的 release output 还没有系统性对齐所有 `fixtures/<phase>/expected/*.freecad.json`。

本批次把 `.freecad.json` 定义为 release parity 目标。它不是把 FreeCADCmd 厚账本搬进 runtime，也不是从 fixture 字符串反推业务逻辑，而是建立一个固定流程：

1. 发现 native expected。
2. 用当前 cad-core 生成同名 `cad-core-res`。
3. 用稳定 comparator 规范化非语义漂移。
4. 把剩余 diff 归类。
5. 按 FreeCAD 源码和 cad-core 模块边界修实现。
6. 重生成 cad-core-res 并关闭 phase。

## 当前基线

S0 live inventory 已冻结：当前发现到的 `.freecad.json` phase 数量是 42 个，expected 文件总数是 475 个。它们覆盖 Part primitives、Sketch/InternalShape、PartDesign Body/DressUp/Pattern、Assembly/App::Link、Pipe/Sweep/Loft、TopoNamingState 等多条语义线。所有 expected 都有同名 input `fixtures/<phase>/<case>.json`，也都有同名当前输出 `fixtures/<phase>/cad-core-res/<case>.cad-core.json`。部分 phase 的 `cad-core-res` 数量大于 expected，说明发布对齐必须按 expected discovery 驱动，不能把 `cad-core-res` 目录里的额外文件自动算作缺口。

`c4m6` 是首批 strict lane，因为它已暴露当前 release publication 的代表性缺口：

- 只发布前端最低需要对象，而不是 expected 中完整 public object set。
- mapperHistory 数量和发布策略与 expected 不一致。
- document/object hash mismatch 当前 hard fail，而 native expected 有可重算输出。
- Link compound 已发布 child path projection 后，仍可能留下 `missing_stable_subname` 诊断和 result 差异。

这些缺口属于 public publication / protocol policy，不应通过手改 expected、删除 cad-core-res 字段或 adapter 输出修剪解决。

## 对齐原则

1. `fixtures/<phase>/expected/*.freecad.json` 是 native expected 权威输入；除非 collector 或 native oracle 被证明错误，否则不修改。
2. `fixtures/<phase>/cad-core-res/*.cad-core.json` 是当前 cad-core 输出，必须由 `build/cad-core recompute` 重生成。
3. `.freecad.ledger.json` 只用于证明 expected 由 FreeCADCmd 账本裁剪而来，不作为 cad-core runtime 输入。
4. `*.expeted.json` 不纳入本批次自动 parity；如果需要继续保留协议合同，走单独 audit。
5. `raw` 里的 `:H...` token 可随机漂移，只在 comparator 层 canonicalize；canonical key、stableSubname、subshape path、diagnostic code、object publication set 不因此放宽。
6. 几何误差只允许在明确字段上使用容差，例如 bbox 浮点微差；拓扑数量、elementMap key、ReferenceShadow 边界和 diagnostic 语义不能用容差掩盖。
7. 每个实现缺口必须落到 FreeCAD 对应语义模块：`runtime` 只做 response assembly 和 publication，`topo` 做命名账本，`geometry` 做 OCCT 低层能力，`features` 做 FreeCAD feature 调用链。
8. 当前无关 dirty 文件 `DESIGN.md` 和 `docs/框架/7-9-15-53-FreeCADCmd权威账本与topoNamingState裁剪原则.md` 不属于 C13-M5 S0，不纳入本批次 diff 或提交。

## 字段策略冻结

- strict：public object set、diagnostic code、results key、subshape path/type/count、stableSubname、mappedName.canonical、canonical elementMap key、childElementMaps key、mapperHistory public event identity、ReferenceShadow 边界。
- canonicalized：`mappedName.raw` 等 raw FreeCAD token 中的随机 `:H...` 片段，只在 comparator 内规整，不改 expected 和 runtime。
- tolerant：明确为数值输出的 bbox、placement、matrix、volume、area、length 等浮点字段，只允许小容差，不允许掩盖拓扑数量或诊断差异。
- ignored-with-evidence：只存在于 `.freecad.ledger.json` sidecar、collector coverage/projection/roundTrip/inputReferences 等 provenance 字段，或 cad-core response 中非 native public expected 的 transport/adapter metadata；忽略时必须能指向证据来源。

## 实现框架

### S0 inventory 与比较边界冻结

产出：

- 已完成 native expected inventory：42 个 phase、475 个 expected、同名 input 完整、同名 cad-core-res 完整，cad-core-res extra 只记录不参与 discovery。
- 已冻结字段策略：strict、canonicalized、tolerant、ignored-with-evidence 四类。
- 已冻结首批 lane：`c4m6` strict public expected。

关闭条件：

- 不再依赖人工翻目录判断 expected 覆盖面。
- 明确排除 `*.expeted.json`、`.freecad.ledger.json` 和 cad-core-res 额外文件。
- 当前 dirty 工作区中无关文件不进入本批次。

### S1 strict comparator 与生成入口

落点建议：

- `cad-core/tools/compare_freecad_expected.py`
- `cad-core/tools/regenerate_cad_core_res.py`
- `cad-core/tests/test_freecad_expected_public_parity.py`

能力：

- `--phase <phase>`：只比较该 phase 的 `.freecad.json`。
- `--case <name>`：只比较单个 case。
- `--write-current`：按 expected discovery 重生成同名 `cad-core-res/<case>.cad-core.json`。
- 输出机器可读 gap report，例如 `cad-core/out/freecad-expected-parity/<phase>.json`。
- 默认 canonicalize raw mapped-name hash，但不 canonicalize canonical key。

关闭条件：

- `c4m6` 能稳定产出 strict diff report。
- comparator 的 normalization 有单测覆盖。
- 同名 expected 缺失 input、cad-core 执行失败、diagnostic-only response 都能结构化记录。

### S2 c4m6 strict public parity 红灯基线

目标不是立即把所有差异改绿，而是把当前 strict 缺口变成可执行清单：

- object publication set：`Body/Pad/Sketch`、`ChildBoxA/ChildBoxB/Compound` 等 expected object 是否发布。
- mapperHistory：保留哪些事件、哪些只是 cad-core 内部 indexed history。
- request failure policy：schema/producer/encoding hard fail 与 document/object hash mismatch 是否同属 hard fail。
- Link compound diagnostics/results：child path 已发布时，是否仍应报 `missing_stable_subname`。

关闭条件：

- 每个 c4m6 strict diff 都有 owner file、FreeCAD source authority、实现路径或协议决策。
- 测试不再只验证前端最低合同，而是新增 strict expected parity 测试。

### S3 topoNamingState 发布策略对齐

优先修改 cad-core runtime publication：

- `cad-core/src/runtime/topo_naming_state.cpp`：发布完整 public object set，不只发布 result targets。
- `cad-core/src/runtime/topo_naming_state.cpp`：为 `mapperHistory` 增加 expected-facing publication policy，避免把内部 indexed history 全量泄漏为 public state。
- `cad-core/src/runtime/topo_naming_state.cpp` / `cad-core/src/runtime/recompute.cpp`：重新裁决 hash mismatch 与 stableSubname publication diagnostics。
- `cad-core/tests/test_topo_naming_state_response.py` 或新 strict parity test：锁住 c4m6 strict expected。

关闭条件：

- `c4m6` strict expected green，或者剩余项被明确登记为协议 intentional divergence。
- 若协议 intentional divergence 存在，需要单独文档说明为什么 release output 不对齐 native expected，不能静默保留红灯。

### S4 phase family 扩展

不做“一次性全量修完”。按家族推进：

1. TopoNamingState / ElementMap / App::Link：`c4m6`、`p8`、`c3m1`。
2. Sketch / InternalShape / split fragment：`c10m1`、`c12m16`、`p2`、`p6`。
3. Part primitives / boolean / sweep / loft / pipe：`p8`、`c3m4`、`c12m12`、`c12m13`。
4. PartDesign Body / dress-up / pattern / hole：`c3m5`、`p7`、`c5*`、`c51*`。
5. Assembly / App::Link / placement：`c3m6`、`p8`。

每个家族必须先用 comparator 输出 diff 分组，再决定实现批次；不能从某个 case 的 JSON 形状直接倒推 C++ 规则。

### S5 release gate 收口

每个 phase 标记 green 必须同时满足：

- expected discovery 中的每个 `.freecad.json` 都有同名 cad-core-res。
- strict comparator green，或剩余 gap 有 known-gap id 和 FreeCAD/OCCT 依据。
- 相关 focused tests 普通通过。
- `git diff --check` 通过。
- README/矩阵记录 phase 状态、验证命令和剩余风险。

## 非目标

- 不手改 `expected/*.freecad.json`。
- 不把 `cad-core-res` 的额外输出当 expected 目标。
- 不把 `.freecad.ledger.json` 接入 cad-core runtime。
- 不把 `*.expeted.json` 和 `.freecad.json` 混成同一个 expected discovery。
- 不通过 adapter 输出修剪、fixture 名称分支或 JSON 字符串复制实现 parity。
- 不默认全量 FreeCADCmd 重采；只有 collector 或 expected 证据错误时才重采。
- 不在本批次一次性实现所有 feature geometry 缺口；本批次先建立 release gate 和推进顺序。

## 验收命令

### 文档包

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次/矩阵/*.tsv
git diff --check -- docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次 docs/CADCore13.0/README.md
```

### 首批实现短跑

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build --target cad-core
python3 tools/compare_freecad_expected.py --phase c4m6 --write-current
python3 tools/compare_freecad_expected.py --phase c4m6 --strict
python3 -m unittest tests.test_topo_naming_state_response tests.test_freecad_expected_public_parity
```

`compare_freecad_expected.py` 和 `tests.test_freecad_expected_public_parity` 是 S1/S2 计划产物；S0 文档阶段不要求存在。
