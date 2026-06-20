# P8-LinkAssemblyRuntime S6 Web Runtime 合同冻结

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
