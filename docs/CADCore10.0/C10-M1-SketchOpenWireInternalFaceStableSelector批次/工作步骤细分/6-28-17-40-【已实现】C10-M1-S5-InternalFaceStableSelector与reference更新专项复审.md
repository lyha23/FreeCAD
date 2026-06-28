# 【已实现】C10-M1-S5 InternalFace StableSelector 与 reference 更新专项复审

## 目标

根据 S3/S4 evidence 裁决 `Profile.StableSubList=InternalFaceN` without `ReferenceShadow` 是否能进入 request-local implementation gate，并定义 elementReferenceUpdates / ShadowSub / diagnostic 边界。S5 是产品 / 协议边界步骤，不写主要 C++。

## live 基线

| 命令 | S5 记录 |
| --- | --- |
| `pwd` | `/home/user/Chili3DProject/FreeCAD` |
| `git rev-parse --short HEAD` | `eedae1200c` |
| `git log -1 --oneline` | `eedae1200c docs: 完成 C10-M1 S4 WireJoiner 账本复审` |
| `git -c core.quotepath=false status --short -uall` | 无输出，工作区干净。 |
| C10-M1 queue | 下一项为 S5。 |

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

## S5 裁决

**裁决：`stable_selector_approved_candidate`。**

批准的是窄的 request-local selector candidate：`Profile.StableSubList=InternalFaceN` 可以在没有 `ReferenceShadow` 的情况下进入 S6 gate，但前提只能是当前 recompute 已经发布 `Sketch.InternalShape` 的 `NamedShape` / `ElementMap`，并且 `resolveElementReference()` 唯一解析到当前仍存在的 `InternalFaceN` subshape。该裁决不批准 raw `FaceN`、几何猜测、跨 request cache 或多解任选。

S3/S4 证据链如下：

- S3 已证明 near-tangent、coincident、touching open cutter、near-overlap 的 public `InternalFace` / `InternalEdge` / `InternalVertex` counts 与 FreeCAD expected 匹配；没有重新打开 FaceMaker C++ gate。
- S4 已证明 complex open-wire 只有唯一 child-wire / mapper-history evidence 时才能写 ElementMap alias；one-to-many、missing child-wire、summary_only、noOriginal purge 仍是 `diagnostic_retained`。
- current `profile_resolver.cpp` 已把 `StableSubList=InternalFaceN` gate 限定到 `Sketch.InternalShape` request-local `NamedShape` / `ElementMap`；缺 evidence、split、deleted、non-face 或 subshape 缺失都会发 diagnostic。

## 批准字段与边界

| 项 | S5 边界 |
| --- | --- |
| request 字段 | `Profile.StableSubList=["InternalFaceN"]`；`Profile.SubList` 可为空或为当前 `InternalFaceN`，但两者都必须解析到同一个 request-local internal face。 |
| evidence 来源 | `context.namedShapes["Sketch.InternalShape"]` 或 `shapeValue.internalNamedShape`；必须有 `elements`、`elementMap`、非空 `internalShape`，并能定位当前 subshape。 |
| cad-core landing | `cad-core/src/part_design/profile_resolver.cpp::resolveInternalFaceStableSubname()` / `resolveSketchInternalFaceProfile()`；S6 只需要按这个边界审计或补 focused evidence，不重开 FaceMaker / WireJoiner no-gap 行。 |
| focused tests | `cad-core/tests/test_p5_sketch.py` 的 accepted / missing-evidence / ReferenceShadow recovery tests，`cad-core/tests/test_p6_topology.py` 的 stable element-map update tests，`cad-core/tests/test_adapters.py` 的 C API update fields / capability schema tests。 |
| elementReferenceUpdates | without `ReferenceShadow` 的纯 request-local selector 不要求发布 `elementReferenceUpdates`；只有存在 `ReferenceShadow` lifecycle 或恢复证据时，才可发布 `SubList`、`StableSubList`、`ShadowSub`、`ReferenceShadow`。 |
| ShadowSub / ReferenceShadow | `ShadowSub` 只能作为 paired stable/current name 写回，并且 `ReferenceShadow` 必须验证当前 subshape；没有 `ReferenceShadow` 时不得用 `ShadowSub` 替代 ElementMap evidence。 |
| diagnostics | 缺 NamedShape/ElementMap、unresolved、split、deleted、non-face、subshape missing、empty InternalShape 分别保持 `unsupported_stable_subname`、`split_stable_subname`、`deleted_stable_subname`、`unsupported_subshape_kind`、`invalid_subshape` 或 `open_profile`。 |
| 禁止字段 / 路径 | raw `FaceN`、bbox、面积、outer-wire edge count、source index、split order、输出顺序、fixture 名、跨 request shape / NamedShape / ElementMap cache。 |

## 必须回写的矩阵行

- `C10M1-SCOPE-105`
- `C10M1-BLOCKER-501`
- `C10M1-CAT-103`
- `C10M1-CAT-104`

## 回写结果

- `C10M1-SCOPE-105`：由 `backend_gap_candidate` 改为 `stable_selector_approved_candidate`，但 close condition 限定为 request-local `Sketch.InternalShape` `NamedShape` / `ElementMap` 唯一证据。
- `C10M1-BLOCKER-501`：关闭为 `closed_s5`。
- `C10M1-CAT-103`：改为 `stable_selector_approved_candidate`，产品裁决已完成。
- `C10M1-CAT-104`：保留 `release_gate`，S6 负责把 S3/S4 no-code gate 与 S5 approved candidate 的 tests / capability / docs 口径对齐。

## S6 gate

S6 可以消费 `C10M1-SCOPE-105`，但只允许在上述字段内审计或补齐 existing selector publication evidence：`profile_resolver.cpp`、`element_reference_update.cpp`、`reference_resolution.cpp`、`test_p5_sketch.py`、`test_p6_topology.py`、`test_adapters.py`。S6 不得把 without-ReferenceShadow 扩展成 raw `FaceN` support，不得把 S4 的一对多 WireJoiner diagnostic 改成 supported alias，也不得引入跨 request cache。

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

## 验收记录

- `rg -n 'StableSubList|InternalFace|elementReferenceUpdates|ShadowSub|ReferenceShadow|stable_selector_approved_candidate|stable_selector_rejected_diagnostic_retained|needs_more_native_evidence' cad-core/src/part_design/profile_resolver.cpp cad-core/src/runtime cad-core/tests docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次`：通过，selector 裁决、accepted/diagnostic tests、reference update fields 和 S5/S6 gate 均可定位。
- `awk -F '\t' ... docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次/矩阵/*.tsv`：通过，TSV 字段数一致。
- `git diff --check`：通过。
- `step_goal_queue.py docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次/工作步骤细分 --format markdown`：下一项为 S6。

## 非目标

- 不新增前端 / Rust 协议。
- 不修改 capability，除非 S6 发布口径需要。
