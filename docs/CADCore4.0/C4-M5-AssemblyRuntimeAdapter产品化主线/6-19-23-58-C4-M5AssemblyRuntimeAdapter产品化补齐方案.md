# C4-M5 Assembly / Runtime / Adapter 产品化补齐方案

## 目标

C4-M5 在不让 adapter 承载建模语义的前提下，补齐前端运行时需要的 Assembly、runtime resource、mesh export 和 adapter contract 能力。

## 当前基线

C3.0 已覆盖：

- grounded Fixed / Revolute / Slider / Ball / Distance / Angle 真实 Ondsel path。
- invalid grounded movement validation。
- placement writeback lifecycle。
- representative fallback。
- CLI / C ABI / Worker / WASM / streaming mesh / binary mesh 共享 core recompute。

## 目标内缺口

| 方向 | 说明 |
| --- | --- |
| JointType 扩展 | 只接收前端产品需要且 FreeCAD / Ondsel 路径明确的 JointType |
| non-grounded / underconstrained diagnostics | 不把 representative fallback 当 solver 成功；需要稳定 diagnostic 和 fallback metadata |
| placement writeback 压力回归 | 多 component、部分失败、unchanged no-op、下一次 request graph 应用后稳定 |
| resource limits | mesh、binary payload、worker / wasm memory 和 time diagnostics |
| adapter contract | adapter 只透传 schema、capability 和 diagnostics，不新增建模语义 |

## 实施批次

| 批次 | 内容 | 验收重点 |
| --- | --- | --- |
| C4-M5-S1 | JointType 产品目标审计 | FreeCAD / Ondsel source path、DTO、unsupported diagnostics |
| C4-M5-S2 | Assembly validation 扩展 | contradictory constraints、underconstrained、partial writeback |
| C4-M5-S3 | Runtime resource diagnostics | mesh limit、binary buffer、worker / wasm parity |
| C4-M5-S4 | Adapter contract freeze | C ABI / Worker / WASM schema parity、capability publication |

## 非目标

- 不实现跨请求 Assembly session。
- 不把 representative fallback 宣称为 full solver。
- 不在 Rust Web、Worker 或 WASM adapter 中实现几何 / topo naming 业务语义。

## 验收

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore4.0 cad-core
```

阶段回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_adapters
```

## 可执行包入口

- Assembly：`docs/CADCore4.0/C4-M5-AssemblyRuntimeAdapter产品化主线/工作步骤细分/6-20-00-11-【已实现】C4-S10-M5-Assembly目标审计与实现.md`
- Runtime / Adapter：`docs/CADCore4.0/C4-M5-AssemblyRuntimeAdapter产品化主线/工作步骤细分/6-20-00-12-【已实现】C4-S11-M5-RuntimeAdapter契约补齐.md`
- validation 矩阵：`docs/CADCore4.0/矩阵/cadcore4_validation_matrix.tsv`
