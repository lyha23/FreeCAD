# C12-M6 S3 input schema 准入验证

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
