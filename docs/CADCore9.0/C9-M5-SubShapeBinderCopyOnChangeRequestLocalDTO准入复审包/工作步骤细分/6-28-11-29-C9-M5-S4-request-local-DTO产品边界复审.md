# C9-M5-S4 request-local DTO 产品边界复审

## 目标

根据 S3 native evidence 判断是否存在可进入 cad-core 的 request-local CopyOnChange DTO。S4 是产品 / 协议边界步骤，不写 C++。

## DTO 准入规则

可接受 DTO 只能包含：

- request DocumentObject graph 中已有或可由前端持久化的字段。
- request-local `documentObjectUpdates` 或 diagnostics。
- source object id/name、support subname、mutated property delta、copy intent 等结构化标量 / JSON。
- 明确的 deletion / update / reselect 建议。

禁止 DTO 包含：

- FreeCAD hidden temporary document。
- Raw `TopoDS_Shape`、BREP、full shape cache。
- request 结束后继续有效的 `NamedShape`、`ElementMap` 或 copied-object cache。
- `_CopiedObjs` private vector 或 native object pointer identity。

## 产品裁决

| 裁决 | 条件 | S5 路由 |
| --- | --- | --- |
| `dto_approved_candidate` | S3 证明字段稳定、可序列化、无持久 geometry cache | S5 可升级 implementation gate。 |
| `dto_rejected_known_gap_retained` | S3 仍依赖 hidden cache 或不可序列化 native state | S5 no-code release gate。 |
| `needs_more_native_evidence` | probe 被环境阻断或字段不完整 | S5 保持 known gap，记录重开条件。 |

## 必须回写的矩阵行

- `C9M5-SCOPE-103`
- `C9M5-SCOPE-201`
- `C9M5-BLOCKER-401`
- `C9M5-CAT-102`
- `C9M5-CAT-103`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'dto_approved_candidate|dto_rejected_known_gap_retained|needs_more_native_evidence|documentObjectUpdates|copy_on_change_full_temporary_document_cache|C9M5-SCOPE-103|C9M5-SCOPE-201' docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包/矩阵/*.tsv
git diff --check
```

验收标准：

- S4 明确给出三选一裁决。
- 若裁决为 `dto_approved_candidate`，必须列出 DTO 字段、来源、cad-core landing、focused tests 和禁止字段。
- 若裁决不是批准，S5/S6 不得落 C++ support。

## 非目标

- 不新增前端 / Rust 协议。
- 不修改 capability。
