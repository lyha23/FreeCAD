# C8-M6 S5 capability 与前端消费边界发布

## 目标

把 S0-S4 复核过的合同发布成下游可消费的 source package：capability payload、diagnostics vocabulary、fixture seeds、request-local 输出和前端消费边界。

## 发布内容

- `part_design.shape_binder`：保持 C8-M1 expected-backed request-local supported。
- `part_design.sub_shape_binder`：保持 C8-M1 expected-backed request-local supported，保留 `copy_on_change_full_temporary_document_cache` known gap。
- diagnostics：发布 `copy_on_change_full_temporary_document_cache_not_supported`、`cycle_rejected_by_property_link`、generic `cycle_dependency` 的适用边界。
- fixture seeds：发布 C8-M1 12 个 input fixture 和 12 个 C8-M5 后 expected。
- output：发布 mesh / subshapes / full subname、ElementMap / NamedShape evidence、maker history、`documentObjectUpdates`、`elementReferenceUpdates` 的 request-local 合同。

## 前端消费边界

前端可以消费：

- capability status / covered / remaining_gaps / known_gaps / diagnostics。
- request-local mesh、subshape、full subname 和 reference update evidence。
- `documentObjectUpdates` 和 `elementReferenceUpdates` 建议。

前端不应消费为持久状态：

- full BREP。
- TopoDS_Shape。
- NamedShape / ElementMap 原始内核对象。
- temporary-document cache。
- request 结束后的 shape cache。

## 必须回写的矩阵行

- `C8M6-SYNC-101` 到 `C8M6-SYNC-105`
- `C8M6-SCOPE-301` 到 `C8M6-SCOPE-305`
- `C8M6-BLOCKER-501`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
./cad-core capabilities > /tmp/c8m6-s5-capabilities.json
python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics tests.test_adapters
```

## 非目标

- 不写前端 UI 代码。
- 不修改下游 adapter。
- 不增加 BREP 或 session state。
