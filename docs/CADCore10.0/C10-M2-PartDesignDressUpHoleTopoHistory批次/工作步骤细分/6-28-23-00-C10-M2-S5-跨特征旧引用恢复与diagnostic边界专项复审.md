# C10-M2-S5 跨特征旧引用恢复与 diagnostic 边界专项复审

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
- 如 S5 发现可实现行，必须写清 expected evidence、current mismatch、code landing 和禁止 shortcut。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n "StableSubList|ReferenceShadow|ShadowSub|split|deleted|merge|Link retag|elementReferenceUpdates|element_history_status" docs/CADCore方案/细化方案/09-P6-TopoNaming主路径.md docs/CADCore方案/细化方案/10-P7-PartDesign常用生态.md cad-core/tests/test_p7_features.py cad-core/tests/test_adapters.py cad-core/src/part_design cad-core/src/part cad-core/src/app
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M2-PartDesignDressUpHoleTopoHistory批次/矩阵/*.tsv
git diff --check
```

S5 验收必须给出 `C10M2-SCOPE-301` 的明确状态。若保持 `notCollected` / `diagnostic_retained`，必须写明 reopen condition；若进入 `backend_gap_candidate`，必须有 source-backed expected 和 current mismatch。

验收通过后，S5 文件可重命名为 `6-28-23-00-【已实现】C10-M2-S5-跨特征旧引用恢复与diagnostic边界专项复审.md`。

## 非目标

- 不实现不可观察 stale ReferenceShadow / Base recovery。
- 不做 raw `FaceN` 或输出端排序 selector。
- 不新增跨请求 geometry cache。
