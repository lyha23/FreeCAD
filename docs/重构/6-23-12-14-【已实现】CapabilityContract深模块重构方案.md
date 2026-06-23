# Capability Contract 深模块重构方案

## 来源与结论

本方案来自 `/var/folders/5k/fms98vy54k18w9n0j5_53r400000gn/T/architecture-review-20260623-120740.html` 的 Top recommendation：`Move the capability contract out of the adapter`。

架构评审给出的判断是 Strong：当前 `cad-core/src/adapters/c_api/c_api.cpp` 里的 `capabilitiesJson()` 不只是 C FFI adapter glue，它还保存了 DocumentObject graph、link property、Sketcher、PartDesign、Assembly、topo history、WireJoiner、known gaps、fixture status 等大量 domain capability facts。结果是 adapter module 的 interface 被迫承载 domain contract，`tests/test_adapters.py` 也变成了多个 domain module 的事实验收入口。

当前结论：下一轮应优先把 capability contract 移入 `cad-core-lib` 内的 deep module，让 C API / CLI / worker / wasm adapter 只负责发布 contract，而不是继续拥有 contract。这个改动不改变外部 JSON contract，不扩大建模能力，只做 module depth 和 locality 修正。

## 当前基线

当前 capability contract 集中在 `cad-core/src/adapters/c_api/c_api.cpp`：

- `capabilitiesJson()` 直接调用 `runtime::buildDefaultRegistry()`、`part::supportedShapeFileFormats()`、本地 `diagnosticCodeList()` 和本地 `cadCoreVersionJson()`。
- 同一个函数内拼出 `document`、`link_transaction`、`link_reference_lifecycle`、`sketcher`、`part_design`、`part_workbench`、`wire_joiner`、`topo_history`、`assembly`、`adapters`、`known_gaps` 等 contract。
- 多个 helper，例如 `ondselSolverCapabilityJson()`、`representativeSolverCapabilityJson()`、`assemblyValidationCapabilityJson()`、`placementWritebackCapabilityJson()`，也在 C API adapter 文件内表达 domain 状态。
- `cad_core_capabilities_json()` 只是把 `capabilitiesJson()` 包进 `makeJsonResult()`，真正复杂度不属于 buffer / FFI adapter。
- `cad-core/tests/test_adapters.py::test_c_api_capabilities_exposes_web_contract_facts` 对 contract 做大段断言，导致 adapter test 同时约束 Document、Sketcher、PartDesign、Assembly、topo history 和 WireJoiner 状态。

这说明当前 shallow interface 不是 `cad_core_capabilities_json(void)`，而是整个 C API adapter 文件。删除 test 或拆 adapter 不会消除复杂度，只会把 domain facts 转移到别处。正确方向是做一个有 depth 的 `Capability Contract` module。

## FreeCAD / cad-core 依据

本轮依据不是新增 FreeCAD 行为，而是保持 `cad-core` 已有无状态 contract：

- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/adapters/c_api/c_api.cpp::capabilitiesJson()`：当前 contract 的事实来源，必须作为迁移前后 JSON parity 基线。
- `/Users/li/Chili3DProject/FreeCAD/cad-core/include/cad_core/adapters/c_api.h::cad_core_capabilities_json()`：C ABI 只暴露发布入口，不应承接 domain facts。
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/runtime/recompute.cpp::recomputeResultJson()`：capability contract 描述的主结果 producer，属于 runtime/domain contract，不属于 adapter 私有事实。
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/part/part_geometry_curve.cpp::partGeometryCurveResultJson()`：当前仍是并行结果 producer，contract 可以如实发布它，但不要在 adapter 中保存它的业务状态。
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/runtime/feature_registry.cpp::buildDefaultRegistry()`：supported TypeId 列表来自 runtime registry，应由 contract module 消费。
- `/Users/li/Chili3DProject/FreeCAD/cad-core/src/part/shape_exporter.cpp::supportedShapeFileFormats()`：export format 事实来自 Part export module，应由 contract module 消费。

FreeCAD 源文件依据只用于已有 contract 条目的相邻注释保留，例如 link lifecycle、external geometry、Assembly solver、PartDesign feature history。迁移时这些注释可以跟随对应 JSON 片段进入新 module，不能丢失。

## 目标 module

新增 `runtime/Capability Contract` module：

- `cad-core/include/cad_core/runtime/capability_contract.h`
- `cad-core/src/runtime/capability_contract.cpp`

这个 module 承接：

- 生成完整 `cad-web-v1` capability JSON。
- 维护 cad-core version / kernel / registry / export formats / diagnostic codes 的 contract projection。
- 维护 Document、link lifecycle、Sketcher、PartDesign、Part Workbench、WireJoiner、topo history、Assembly 和 adapter publication facts。
- 把 domain facts 组织成局部 helper，让每个 helper 贴近对应 module 语义，而不是贴近 C FFI 文件。
- 保留现有 JSON 字段名、数组顺序、状态字符串、remaining gaps 行为和注释依据。

重构后的 adapter 角色：

