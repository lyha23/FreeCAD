# 【已实现】C5-M19-S5 capability 与 requestLocal 边界复审

状态：`done_capability_boundary`

## 结论

capability 现在表达两件事：已支持的 `IntersectionLine` 保持 supported；不可选的 `TangentU`、`TangentV`、`IntersectionPoint` 不再作为 implementable exact blocker，而是 source-audited non-goal。

## capability 更新

| 项 | C5-M19 处理 |
| --- | --- |
| supported | 保留 `DatumLine IntersectionLine selected MapMode` |
| exact blockers | 删除 `datum_attach_engine_remaining_modes` |
| non-goals | 新增 source-audited enum-only `TangentU/TangentV/IntersectionPoint` |
| remaining gaps | 继续为空 |
| fixtures | 不新增 C5-M19 fixture |
| diagnostics | 不新增 C5-M19 diagnostic |

## request-local 边界

- `AttachmentSupport`、`MapMode`、`AttachmentOffset`、`MapReversed` 仍来自单次 request graph。
- response 只返回 placement、diagnostics、`documentObjectUpdates` / `elementReferenceUpdates` suggestions。
- 不新增 backend attachment session、shape cache 或完整 BREP 状态。
- `ReferenceShadow.brep` 仍只是单 referenced subshape snapshot 例外。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
cd /home/user/Chili3DProject/FreeCAD
rg -n 'source-audited non-executable AttachEngine enum-only modes|datum_attach_engine_remaining_modes|DatumLine IntersectionLine selected MapMode' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py
```

## 非目标

- 不改 adapter 协议。
- 不新增 C ABI route。
- 不把 docs-only non-goal 发布成 runtime diagnostic。
