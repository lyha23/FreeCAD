# 【已实现】C10-M2-S3 DressUp 生产者 History 专项复审

## 目标

复核 DressUp 生产者 history：Fillet / Chamfer 的 AddSubShape slot、multi-selection / face-selection history，Draft / Thickness 的 face ownership、neutral plane、multi-solid / fuse history，以及这些结果进入 Body、transformed family 和 focused tests 的传播。S3 只在 expected-backed mismatch 出现时打开 `backend_gap_candidate`。

## FreeCAD 依据

- `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getAddSubShape()`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getContinuousEdges()`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureFillet.cpp::Fillet::execute()`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureChamfer.cpp::Chamfer::execute()`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureDraft.cpp::Draft::execute()`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureThickness.cpp::Thickness::execute()`

## 范围

| scope | 内容 | 默认路由 |
| --- | --- | --- |
| `C10M2-SCOPE-101` | Fillet / Chamfer AddSubShape slot、multi-edge / face selection、chamfer parameter variants | `oracle_candidate` |
| `C10M2-SCOPE-102` | Draft / Thickness selected faces、neutral plane、multi-solid / fuse history | `oracle_candidate` |
| `C10M2-SCOPE-301` | DressUp 后续 transformed / Body / Link retag 的 terminal split / deleted 边界 | 只记录给 S5，不在 S3 直接实现 |

## 必须检查的 current 证据

- `cad-core/tests/test_p7_features.py` 中 Fillet / Chamfer / Draft / Thickness focused tests 是否覆盖 S3 scope。
- `cad-core/src/runtime/capability_contract.cpp` 中 `dressup_*` producer matrix 是否与 tests / docs 一致。
- `cad-core/src/part_design/feature_dress_up_support.h` 与 `cad-core/src/part_design/feature_dress_up.cpp` 是否仍是 AddSubShape slot / selection history 的正式落点。
- 若需要新增 oracle，优先新增 source-backed P7 / C10M2 representative fixture，不允许用 diagnostic-only standalone DressUp 输出做 native golden。

## 必须回写的矩阵行

- `C10M2-SCOPE-101`
- `C10M2-SCOPE-102`
- `C10M2-BLOCKER-301`
- `C10M2-CAT-101`
- 如发现 no-gap：写为 `no_gap` 或 `expected_backed_no_gap`，并说明 checked-in expected / focused tests。
- 如发现 current mismatch：写为 `backend_gap_candidate`，并把 S6 code landing 限定到对应 C++ / tests。
- 如只有不可观察 old reference recovery：写为 `notCollected` 或 `diagnostic_retained`，不写 implementation row。

## S3 复核结果

- live baseline：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=d5adcb5666`（`docs: 完成 C10-M2 S2 范围准入矩阵`），起点工作区干净。
- `C10M2-SCOPE-101=expected_backed_no_gap`：FreeCAD `DressUp::getAddSubShape()` 的 AddSubShape cache / SupportTransform slot、`getContinuousEdges()` 的 Edge/Face/Wire 展开，以及 `Fillet::execute()` / `Chamfer::execute()` 的 maker + rawShape-before-refine 路径，与 current `cad-core/src/part_design/feature_dress_up.cpp`、`feature_fillet.cpp`、`feature_chamfer.cpp` 和 P7/C3M5 checked-in expected 对齐。覆盖证据包括 `fillet-pad-edge`、`chamfer-pad-edge`、`fillet-pad-multi-edge`、`fillet-pad-use-all-edges`、`chamfer-*` 参数变体、`fillet-face-selection-history`、`mirrored-fillet-support-transform`、`mirrored-dressup-chain-support-transform`、`chained-dressup-pattern-history`，未发现 current mismatch。
- `C10M2-SCOPE-102=no_gap`：FreeCAD `Draft::execute()` 的 selected FaceN、neutral plane / pull direction 与 copy-no-face path，以及 `Thickness::execute()` 的 selected close faces、Mode / Join / Reversed / Intersection、per-solid thick-solid 和 multi-solid fuse path，与 current `feature_draft.cpp`、`feature_thickness.cpp`、focused tests 和 capability producer matrix 一致。Draft / Thickness 行当前没有 checked-in `.freecad.json` expected，因此不升级为 `expected_backed_no_gap`，也没有证据进入 `backend_gap_candidate`。
- `C10M2-BLOCKER-301=closed_s3`：S3 没有打开 DressUp C++ / tests / fixtures / expected / capability 改动；S6 只需发布 no-code no-gap release gate。
- `C10M2-CAT-101=no_gap`：DressUp producer-history 分类已从 `oracle_candidate` 收口为 no-code no-gap。stale `ReferenceShadow` / Base recovery 仍由 `C10M2-NG-006` 和 S5 保持 `diagnostic_retained`，不能声明 supported。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n "Fillet|Chamfer|Draft|Thickness|dressup|AddSubShape|SupportTransform|element_history_status" cad-core/tests/test_p7_features.py cad-core/src/runtime/capability_contract.cpp cad-core/src/part_design
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M2-PartDesignDressUpHoleTopoHistory批次/矩阵/*.tsv
git diff --check
```

S3 验收必须给出 `C10M2-SCOPE-101` / `C10M2-SCOPE-102` 的明确状态：`no_gap`、`current_mismatch_candidate`、`backend_gap_candidate`、`diagnostic_retained` 或 `notCollected`。不能只写“待定”。

验收通过后，S3 文件可重命名为 `6-28-22-58-【已实现】C10-M2-S3-DressUp生产者History专项复审.md`。

## 非目标

- 不扩大 full DressUp universe。
- 不靠几何相似度、bbox、面积或输出排序推断 source ownership。
- 不删除 C7-M4 stale `ReferenceShadow` oracle-blocked blocker，除非有新的 native observable evidence。