- `cad-core/src/adapters/c_api/c_api.cpp` 只保留 buffer 分配、错误封装、C ABI entrypoint、export / recompute request 转换。
- `cad_core_capabilities_json()` 调用 `runtime::capabilityContractJson()` 或同名函数后返回 `makeJsonResult()`。
- CLI / worker / wasm 如果后续需要 capability publication，也调用同一个 module，不复制 C API 的 JSON builder。

## 分层落点

- `runtime/capability_contract.*`：deep module，承接 capability contract 的 domain-owned implementation。
- `runtime/diagnostics.*`：继续定义 diagnostic code 类型；如需要公开 code list，优先通过 runtime helper 暴露，不在 adapter 中手写。
- `runtime/feature_registry.*`：继续提供 supported TypeId，contract module 只消费。
- `part/shape_exporter.*`：继续提供 export formats，contract module 只消费。
- `adapters/c_api/c_api.cpp`：保留 C ABI adapter，不再拥有 capability facts。
- `adapters/cli/cli.cpp`：本轮不强行新增 capabilities 命令；若后续发布，只接同一 module。
- `tests/test_adapters.py`：继续保护 C API publication parity，但新增更窄的 contract/module test surface，避免所有事实只通过 adapter test 兜底。

## 实施步骤

### S0：冻结当前 contract

先把 `cad_core_capabilities_json()` 当前输出当作迁移基线：

- `schema_version` 仍是 `cad-web-v1`。
- `cad_core.api` 仍是 `cad_core_ffi`，除非另开版本 contract 方案。
- `supported_type_ids` 顺序与 `buildDefaultRegistry().typeIds()` 一致。
- `export_formats` 仍是 `["brep", "step", "stl"]`。
- `diagnostic_codes` 字段、`known_gaps`、`remaining_gaps`、各模块 `status` 字符串不漂移。
- `adapters.schema_parity.core_result_producers` 继续如实列出 `runtime::recomputeResultJson` 和 `part::partGeometryCurveResultJson`，不在本轮处理并行 PartConicCurve request path。

本步建议用一次当前 FFI 输出快照或 focused Python assertion 作为迁移前检查，不需要新增 fixture expected。

### S1：新增 CapabilityContract public interface

新增头文件和实现文件：

```cpp
namespace cad_core::runtime {

nlohmann::json capabilityContractJson();
nlohmann::json cadCoreVersionJson();

}
```

如果后续需要把 adapter-specific metadata 分开，可增加轻量 options：

```cpp
struct CapabilityContractOptions {
    std::string apiName = "cad_core_ffi";
};
```

第一轮不要把整个 adapter context 传进去。module 自己消费 runtime registry、diagnostic definitions、part export metadata 等稳定来源。

### S2：搬迁 version / diagnostic / top-level contract helper

从 `c_api.cpp` 移出：

- `cadCoreVersionJson()`。
- `diagnosticCodeList()`。
- `capabilitiesJson()` 的 top-level skeleton。
- `supported_type_ids`、`export_formats`、`diagnostic_codes` projection。

迁移后 `c_api.cpp` 中 `cad_core_version_json()` 和 `cad_core_capabilities_json()` 只调用 runtime contract module，再做 `makeJsonResult()`。

### S3：按 domain locality 搬迁 contract sections

把大 JSON 先按局部 helper 拆到 `capability_contract.cpp`：

- `documentCapabilityJson()`：DocumentObject graph、link property fields、document reference fields、external geometry lifecycle。
- `linkTransactionCapabilityJson()`：copy-on-change、writeback properties、request-local link transaction。
- `linkReferenceLifecycleCapabilityJson()`：StableSubList、ShadowSub、ReferenceShadow、elementReferenceUpdates。
- `sketcherCapabilityJson()`：solver-facing contract、external/internal pressure。
- `partDesignCapabilityJson()`：Body chain、Pad/Pocket、Revolution/Groove、Hole、DressUp、Pattern、Datum Attachment 等。
- `partWorkbenchCapabilityJson()`：conic curves、offset、ProjectOnSurface、RuledSurface、Loft、Sweep、Filling、GeomPlate 等。
- `wireJoinerCapabilityJson()`：WireJoiner diagnostic / history contract。
- `topoHistoryCapabilityJson()`：ElementMap、MapperHistory、maker history matrix。
- `assemblyCapabilityJson()`：Ondsel adapter、representative fallback、validation、placement writeback。
- `adapterCapabilityJson()`：只描述 entrypoints、schema parity、stateless result channels、resource diagnostics、mesh binary publication。

本步只移动，不重命名字段，不折叠 JSON。这样 review 可以用 diff 判断是否只有 ownership 改变。

### S4：缩小 C API adapter

迁移后 `c_api.cpp` 应满足：

- 不再出现 `capabilitiesJson()` 的 domain helper。
- 不再直接写 Sketcher / PartDesign / Assembly / WireJoiner capability facts。
- 可以保留 C ABI / buffer / request parse / export adapter 相关 helper。
- `cad_core_capabilities_json()` 的复杂度和 `cad_core_version_json()` 同级：try/catch + `makeJsonResult(runtime::capabilityContractJson())`。

