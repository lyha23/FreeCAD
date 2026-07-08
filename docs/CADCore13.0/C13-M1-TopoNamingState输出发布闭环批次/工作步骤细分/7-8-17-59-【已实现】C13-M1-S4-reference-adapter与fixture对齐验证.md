# 【已实现】C13-M1 S4 reference adapter 与 fixture 对齐验证

## 目标

验证 S3 输出的 `topoNamingState` 可被下一次请求消费，并确认 CLI / C API / worker / wasm 共享同一正式 response channel。S4 只做 focused fixture 与 adapter/reference 验证，不扩大到全量 expected parity。

## live 基线

- `pwd`: `/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`: `a7aa502722`
- `git log -1 --oneline`: `a7aa502722 实现 C13-M1 S3 topoNamingState 发布器`
- `git -c core.quotepath=false status --short -uall`: 无输出，S4 开始前工作区干净。
- `step_goal_queue.py ... --format markdown`: S4 开始前队列为 S4、S5。

## 验证结果

- `cmake --build build`：通过。
- focused CLI 输出已刷新到 `cad-core/fixtures/<phase>/cad-core-res/<case>.cad-core.json`，未写入 `expected/`。
- 5 个 focused 输出均有顶层 `topoNamingState.schemaVersion == "cad-core.topo-state.v1"`。
- `python3 -m unittest tests.test_topo_naming_state_response`：通过，覆盖 official response 和 c4m6 response state 注入下一次请求后的 Body/Tip stable reference 不回退。
- `python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c13m1_cli_c_api_worker_wasm_share_topo_naming_state_channel`：通过，CLI / C API / worker / wasm 的 `topoNamingState` payload 一致。
- `python3 -m unittest tests.test_topo_state_fixture_migration`：通过。
- legacy output smoke：`CAD_CORE_TEST_LEGACY_OUTPUT=1` 下仍输出旧 `objects/mesh/subshapes/named_shapes` shape，且不带 `topoNamingState`。
- `python3 -m unittest tests.test_adapters.CadCoreAdapterTest || true`：已运行；当前 baseline 仍是 `6 failures, 8 errors`，失败范围为既有非 S4 blocker：缺失 p5 plane-frame expected、p8/export expected 缺 bbox、Body Tip stable-name 旧断言、legacy internal reference diagnostic 旧断言、c12m16 split fragment 旧断言。

## focused outputs

| fixture | result path | schema | diagnostics |
| --- | --- | --- | --- |
| `p2/rect-pad-pocket` | `cad-core/fixtures/p2/cad-core-res/rect-pad-pocket.cad-core.json` | passed | 0 |
| `c4m6/topo-state-body-tip-stable-recovery` | `cad-core/fixtures/c4m6/cad-core-res/topo-state-body-tip-stable-recovery.cad-core.json` | passed | 0 |
| `p5/sketch-internal-face` | `cad-core/fixtures/p5/cad-core-res/sketch-internal-face.cad-core.json` | passed | 0 |
| `p6/up-to-face-stable-body-history` | `cad-core/fixtures/p6/cad-core-res/up-to-face-stable-body-history.cad-core.json` | passed | 0 |
| `p8/app-link-box-face` | `cad-core/fixtures/p8/cad-core-res/app-link-box-face.cad-core.json` | passed | 0 |

## gap 分类

Focused expected 与当前 runtime 的 `documentHash` 和 focused target `objectHash` 均一致；`hash_encoding_gap` 本轮未复现，仍按合同保留为 broader parity 的 gap_allowed。

剩余差异不作为 S4 blocker：

- `freecad_mapped_name_encoding_gap`：expected 使用 FreeCAD raw mapped name（如 `#...:H...,F` / `#...:G;XTR...`），runtime 只发布当前 `NamedShape.elementMap` 稳定 token（如 `Pad.Edge1`、`ProbeSketch.Edge1`、`g1`）。S4 不从 expected 字符串反推编码器。
- `child_element_map_key_gap`：expected evidence 使用 `childElementMapKey` 字段；runtime 当前 focused child maps 为空，entry evidence 只发布 `source` 与 mapper indexes。
- `mapper_history_id_gap`：expected evidence 使用 `mapperHistoryIds` 且 mapper history 为空；runtime 发布 request-local `mapperHistory` array 与 `mapperHistoryIndexes`。这是当前 cad-core 投影，不宣称 FreeCAD id 字节级 parity。
- Runtime 还会发布额外 request-local named-shape objects（如 `Pad`、`Sketch.InternalShape`、`Box`），expected focused 文件通常只保留 target object。这是 S3 发布器的 state superset，不改变 target response 语义。

## 关闭条件

- `C13M1-BLOCKER-501` 已关闭。
- `C13M1-IMPL-007/008` 已从 awaiting_s4_sweep 推进为 S4 验证关闭。
- `VAL-101/102/103` 与 focused fixture outputs、adapter channel、round-trip、legacy branch 已记录到 validation matrix。
- 后续进入 S5 发布闸门与 mapped-name follow-up 拆分，不在 S4 扩大到全量 parity。
