# 【已实现】C13-M2 S4 mappedName codec 实现

## 目标

实现并正式关闭 FreeCAD focused raw/canonical `mappedName` codec/helper，让 runtime state publisher 只消费 source-backed producer ledger / codec 输出。

## 关闭结果

- live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=38be8d62a5`（`38be8d62a5 docs: 关闭 C13-M3 S5 发布闸门`），起点存在无关脏改：`DESIGN.md`、`docs/框架/7-9-15-53-FreeCADCmd权威账本与topoNamingState裁剪原则.md`、`docs/CADCore13.0/README.md`、未跟踪 `docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次/`；本步未暂存、未回退、未提交这些无关改动。
- C13-M3 S1-S4 已解除 producer-ledger 前置阻塞；本步复核当前 live code/tests 后确认 C13-M2 S4 不再需要新增 C++ 代码，只做正式状态收口。
- `cad-core/include/cad_core/topo/freecad_mapped_name_codec.h` 与 `cad-core/src/topo/freecad_mapped_name_codec.cpp` 已承接 FreeCAD `MappedName` / `ElementMap::encodeElementName()` focused raw/canonical codec，注释标明 FreeCAD 源文件、函数和关键字段。
- `cad-core/src/runtime/topo_naming_state.cpp` 的 top-level `elementMap.entries` 只发布 `MappedNameProvenanceStatus::SourceBacked`、raw/canonical 非空且包含 FreeCAD `#...;:H...` encoded token 的 provenance；缺 producer evidence 时保持 indexed-only，不从 `stableSubname`、display path 或 `fullSubname` 伪造 raw mapped name。
- focused live scope 已普通通过：`p2/rect-pad-pocket` 与 `c4m6/topo-state-body-tip-stable-recovery` 的 `mappedName.raw/canonical` parity 通过；`p5/sketch-internal-face` 与当前 `p8/app-link-box` 保持 indexed-only / no-fake-raw 边界；旧 `p6/up-to-face-stable-body-history` 仍是 retired fixture，不能作为 live parity 证据。
- `childElementMapKey` 与 `mapperHistoryIds` 仍属于 C13-M2 S5/S6；S4 只关闭 focused raw/canonical codec 和 runtime consumption，不把 key/id 标成 supported。
- 本步未改 expected JSON、未改前端、未扩大全量 expected parity、未新增 fixture/phase/object 特判。

## 验收记录

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
# 通过：cad-core-lib、cad_core_ffi、cad-core 与 probe targets 均 built。

python3 -m unittest tests.test_topo_naming_state_response
# 通过：Ran 15 tests in 3.885s OK，无 expectedFailure。

python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c13m1_cli_c_api_worker_wasm_share_topo_naming_state_channel
# 通过：Ran 1 test in 0.384s OK。

cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M2-FreeCADMappedNameParity实现批次/工作步骤细分 --format markdown
# S4 关闭前队列从 S4 开始；本步关闭后应从 S5 开始。

awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M2-FreeCADMappedNameParity实现批次/矩阵/*.tsv
# 通过：无 malformed TSV rows。

git diff --check
# 通过：无 whitespace errors。
```
