# C10-M1-S5 InternalFace StableSelector 与 reference 更新专项复审

## 目标

根据 S3/S4 evidence 裁决 `Profile.StableSubList=InternalFaceN` without `ReferenceShadow` 是否能进入 request-local implementation gate，并定义 elementReferenceUpdates / ShadowSub / diagnostic 边界。S5 是产品 / 协议边界步骤，不写主要 C++。

## 准入规则

可接受 stable selector 只能基于：

- 当前 recompute 的 `Sketch.InternalShape` `NamedShape` evidence。
- `ElementMap` 中唯一的 `InternalFaceN` alias。
- FaceMaker / WireJoiner producer history 唯一映射。
- request-local `elementReferenceUpdates`、`StableSubList`、`ShadowSub` 或 diagnostic。

禁止 stable selector 基于：

- raw `FaceN` alias。
- bbox、面积、outer-wire edge count、source index、split order 或输出排序。
- request 结束后继续有效的 NamedShape / ElementMap cache。
- 缺证据或多解时的任意 target。

## 裁决选项

| 裁决 | 条件 | S6 路由 |
| --- | --- | --- |
| `stable_selector_approved_candidate` | S3/S4 证明唯一 request-local evidence | S6 可实现。 |
| `stable_selector_rejected_diagnostic_retained` | 多解、缺 evidence 或依赖 raw FaceN | S6 no-code 或 diagnostic gate。 |
| `needs_more_native_evidence` | oracle / current comparison 不完整 | S6 保留 notCollected / blocker。 |

## 必须回写的矩阵行

- `C10M1-SCOPE-105`
- `C10M1-BLOCKER-501`
- `C10M1-CAT-103`
- `C10M1-CAT-104`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'StableSubList|InternalFace|elementReferenceUpdates|ShadowSub|ReferenceShadow|stable_selector_approved_candidate|stable_selector_rejected_diagnostic_retained|needs_more_native_evidence' cad-core/src/part_design/profile_resolver.cpp cad-core/src/runtime cad-core/tests docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次/矩阵/*.tsv
git diff --check
```

## 验收标准

- S5 明确给出三选一裁决。
- 若裁决为 `stable_selector_approved_candidate`，必须列出字段、来源、cad-core landing、focused tests 和禁止字段。
- 若裁决不是批准，S6 不得落 stable selector support。

## 非目标

- 不新增前端 / Rust 协议。
- 不修改 capability，除非 S6 发布口径需要。
