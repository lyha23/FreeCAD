# C4N-S3-S1 producer tag deterministic ledger 实施步骤

## 目标

关闭 topoNamingState raw mapped-name 在 CLI / C API / worker / wasm 等入口之间的 producer tag 漂移，保持 C4N-S1/S2 focused parity 不回退。

## Step 1：基线确认

先确认 focused parity 仍绿：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_topo_naming_state_response
```

再确认 adapter 差异只聚焦在 topoNamingState / raw tag：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest \
  tests.test_adapters.CadCoreAdapterTest.test_c13m1_cli_c_api_worker_wasm_share_topo_naming_state_channel \
  tests.test_adapters.CadCoreAdapterTest.test_c4s11_cli_c_api_worker_wasm_share_core_result_contract
```

## Step 2：定位 tag 来源

检查 `cad-core/src/part/topo_shape.cpp` 中 request-local producer tag 的来源，尤其是是否依赖：

- `TopoDS_Shape` hash；
- 进程内地址或 OCCT 内部 hash seed；
- adapter 执行顺序；
- serialization 顺序之外的隐含状态。

如果差异只发生在 raw `:H...` 片段，不要先改 expected，也不要在测试里 canonical 掉 raw。

## Step 3：实现 deterministic ledger

实现位置优先：

- `cad-core/src/topo/freecad_mapped_name_codec.cpp`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/src/runtime/topo_naming_state.cpp`

要求：

- producer tag 由 request-local StringHasher / ledger 顺序派生；
- 同一请求、同一 graph、同一 producer family 在不同 adapter 下得到同一 raw mapped-name；
- refine / preserved pass 继续继承 source-backed raw/canonical evidence；
- indexed-only、split、deleted、ambiguous 不伪造 raw mapped-name。

## Step 4：补测试

优先扩展现有 adapter parity 测试，而不是新增 broad-suite 全量门槛：

- `test_c13m1_cli_c_api_worker_wasm_share_topo_naming_state_channel`
- `test_c4s11_cli_c_api_worker_wasm_share_core_result_contract`

必要时增加一个最小 request fixture，只比较 topoNamingState raw/canonical，不比较 unrelated mesh / bbox / product fields。

## Step 5：文档收口

更新：

- `docs/CADCore4.0_New/C4N-S3/矩阵/c4n_s3_scope.tsv`
- `docs/CADCore4.0_New/C4N-S3/矩阵/c4n_s3_blocker_queue.tsv`
- `docs/CADCore4.0_New/C4N-S3/矩阵/c4n_s3_fixture_intake.tsv`

验收：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_topo_naming_state_response
python3 -m unittest \
  tests.test_adapters.CadCoreAdapterTest.test_c13m1_cli_c_api_worker_wasm_share_topo_naming_state_channel \
  tests.test_adapters.CadCoreAdapterTest.test_c4s11_cli_c_api_worker_wasm_share_core_result_contract
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore4.0_New cad-core
```
