# 【已实现】C13-M1 S3 runtime 发布器 C++ 实现

## 目标

实现 `runtime/topo_naming_state` 模块，并在正式 response 中输出 `topoNamingState`。

## live 基线

- `pwd`: `/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`: `4d076d83c5`
- `git log -1 --oneline`: `4d076d83c5 测试：关闭 C13-M1 S2 topoNamingState 红线`
- `git -c core.quotepath=false status --short -uall`: 无输出，S3 开始前工作区干净。
- `step_goal_queue.py ... --format markdown`: S3 开始前第一项为 `7-8-17-58-C13-M1-S3-runtime发布器C++实现.md`。

## 实现落点

- 新增 `cad-core/include/cad_core/runtime/topo_naming_state.h`，公开 `topoNamingStateJson(document, context, responseSubshapesByObject)`。
- 新增 `cad-core/src/runtime/topo_naming_state.cpp`，发布 `cad-core.topo-state.v1` 顶层 state。
- `cad-core/src/runtime/recompute.cpp::recomputeResultJson()` 缓存每个 target 的 `responseSubshapes()`，结果 payload 与 topo state 共用同一份 subshape identity。
- `cad-core/CMakeLists.txt` 把 `src/runtime/topo_naming_state.cpp` 纳入 `cad-core-lib`。
- `cad-core/src/adapters/cli/cli.cpp` 的官方 parse-error payload 也带空 `topoNamingState`；legacy test output 分支保持旧格式。
- `cad-core/src/runtime/capability_contract.cpp` 和 `cad-core/tests/test_adapters.py::CORE_RESULT_CHANNELS` 把 `topoNamingState` 纳入官方 stateless result channels。
- S2 三个 `@unittest.expectedFailure` 与 guarded redline 过期注释已删除，测试改为普通通过。

## schema 与边界

- 顶层输出：`schemaVersion`、`producer`、`documentHash`、`objects`。
- `producer` 优先继承输入 `topoNamingState.producer`，否则使用 runtime defaults 和当前 OCCT kernel version。
- `documentHash` / `objectHash` 使用稳定 JSON 投影加 `sha256:`；C13-M1 只承诺非空与稳定，collector 字节级一致仍是 `hash_encoding_gap`。
- object payload 包含 `objectHash`、`elementMapVersion`、`subshapes`、`elementMap{encoding,status,entries}`、`childElementMaps`、`mapperHistory`。
- target object 的 `subshapes` 来自缓存后的正式 response subshape array；非 target 但本轮有 `NamedShape` 账本的 object 使用 `NamedShape.elements` 索引投影，保证 round-trip state 不丢 Sketch/InternalShape 证据。
- `elementMap.entries` 只从 `NamedShape.elementMap` 和当前 mapper history projection 生成；`Body.FaceN -> FaceN` 这类 indexed-only alias 不写 entry。没有实现 FreeCAD raw `#...:H...` mapped-name encoder，也没有改 fixtures/collector/frontend。

## 关闭条件

- S2 focused tests 已普通通过。
- `cad-core` 构建通过。
- 没有在 executor/adapter 层新增 fixture 特判。
- `C13M1-BLOCKER-401` 已关闭，后续进入 S4 adapter/reference/fixture 对齐验证。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
build/cad-core recompute fixtures/p2/rect-pad-pocket.json --output out/c13m1-rect-pad-pocket.result.json
jq '.topoNamingState.schemaVersion, (.topoNamingState.objects | keys)' out/c13m1-rect-pad-pocket.result.json
python3 -m unittest tests.test_topo_naming_state_response
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c13m1_cli_c_api_worker_wasm_share_topo_naming_state_channel
python3 -m unittest tests.test_adapters.CadCoreAdapterTest || true
cd ..
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/矩阵/*.tsv
git diff --check
```

本轮实际结果：

- `cmake --build build`：通过。
- `build/cad-core recompute fixtures/p2/rect-pad-pocket.json --output out/c13m1-rect-pad-pocket.result.json` + `jq '.topoNamingState.schemaVersion, (.topoNamingState.objects | keys)' ...`：通过，schemaVersion 为 `cad-core.topo-state.v1`，objects 包含 `Body`、`Pad`、`Pocket`、`SketchPad`、`SketchPad.InternalShape`、`SketchPocket`、`SketchPocket.InternalShape`。
- `python3 -m unittest tests.test_topo_naming_state_response`：通过，`OK`。
- `python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c13m1_cli_c_api_worker_wasm_share_topo_naming_state_channel`：通过，`OK`。
- `python3 -m unittest tests.test_adapters.CadCoreAdapterTest || true`：已按要求运行；当前仍为既有 baseline `6 failures, 8 errors`，失败范围与 S2 记录一致，未作为 S3 新 blocker。
