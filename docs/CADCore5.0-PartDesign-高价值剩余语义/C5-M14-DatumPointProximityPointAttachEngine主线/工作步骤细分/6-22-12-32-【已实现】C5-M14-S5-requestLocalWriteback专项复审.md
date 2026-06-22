# 【已实现】C5-M14-S5 request-local Writeback 专项复审

状态：`s5_request_local_boundary_reviewed_pending_S6`

## 目标

确认 `ProximityPoint1/2` 实现不会破坏 CAD Core 无状态边界，尤其是双 support 解析、`ReferenceShadow` / `StableSubList` 恢复、`documentObjectUpdates` / `elementReferenceUpdates` 和 capability wording。S5 只冻结发布前边界，不改 C++、fixtures 或 capability supported；代码、oracle、focused tests 和 capability adapter test 留到 S6。

## FreeCAD 依据

- `AttachExtension::positionBySupport()` 在 FreeCAD 文档对象内 mutates `AttachmentSupport` sub values。
- cad-core 不能保存文档 session，只能在一次 recompute 响应中返回 `documentObjectUpdates` / `elementReferenceUpdates`。
- `PropertyLinkSub/List::getSubValues()` 是 request 输入证据，不是后端长期状态。

## 当前代码边界

- `cad-core/src/part_design/datum_attachment.h` 已有 `resolveAttachmentSupport()`，会按 `SubList`、`StableSubList`、`ShadowSub`、`ReferenceShadow` 的 request-local evidence 解析单个 support。
- `appendAttachmentSupportWriteback()` 只把恢复结果写入 `context.documentObjectUpdates`，不修改输入 graph，也不保存跨请求状态。
- 当前 `usesLineFamilySupports` 只覆盖 `TwoPointLine`、`IntersectionLine`、`ProximityLine`，其它 selected Datum 默认 `supportCount=1`；因此 S6 必须让 DatumPoint `ProximityPoint1/2` 消费前两个有效 `AttachmentSupport` link。
- `AttachmentOffset` 已在 selected placement 之后统一右乘，S6 不应把 offset 混入 proximity helper。
- `MapReversed` 目前只传给已有 line/plane placement 方向构造；S6 不给 DatumPoint proximity 新增 line/plane reverse 语义。

## cad-core 复审点

| 点 | 要求 |
| --- | --- |
| support count | S6 让 `ProximityPoint1/2` 消费前两个有效 `AttachmentSupport` link；当前代码只有 line-family 多 support，其它 selected Datum 默认只消费 1 个 |
| subshape resolution | 每个 support 独立解析 subname / stable / shadow / `ReferenceShadow` request-local evidence，不共享解析缓存 |
| writeback | 若 support recovery 发生，只返回 request-local `documentObjectUpdates` / `elementReferenceUpdates` 建议 |
| state boundary | 不保存 cross-request backend attachment session、shape cache 或完整 BREP |
| ReferenceShadow | `ReferenceShadow.brep` 仍只允许作为单 subshape 旧快照证据，不能成为完整对象建模输入 |
| AttachmentOffset | 继续由 selected Datum placement 之后统一乘 offset |
| MapReversed | DatumPoint proximity 不新增 line/plane reverse 语义 |
| capability | S6 才新增 `DatumPoint ProximityPoint1/2 selected MapMode`；adapter 只发布 contract metadata，不承载业务修正 |

## 必须回写的矩阵行

- `C5M14-SCOPE-301`
- `C5M14-BLK-301`
- `C5M14-ORC-301`
- `C5M14-VAL-301`
- `C5M14-NG-003`、`C5M14-NG-004`

## S6 交付边界

- 在 `datum_attachment.h` 扩展 support family 判定，使 `ProximityPoint1/2` 读取前两个有效 support，而不是沿用单 support selected Datum 路径。
- 两个 support 分别调用现有 request-local subname / stable / shadow 解析；任一 support recovery 只能生成 response update 建议。
- proximity placement helper 只消费解析后的 `SupportResolution`，不保存 attachment session、shape cache、完整 BREP，也不把 cad-core 输出回填成 expected。
- `ReferenceShadow.brep` 保持单 subshape 旧几何快照证据的例外边界。
- focused tests 需要覆盖 response-only writeback；capability adapter test 等 S6 移除 exact blocker 后再运行。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'documentObjectUpdates|appendAttachmentSupportWriteback|ReferenceShadow|StableSubList|ProximityPoint' cad-core/src/part_design/datum_attachment.h cad-core/tests
rg -n 'cross-request backend attachment session|cad-core-output-derived expected|C5M14-NG-003|C5M14-NG-004|documentObjectUpdates' docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M14-DatumPointProximityPointAttachEngine主线
```

S6 capability 发布后再运行 `tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts`。

## 非目标

- 不增加 backend attachment session。
- 不保存 shape cache、完整 BREP 或跨请求 AttachmentSupport 解析状态。
- 不把 response update 当作持久状态；前端若采用 update，必须作为下一次 request graph 的显式输入。
- 不在 adapter 层修正 business semantics。
