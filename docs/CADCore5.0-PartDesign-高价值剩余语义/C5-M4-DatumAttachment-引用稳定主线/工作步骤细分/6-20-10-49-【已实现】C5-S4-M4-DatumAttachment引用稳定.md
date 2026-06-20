# 【已实现】C5-S4 M4 Datum Attachment 引用稳定

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

## 实施结果

- FreeCAD 调用链已记录到 `../6-20-10-48-C5-M4-DatumAttachment引用稳定方案.md`：DatumPoint/Line/Plane/CS 分别安装 `AttachEnginePoint/Line/Plane/3D`；`AttachExtension::positionBySupport()` 通过 `MapMode`、`MapReversed`、`MapPathParameter`、`AttachmentOffset` 调用 `calculateAttachedPlacement()`，并可能写回 `AttachmentSupport` subnames；`PropertyLinks.cpp` 用 `ShadowSub` / element reference map 维护 downstream 引用。
- product gate：本轮不发布 selected map mode support。`FlatFace`、`ObjectXY/ObjectX/ObjectOrigin`、`NormalToEdge` 等候选都继续 diagnostic-backed，因为 cad-core 尚无 request-local AttachEngine placement solver 和 subname writeback 闭环。
- supported existing：无 active attachment 的 Datum placement、DatumLine/DatumCS 下游 ReferenceAxis、Body Origin datum role relink。
- diagnostic-backed：active `AttachmentSupport`、`MapMode`、`AttachmentOffset`、`MapReversed` / Reverse、`MapPathParameter` / Parameter、`ShadowSub` evidence；下游引用 attached Datum 时稳定跳过且不产生引用写回。
- 新增 `cad-core/fixtures/c5m4/partdesign-datum-attachment-mapmode-diagnostics.json`、focused tests 和 capability metadata；M4/global scope/oracle/blocker/validation 矩阵已同步，`C5-BLK-401` 与 `C5M4-DAT-BLK-*` 已关闭为 diagnostic gate。
