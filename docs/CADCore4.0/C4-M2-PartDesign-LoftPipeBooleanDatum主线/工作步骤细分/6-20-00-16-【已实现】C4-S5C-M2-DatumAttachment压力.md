# C4-S5C M2 Datum / Attachment 压力

## 目标

仅在产品需要非 GUI Attachment 压力时执行。基于 C4-S5 已确认的 DatumPoint / DatumLine / DatumPlane / DatumCS placement、ReferenceAxis 和 Body Origin role relink 支持，补充 AttachEngine map-mode 压力 oracle 或稳定 unsupported diagnostic。

## 必读文件

- `src/Mod/PartDesign/App/DatumPoint.cpp`
- `src/Mod/PartDesign/App/DatumLine.cpp`
- `src/Mod/PartDesign/App/DatumPlane.cpp`
- `src/Mod/PartDesign/App/DatumCS.cpp`
- `src/Mod/PartDesign/App/Body.cpp`
- `cad-core/src/part_design/datum_*`
- `cad-core/src/part_design/body.cpp`

## 产物

- Datum Attachment pressure fixture / diagnostic rows。
- 若进入实现，补 collector、fixtures、tests、capability metadata。
- 若仍非产品目标，保持 GUI Attachment editor / visual resize non-goal。

## 非目标

- 不迁移 TaskAttachmentEditor、ViewProvider 或交互式编辑状态。
- 不引入跨请求 attachment session。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 完成记录

- Supported existing：DatumPoint / DatumLine / DatumPlane / DatumCS 继续只发布 placement shape、DatumLine / DatumCS downstream ReferenceAxis、Body Origin role relink，证据为 `p7/datum-coordinate-system-reference-axis`、`p7/datum-coordinate-system-sketch-support`、`c3m5/body-origin-link-placement`。
- Deferred diagnostic：新增 `cad-core/fixtures/c4m2/partdesign-datum-attachment-deferred-diagnostics.json`，覆盖 DatumPoint / DatumLine / DatumPlane / DatumCS 的 active `AttachmentSupport` + non-default `MapMode`，返回 `unsupported_property`，并带 `object` / `property` / `target` / `subname`。
- cad-core 落点：`cad-core/src/part_design/datum_attachment.h` 作为 Datum 私有 gate；四个 `datum_*` executor 在生成 shape 前拒绝 active AttachEngine map-mode 请求，避免静默 overclaim solver。
- Capability / 矩阵：`part_design.datum_attachment` 增加 c4m2 deferred fixture，移除 `datum_attachment_pressure_oracle` 剩余项，仅保留 `attachment_engine_map_mode_solver`；同包和全局 C4-ORC-203 / C4M2-LPBD-BLK-004 改为 done。
- Non-goal：TaskAttachmentEditor、ViewProvider、交互式 resize、跨请求 attachment session 和完整 AttachEngine map-mode solver 仍不迁移。
