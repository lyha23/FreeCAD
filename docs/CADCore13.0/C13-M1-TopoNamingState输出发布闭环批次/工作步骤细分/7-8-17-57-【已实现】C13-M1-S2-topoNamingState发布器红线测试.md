# 【已实现】C13-M1 S2 topoNamingState 发布器红线测试

## 目标

先用 focused tests 锁定 response state 发布与 round-trip 消费，再写 C++ 实现。

## live 基线

- `pwd`: `/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`: `c1cb3a0073`
- `git log -1 --oneline`: `c1cb3a0073 文档：关闭 C13-M1 S1 合同复核`
- `git -c core.quotepath=false status --short -uall`: 无输出，S2 开始前工作区干净。
- `step_goal_queue.py ... --format markdown`: S2 开始前第一项为 `7-8-17-57-C13-M1-S2-topoNamingState发布器红线测试.md`。

## 新增红线测试

### official response / round-trip

- 文件：`cad-core/tests/test_topo_naming_state_response.py`
- `test_c13m1_official_cli_response_publishes_body_topo_state_schema_gap_only`
  - 锁定正式 CLI response 必须包含顶层 `topoNamingState`。
  - 锁定 `topoNamingState.objects.Body.elementMap`、`subshapes`、`childElementMaps`、`mapperHistory` 的 schema。
  - 对 `documentHash`、`objectHash`、`mappedName.raw/canonical` 只断言字段类型和非空/结构，不要求 FreeCAD raw `#...:H...` 字节级一致。
- `test_c13m1_response_topo_state_round_trips_without_body_tip_recovery_regression`
  - 第一次 response 的 `topoNamingState` 放回 `c4m6/topo-state-body-tip-stable-recovery` 下一次请求。
  - 锁定 Body/Tip edge subshape 的 `identityStatus=stable` 和非空 `stableSubname` 不回退。

### adapter channel

- 文件：`cad-core/tests/test_adapters.py`
- `test_c13m1_cli_c_api_worker_wasm_share_topo_naming_state_channel`
  - 锁定 CLI、C API、worker、wasm 共享同一个顶层 `topoNamingState` channel。
  - worker / wasm 仍可保留 adapter 字段，但 `topoNamingState` payload 必须与 CLI 一致。

## 红线保护策略

上述 3 个测试均使用 `@unittest.expectedFailure`。这是 S2 的 guarded redline，不是 skip：

- 当前实现尚未发布 response `topoNamingState`，所以测试应以 expected failure 形式存在。
- S3 接入 runtime 发布器后，这些测试应变成 unexpected success。
- S3 必须移除对应 `expectedFailure` decorator，并让测试作为普通测试通过。
- 若 S3 后仍需保留 decorator，说明发布闭环没有真正完成，不能关闭 S3/S4。

## 关闭条件

- focused redline tests 已写入 `cad-core/tests/test_topo_naming_state_response.py` 和 `cad-core/tests/test_adapters.py`。
- implementation matrix 已把新增测试对应到 `C13M1-IMPL-004/005/007/008`。
- validation matrix 已记录 build、guarded redline 测试和当前 full adapter class baseline。
- blocker queue 中 `C13M1-BLOCKER-301` 已关闭，后续进入 S3。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_adapters.CadCoreAdapterTest
python3 -m unittest tests.test_topo_naming_state_response || true
cd ..
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/矩阵/*.tsv
git diff --check
```

本轮实际结果：

- `cmake --build build`：通过。
- `python3 -m unittest tests.test_topo_naming_state_response`：通过，`OK (expected failures=2)`。
- `python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c13m1_cli_c_api_worker_wasm_share_topo_naming_state_channel`：通过，`OK (expected failures=1)`。
- `python3 -m unittest tests.test_adapters.CadCoreAdapterTest`：已按要求运行；当前 baseline 仍有非 S2 引入的既有失败（6 failures, 8 errors, 1 expected failure），主要来自缺失 p5 expected、p8 expected summary 字段、既有 Body Tip stable name / legacy internal reference / split fragment 断言。S2 未修改这些无关测试、fixtures 或 C++。
