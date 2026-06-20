# C5-S4 M4 Datum Attachment 引用稳定

## 目标

对 active `AttachmentSupport` / `MapMode` 做产品 gate：若进入 CAD Core 支持，补 selected map modes 的 expected-backed placement 与 downstream reference tests；否则保持稳定 unsupported diagnostics。

## 必读

- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M4-DatumAttachment-引用稳定主线/6-20-10-48-C5-M4-DatumAttachment引用稳定方案.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M4-DatumAttachment-引用稳定主线/矩阵/datum_attachment_scope.tsv`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M4-DatumAttachment-引用稳定主线/矩阵/datum_attachment_blocker_queue.tsv`
- `src/Mod/Part/App/AttachExtension.cpp`
- `src/Mod/Part/App/Attacher.cpp`
- `src/Mod/PartDesign/App/Datum*.cpp`

## 工作内容

- 记录 AttachExtension / AttachEngine 调用链和 Datum engine type 差异。
- 明确哪些 map modes 进入 product-supported slice，哪些保留 diagnostic。
- 补 fixture、diagnostics、focused tests 和 capability metadata。
- 保证 GUI Attachment editor 和跨请求 attachment session 不进入 cad-core。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义 cad-core
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_adapters
```

## 完成条件

- selected map modes supported 或所有 active map modes 都有 locatable diagnostics。
- capability metadata 不 overclaim full AttachEngine solver。
