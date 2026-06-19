# C4-S11 M5 Runtime / Adapter 契约补齐

## 目标

加固 CLI / C ABI / Worker / WASM / mesh / binary payload 的 schema parity、resource diagnostics 和 capability publication。Adapter 只做协议转换，不承载建模语义。

## 必读文件

- `docs/CADCore4.0/C4-M5-AssemblyRuntimeAdapter产品化主线/6-19-23-58-C4-M5AssemblyRuntimeAdapter产品化补齐方案.md`
- `docs/CADCore4.0/矩阵/cadcore4_validation_matrix.tsv`
- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/src/part/shape_exporter.cpp`
- `cad-core/tests/test_adapters.py`
- `cad-core/fixtures/c3m7/rect-pad-worker-mesh-limit.json`

## 产物

- Adapter/resource contract matrix rows。
- Tests for C ABI / Worker / WASM schema parity when relevant。
- Diagnostics for mesh/resource/binary payload limit。
- Capability metadata synchronization。

## 非目标

- 不在 adapter 中解析或修正 FreeCAD business semantics。
- 不改变 core recompute schema without tests。
- 不扩大 Web/Rust adapter scope unless local repo contract requires it。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_adapters
```

## 完成口径

Adapter capability 和 tests 能证明所有 exposed entrypoints 复用同一 core result contract；resource failures 有稳定 diagnostics。

## 完成记录

- Schema parity：CLI / C ABI / Worker / WASM 复用 `cad-core-result-v1`，结果通道固定为 `results`、`elementReferenceUpdates`、`documentObjectUpdates`、`diagnostics`、`binaryPayloads`。
- Resource diagnostics：Worker / WASM mesh limits 保持 `mesh_limit_exceeded`；无效 adapter resource limit 和 binary payload 超限返回 `adapter_resource_limit`，不把错误降级成裸 FFI error string。
- Binary payload：`cad_core_mesh_binary_json` metadata 固定 `protocol=cad-core-binary-mesh-v1`，并支持 `binary_payload_limits.max_bytes` 元数据诊断。
- Capability：`capabilitiesJson().adapters` 发布 `contract_version`、schema parity、resource diagnostics、Worker/WASM result contract 和 binary payload limits。