这是本轮 deletion test：删除 C API adapter 的 capability helper 后，domain complexity 应集中到 `runtime/capability_contract.cpp`，而不是散落到 CLI 或测试。

### S5：测试面从 adapter-only 调整为 contract-first

保留现有 `test_c_api_capabilities_exposes_web_contract_facts`，但建议补更窄的测试：

- C API publication smoke：断言 `cad_core_capabilities_json()` 可返回完整 JSON，且 top-level version / schema / adapter entrypoints 正确。
- Contract parity test：通过 C++ probe 或更小的 binding 调用 `runtime::capabilityContractJson()`；若当前 Python 无法直接调 C++，短期可以先保留 C API test，并把大段 domain assertions 拆成 helper functions，命名上表达它们在验证 contract 而不是 adapter。
- Regression assertions：重点保护 JSON 字段、状态字符串和空数组，不靠 fixture mesh/bbox。

第一轮不要为了测试新增新的 public C ABI；如果没有现成 C++ test harness，可以先通过 C API 覆盖 publication parity，后续再补 core-level probe。

### S6：CMake 与 include 收口

- 把 `src/runtime/capability_contract.cpp` 加入 `cad-core-lib` source list。
- 让 `cad_core_ffi` 继续只链接 `cad-core-lib`，不新增 domain include。
- `c_api.cpp` include `cad_core/runtime/capability_contract.h`，删除不再需要的 `feature_registry.h`、`shape_exporter.h`、`diagnostics.h` 等 include。
- 若 `cadCoreVersionJson()` 迁出后 CLI `--version` 需要 JSON version，后续 CLI 可复用同一个 `runtime::cadCoreVersionJson()`；本轮不必改变 CLI 输出格式。

## 非目标

- 不改变 `cad_core_capabilities_json()` 的 JSON contract。
- 不调整 `/cad/recompute`、C ABI、worker、wasm 的 result contract。
- 不处理 `partGeometryCurve` 并行 request path；该项属于 HTML 里的 `Retire the parallel PartConicCurve request path`，应另开 speculative 方案。
- 不把 capability contract 拆进每个 domain module 的 public interface；第一轮先从 adapter 移到 runtime deep module，避免大范围 churn。
- 不重新定义 capability status、known gaps、fixture rows 或阶段能力结论。
- 不新增 backend session、缓存、动态 feature discovery。

## 风险与控制

- 风险：JSON 字段顺序、字段名或空数组行为漂移。控制：S0 先冻结 C API 输出；S3 只搬迁，不做字段整理。
- 风险：`runtime/capability_contract.cpp` 变成新的大杂烩。控制：helper 按 domain locality 分组；adapter facts 只放 `adapterCapabilityJson()`。
- 风险：为了追求纯粹，把 contract facts 分散进太多 module，导致 interface 扩张。控制：第一轮只新增一个 deep module，不要求每个 domain module 暴露 capability builder。
- 风险：C API adapter include 删除过头，影响 export/recompute path。控制：S4 后跑 C API adapter focused tests，而不是只跑 capabilities。
- 风险：把已有 FreeCAD 依据注释丢掉。控制：迁移 JSON 片段时携带相邻注释，必要时在新 helper 上方保留原路径和关键短句。

## 后续候选边界

同一个 HTML 还列出四个候选，本轮不混做：

- `Hide WireJoiner's diagnostic ledger behind depth`：强候选，但会触碰 `part/wire_joiner.*` 与 Sketch internal builder 的 ledger interface，需单独方案。
- `Collapse reference recovery into one deep module`：本仓库已有 `6-23-11-37-【已实现】ReferenceResolution深模块重构方案.md`，除非代码又漂移，不重复开启。
- `Deepen the request-local recompute state`：会改 `ComputeContext` 和 50 多个 include，适合作为后续阶段性重构，不应和 adapter contract 搬迁同轮。
- `Retire the parallel PartConicCurve request path`：评审标为 Speculative，且当前 comments 明确避免伪造 `Part::Hyperbola` / `Part::Parabola` DocumentObject；需要先重新确认 domain model。

## 验收命令

### 本轮短跑

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
python3 -m unittest tests/test_adapters.py
git diff --check
```

若只完成 S1-S3，可先跑 capability focused case 和 `git diff --check`，再补一次完整 adapter test。

### 阶段回归

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests/test_diagnostics.py tests/test_feature_flows.py tests/test_p5_sketch.py tests/test_p7_features.py
python3 -m unittest tests/test_adapters.py
```

### 重型收口

仅在同时改动 contract 内容、known gaps、fixture status 或多个 domain capability section 时执行：

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
python3 -m unittest discover -s tests
```

## 推荐顺序

优先执行 S0-S4。理由是当前最明确的 leverage 在 adapter seam：`cad_core_capabilities_json()` 的 public interface 很小，但 implementation 持有大量 domain facts。先把这些 facts 移入 `runtime/Capability Contract` deep module，可以立刻提升 locality，并为后续 CLI / worker / wasm 共享 capability publication 留出稳定 interface。
