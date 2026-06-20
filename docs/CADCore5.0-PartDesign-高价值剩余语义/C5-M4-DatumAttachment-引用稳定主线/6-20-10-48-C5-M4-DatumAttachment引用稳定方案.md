# C5-M4 Datum Attachment 引用稳定方案

## 目标

C4 已把 active `AttachmentSupport` / `MapMode` 作为稳定 unsupported diagnostic 收口。C5-M4 的目标是判断哪些非 GUI AttachEngine map modes 对前端 CAD runtime 有价值，并把这些模式做成 expected-backed support；其余继续保持 locatable diagnostic。

## 范围

- Datum 源码依据：`src/Mod/PartDesign/App/DatumPoint.cpp`、`DatumLine.cpp`、`DatumPlane.cpp`、`DatumCS.cpp`。
- Attachment 源码依据：`src/Mod/Part/App/AttachExtension.cpp`、`AttachExtension.h`、`Attacher.cpp`、`Attacher.h`。
- link 依据：`src/App/PropertyLinks.cpp`。
- cad-core 落点：`cad-core/src/part_design/datum_*`、`cad-core/src/app`、`cad-core/src/runtime/recompute.cpp`、`cad-core/src/adapters/c_api/c_api.cpp`。
- 验收：`tests.test_p7_features`、`tests.test_adapters`；若采集 native expected，再加 `tests.test_expected_fixtures`。

## 产品 gate

M4 不是默认迁移完整 AttachEngine。进入 support 的 map mode 必须同时满足：

- 不依赖 GUI Attachment editor。
- 可由 request graph 的 `AttachmentSupport`、`MapMode`、`AttachmentOffset`、`Reverse`、`Parameter` 完整描述。
- 能用 native expected 或稳定 placement / downstream reference assertion 验收。
- 不引入跨请求 attachment session。

## 阶段

| 步骤 | 内容 |
| --- | --- |
| S0 | source audit：AttachExtension property、AttachEngine map-mode list、Datum engine type |
| S1 | product gate：选择 FlatFace / NormalToEdge / Inertial 等候选或保留 diagnostic |
| S2 | expected fixture / placement / downstream reference tests |
| S3 | capability metadata 和 remaining boundary 收口 |

## 非目标

- 不迁移 GUI Attachment editor、ViewProvider resize 或 task panel。
- 不支持完整 AttachEngine mode list，除非产品 owner 逐项接受。
- 不保存跨请求 attachment session。

## 验收

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义 cad-core
```

阶段回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_adapters
```
