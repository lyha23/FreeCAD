# C12-M6 S3 input schema 准入验证【已实现】

## 目标

验证 wire/wire 支持是否通过 request-local DocumentObject graph 表达，而不是 BREP、TopoDS、adapter shortcut 或 fixture-name branch。

## 必读来源

- `cad-core/fixtures/c4m1/part-ruled-surface-wire-wire.json`
- `cad-core/src/part/part_ruled_surface.cpp`
- `cad-core/src/document/*`
- `cad-core/tests/test_adapters.py` 的 `part_workbench.ruled_surface` capability assertion。

## 操作

1. 核对 payload key：`Objects[].TypeId`、`Objects[].Properties.Curve1.value`、`Curve1.SubList`、`Curve2.value`、`Curve2.SubList`、`Orientation.value`、`recompute.objs`。
2. 核对 wire producers 是否由当前请求图重算得出，不携带 full object BREP 或旧 shape cache。
3. 核对 `part_ruled_surface.cpp` 对 edge/wire inputs 的解析逻辑和 diagnostics；不得靠 compound hacks 把非 wire 输入伪装成 wire/wire。
4. 核对 capability JSON 中 `request_local_boundaries` 是否含 `source_shape_recomputed_from_document_graph` 和 `wire_wire_brepfill_shell`。

## 裁决规则

- schema 只能表达 request graph 和 link-sub；不允许引入 persistent TopoDS / NamedShape / ElementMap / full BREP。
- 若 schema 只靠 fixture 特例或 adapter 输出，S3 必须关闭为 `retained_validation_blocker`。
- 若 schema 成立但 docs/capability payload wording 漂移，S5 分类为 `publication_repair_required`。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
cad-core/build/cad-core capabilities | python3 -c 'import json,sys; print(json.dumps(json.load(sys.stdin)["part_workbench"]["ruled_surface"], ensure_ascii=False, indent=2))'
git diff --check
```

## S3 live baseline

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=eb7a856b15`。
- `git log -1 --oneline=eb7a856b15 docs: 完成 C12-M6 S2 expected 准入验证`。
- `git -c core.quotepath=false status --short -uall` 无输出，S3 起点为 clean。

## payload schema 复核结论

- `cad-core/fixtures/c4m1/part-ruled-surface-wire-wire.json` 的 target object 为 `Objects[2].TypeId=Part::RuledSurface`。
- `Curve1` 为 `App::PropertyLinkSub`，`value=LowerWire`；`Curve2` 为 `App::PropertyLinkSub`，`value=UpperWire`。本 fixture 没有 `SubList`，表示 whole wire object link；capability `payload_keys` 仍公开 `Curve1.SubList` / `Curve2.SubList` 作为可选 subelement link-sub key。
- `Orientation` 为 `App::PropertyEnumeration`，`value=Automatic`；current executor 也接受 `Forward` / `Reversed`，非法枚举进入 `unsupported_property` diagnostic。
- `recompute.objs=["RuledSurface"]`；临时 recompute smoke 输出 `diagnostics=[]`，result object 为 `RuledSurface`。
- `LowerWire` / `UpperWire` 均是同一请求图内的 `Part::RegularPolygon` wire producers；输入 JSON 没有 `BREP`、`TopoDS`、persistent `NamedShape`、persistent `ElementMap`、mesh 或旧 shape cache 字段。

## document/link 解析复核结论

- 当前 checkout 没有 `cad-core/src/document/*` 目录；对应 document 层落点是 `cad-core/src/app/document.cpp`、`document_object.cpp`、`property.cpp` 和 `property_links.cpp`。
- `parseDocument()` 只解析 JSON root、`Objects` 列表和 `recompute.objs`；没有 backend session、persistent document、old shape cache 或 full object BREP 入口。
- `parseDocumentObject()` 要求 `Name`、`ID`、`TypeId`、`Properties`，并把 properties 标准化到 `propertyValues` / dependency links。
- `parsePropertyValue()` 对 `App::PropertyLinkSub` 走 `readLinks()`；`readLinkObject()` 接受 `value` 和可选 `SubList` / `StableSubList` / `FullSubList` / request-local recovery evidence。S3 fixture 只使用普通 `value` link，不携带 `ReferenceShadow.brep`。
- `ReferenceShadow.brep` 是仓库已批准的单 subshape snapshot evidence channel；本 fixture 没有使用，也不构成 full BREP transport 或跨请求几何缓存。

## executor schema 复核结论

- `executePartRuledSurface()` 只允许 `Curve1`、`Curve2`、`Orientation`，先解析 orientation，再分别解析两个 link-sub curve。
- `resolveRuledSurfaceCurve()` 要求 exactly one linked object，且 subname 至多一个；从 `context.shapes[link.object]` 读取本次 recompute 已产生的 source shape，再按 `SubList` / stable subname 选择 subshape。
- executor 明确拒绝 missing link、invalid link、no edge 和非 edge/wire 输入；没有通过 fixture name branch、adapter shortcut、compound hack 或输出端修剪把非 wire 输入伪装为 wire/wire。
- wire/wire 的 shell 构造和 topo provenance 强度仍归 S4；S3 只关闭 request-local input schema。

## capability snapshot

`cad-core/build/cad-core capabilities | python3 -c 'import json,sys; print(json.dumps(json.load(sys.stdin)["part_workbench"]["ruled_surface"], ensure_ascii=False, indent=2))'` 输出：

- `status=supported_wire_wire_expected_backed`。
- `payload_keys` 包含 `Objects[].TypeId`、`Objects[].Properties.Curve1.value`、`Curve1.SubList`、`Curve2.value`、`Curve2.SubList`、`Orientation.value`、`recompute.objs`。
- `request_local_boundaries` 包含 `source_shape_recomputed_from_document_graph` 和 `wire_wire_brepfill_shell`。
- capability wording 未要求 full BREP transport；`BRepFill::Shell` 仅描述 OCCT runtime 构造分支，不是请求/响应 BREP 持久化合同。

## S3 裁决

- input schema 成立：request-local `DocumentObject graph` + `App::PropertyLinkSub` + `recompute.objs` 足以表达 wire/wire。
- 未发现 adapter-only 输出、fixture-name branch、persistent `TopoDS` / BREP / `NamedShape` / `ElementMap` / mesh cache 依赖。
- 未发现需要交给 S5 的 capability wording 漂移；S3 不标记 `publication_repair_required`。
- `C12M6-BLOCKER-301` 关闭为 `closed_s3`，`C12M6-CAT-004` 关闭为 `input_schema_admitted`。
- S4 shell/topo provenance gate 与 S5 publication gate 保持 open。

## 输出

- `c12m6_ruled_surface_wire_wire_input_schema_matrix.tsv` 全部 S3 schema 行更新为 `passed_s3`。
- `scope_review_matrix.tsv`、`backend_gap_classification.tsv`、`blocker_queue.tsv`、`source_candidates.tsv` 和 `validation_matrix.tsv` 已同步 S3 结论。
- 未修改 `cad-core/src`、fixtures、expected、tests、adapters 或 capability source。

## 下一步

下一步为 S4 shell/topo provenance gate：复核 current implementation 是否真正进入 `BRepFill::Shell(Wire, Wire)`，并裁决 `element_history_status` / source edge relation 是否足以正式关闭 provenance admission。S4 不应由 S3 schema 结论代替。
