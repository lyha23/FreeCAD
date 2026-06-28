# 【已实现】C10-M2-S5 跨特征旧引用恢复与 diagnostic 边界专项复审

## 目标

复核 DressUp / Hole 经 Body、transformed family、Link retag 或后续 maker 后的旧引用恢复与 diagnostic 边界。S5 的重点不是扩大实现，而是防止把不可观察 old-reference recovery、split 一对多或 deleted 旧引用误写成 stable selector。

## FreeCAD 依据

- `/home/user/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp`
- `/home/user/Chili3DProject/FreeCAD/src/App/GeoFeature.cpp`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/PropertyTopoShape.cpp`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShape.cpp`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp`

## 范围

| scope | 内容 | 默认路由 |
| --- | --- | --- |
| `C10M2-SCOPE-301` | DressUp / Hole 后续 Body boolean、transformed copy、Link retag 后的 split / deleted / merge history | `notCollected` 或 `diagnostic_retained`，除非已有 expected-backed mismatch |
| `C10M2-SCOPE-401` | capability / diagnostics / publication wording | S6 release gate |

## 判断规则

- 同类唯一 target 且有 `ElementMap` / MapperHistory 证据：可以进入 reference update candidate。
- split 一对多、deleted、source evidence 不足：保留 structured diagnostic，不写可解析 stable subname。
- stale `ReferenceShadow` / Base recovery 若 FreeCAD Python API 仍不可观察 `ShadowSub` / `ReferenceShadow` 原生恢复：保持 oracle-blocked / notCollected，不实现。
- 只要 proposed selector 依赖 raw `FaceN`、bbox、面积、顺序、source index 或 fixture 名称，必须退回 non-goal。

## 必须回写的矩阵行

- `C10M2-SCOPE-301`
- `C10M2-BLOCKER-501`
- `C10M2-CAT-103`
- `C10M2-NG-004`
- `C10M2-NG-006`
- 如 S5 发现可实现行，必须写清 expected evidence、current mismatch、code landing 和禁止 shortcut。

## S5 复核结果

- live baseline：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=5a15c0fb9a`（`docs: 完成 C10-M2 S4 Hole History 复审`），起点工作区干净。
- `C10M2-SCOPE-301=diagnostic_retained`：FreeCAD `PropertyLinkBase::updateElementReferences()` / `_updateElementReference()` 通过 `ShadowSub` 和 `GeoFeature::resolveElement()` 更新旧引用，`PropertyPartShape::afterRestore()` 也只是触发 element map 版本重建；这些源码不支持用 raw `FaceN`、bbox、面积、顺序、source index 或 fixture 名称从最终几何倒推旧引用。current `cad-core` 的 `resolveElementReference()` 对一对多 split 返回 `Split`，对 deleted 返回 `Deleted`，`namedShapeForTransformedCopy()` 按 FreeCAD `copyElementMap(tmp, op)` 传播 source alias / merge / terminal history，Body boolean 和 Link retag 继续保留 `element_history_status` 与 mapper_history 诊断。
- DressUp / Hole 经 Body boolean、transformed copy、Link retag 后的 split / deleted / merge history：保持 retained diagnostic / no-code release gate。`test_p7_features.py` 已约束 transformed DressUp slot、chained DressUp pattern、Hole Body cut 和 transformed stable history diagnostics；`test_adapters.py` 已约束 capability 中 `terminal_history=["deleted","split","merge"]`、`element_history_status`、`link_retag_composition` 和 `terminal_split_deleted`。这些证据证明 diagnostic 边界存在，不证明不可观察 old-reference recovery supported。
- stale `ReferenceShadow` / Base recovery：保持 `notCollected` / `diagnostic_retained`，不进入 `backend_gap_candidate`。`dressup-reference-shadow-base-recovery.freecad.json` 和 native probe evidence 显示 FreeCADCmd FCStd/XML restore 成功，但 Python-visible Base property 仍无法观察 `Base.getShadowSubs()`、`getSubValues(false)` 或 `getSubValues(true)`；`StableSubList`-fed geometry output 只是 negative control，不能删除 blocker 或声明 supported。
- `C10M2-BLOCKER-501=closed_s5`：S5 没有发现 source-backed expected + current mismatch，因此不开 S6 C++ / tests / fixtures / expected / capability implementation row。
- `C10M2-CAT-103=diagnostic_retained`：S6 只发布 retained diagnostic / no-code release gate；未来只有 native observable `ShadowSub` / `ReferenceShadow` recovery evidence 加 current mismatch 才能重开。
- `C10M2-NG-004` 继续是永久 shortcut ban；`C10M2-NG-006` 继续是 stale `ReferenceShadow` / Base recovery supported 的 retained diagnostic / oracle-blocked non-goal。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n "StableSubList|ReferenceShadow|ShadowSub|split|deleted|merge|Link retag|elementReferenceUpdates|element_history_status" docs/CADCore方案/细化方案/09-P6-TopoNaming主路径.md docs/CADCore方案/细化方案/10-P7-PartDesign常用生态.md cad-core/tests/test_p7_features.py cad-core/tests/test_adapters.py cad-core/src/part_design cad-core/src/part cad-core/src/app
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore10.0/C10-M2-PartDesignDressUpHoleTopoHistory批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M2-PartDesignDressUpHoleTopoHistory批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0/C10-M2-PartDesignDressUpHoleTopoHistory批次 docs/CADCore10.0/README.md
git diff --check
```

S5 验收必须给出 `C10M2-SCOPE-301` 的明确状态。若保持 `notCollected` / `diagnostic_retained`，必须写明 reopen condition；若进入 `backend_gap_candidate`，必须有 source-backed expected 和 current mismatch。

验收通过后，S5 文件已重命名为 `6-28-23-00-【已实现】C10-M2-S5-跨特征旧引用恢复与diagnostic边界专项复审.md`。

## 验收记录

- `rg -n "StableSubList|ReferenceShadow|ShadowSub|split|deleted|merge|Link retag|elementReferenceUpdates|element_history_status" ...`：通过，确认 P6/P7 docs、current tests 和 cad-core source 中可定位 shadow evidence、terminal split/deleted/merge diagnostics、Link retag 和 `element_history_status`。
- `step_goal_queue.py ... --format markdown`：通过，S5 重命名后下一项为 S6。
- TSV 字段数检查：通过。
- trailing whitespace 检索：无匹配，退出码 1 为预期通过。
- `git diff --check`：通过。

## 非目标

- 不实现不可观察 stale ReferenceShadow / Base recovery。
- 不做 raw `FaceN` 或输出端排序 selector。
- 不新增跨请求 geometry cache。
