# 【已实现】P8-LinkAssemblyRuntime S6 Web Runtime 合同冻结

## 目标

冻结 CLI / C ABI / Worker / WASM / Web adapter 对 Link / Assembly runtime 的 request、response、diagnostics、resource limits、binary payload metadata 和 capability 合同。S6 是发布闸门，不承载新的建模语义。

## 必读

- S0-S5 的已实现结论、能力矩阵和 blocker queue。
- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/include/cad_core/adapters/c_api.h`
- `cad-core/tests/test_adapters.py`
- `cad-core/fixtures/c3m7/rect-pad-worker-mesh-limit.json`
- `cad-core/fixtures/c4m5/assembly-runtime-adapter-*.json`
- `docs/接口规定/` 中与 CAD Core adapter 相关的接口文档，如存在则必须同步。

## 实现要求

- Adapter 只透传 core schema、capability 和 diagnostics，不实现 Link、Assembly、topo naming 业务逻辑。
- Worker / WASM / Web 与 CLI / C ABI 对同一 fixture 的 core result、diagnostic code、capability key 和 binary metadata 保持一致。
- resource limits 覆盖 mesh、binary buffer、import/export payload、timeout / memory diagnostic。
- 发布文档必须明确 supported subset、remaining gaps、nonGoal 和 regression commands。

## 非目标

- 不实现前端 UI。
- 不在 Worker / WASM 内部保存几何状态。
- 不让 adapter 修正 subname、placement 或 Link ownership。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_adapters tests.test_p8_features tests.test_expected_fixtures
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线/工作步骤细分 --format markdown
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线 cad-core
```

## 完成结论

S6 已完成。当前 LinkAssembly runtime 对外发布合同冻结为 `cad-core-result-v1`，CLI / C ABI / Worker / WASM / Web 共享同一套 request-local core result。普通 recompute 顶层通道固定为 `results`、`elementReferenceUpdates`、`documentObjectUpdates`、`diagnostics`、`binaryPayloads`；Worker / WASM 只增加 adapter 标记，归一化后必须与 CLI / C ABI 等价。

Adapter 发布边界：

- `cad-core/src/adapters/c_api/c_api.cpp::capabilitiesJson()` 已发布 `adapters.contract_version`、`schema_parity`、`stateless_result_channels`、`resource_diagnostics`、Worker / WASM entrypoint、mesh streaming limits、binary mesh payload metadata、C API export 和 CLI export 能力。
- `cad-core/include/cad_core/adapters/c_api.h` 暴露 `cad_core_recompute_json`、`cad_core_worker_recompute_json`、`cad_core_wasm_recompute_json`、`cad_core_export_json`、`cad_core_mesh_binary_json`，adapter 只做协议转换和 buffer 管理。
- `cad-core/tests/test_adapters.py::test_c4s11_cli_c_api_worker_wasm_share_core_result_contract` 锁定 CLI / C ABI / Worker / WASM schema parity。
- `cad-core/tests/test_adapters.py::test_c4s11_adapter_resource_limit_diagnostic_preserves_result_schema` 和 `test_c3m7_worker_and_wasm_adapters_apply_streaming_mesh_limits` 锁定 `adapter_resource_limit` / `mesh_limit_exceeded` 诊断和 mesh streaming metadata。
- `test_c3m7_c_api_exports_binary_mesh_payload`、`test_c4s11_binary_mesh_payload_limit_reports_metadata_diagnostic`、`test_c_api_exports_recomputed_shape_buffers` 锁定 binary mesh metadata、`binary_payload_limits.max_bytes` 和 shape export buffer metadata。

S6 未修改 adapter 业务逻辑，也不新增 C++ 实现。Link、Assembly、topo naming、subname、placement 和 ownership 语义仍位于 app / topo / assembly / runtime 层；adapter 不合成 `elementReferenceUpdates` 或 `documentObjectUpdates`。

明确 future / nonGoal 边界：

- 当前没有 adapter 级 timeout diagnostic 或 deadline contract。
- 当前没有 adapter 级 memory diagnostic 或 memory quota contract。
- 当前没有通用 import/export payload byte quota 或 streaming export；已发布的是 shape export buffer metadata、CLI file export，以及二进制 mesh 的 `max_bytes` 限制。

验证结果：`cmake --build build` 通过；`python3 -m unittest tests.test_adapters tests.test_p8_features tests.test_expected_fixtures` 通过 175 个测试，跳过 14 个 known gap；队列工具返回空队列；`git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线 cad-core` 通过。
