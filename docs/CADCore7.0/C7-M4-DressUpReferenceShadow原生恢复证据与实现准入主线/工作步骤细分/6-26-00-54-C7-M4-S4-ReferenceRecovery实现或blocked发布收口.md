# C7-M4 S4 ReferenceRecovery 实现或 blocked 发布收口

## 目标

按 S3 route 执行：如果 S3 打开 `backend_gap_requires_implementation`，实现正式 ReferenceShadow recovery；否则做 no-code blocked / diagnostic 发布收口。S4 不允许扩大到 full MapperHistory、full DressUp universe 或 output-side guessing。

## 必读文件

- S3 完成后的本包 README、方案和矩阵。
- S2/S3 fixture / expected / blocker JSON。
- `src/App/PropertyLinks.cpp`
- `src/Mod/PartDesign/App/FeatureDressUp.cpp`
- `cad-core/src/app`
- `cad-core/src/part`
- `cad-core/src/part_design/feature_dress_up.cpp`
- `cad-core/tests/test_p6_topology.py`
- `cad-core/tests/test_p7_features.py`

## 执行要点

1. 记录 live baseline 和 C7-M4 queue。
2. 若 S3 route 不是 `backend_gap_requires_implementation`，只更新 README、方案、矩阵和 P7 发布口径。
3. 若 S3 打开 code gate，先补 link/reference evidence validation，再接入 DressUp Base 消费点。
4. 写 focused tests，至少约束 diagnostics、shape summary、`documentObjectUpdates` / `elementReferenceUpdates`。
5. 不允许在 adapter、JSON 输出、fixture 名称、EdgeN 排序或 source shape 猜测上修正结果。
6. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S5。

## 实现落点

- `cad-core/src/app`：PropertyLinkSub / SubList / StableSubList / ShadowSub / ReferenceShadow 输入与更新建议。
- `cad-core/src/part`：`ReferenceShadow.brep` snapshot 校验和旧 subshape 证据读取。
- `cad-core/src/part_design/feature_dress_up.cpp`：只消费正式恢复后的 Base target / subnames。
- `cad-core/tests/test_p7_features.py`、`cad-core/tests/test_p6_topology.py`：focused regression。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest
python3 -m unittest tests.test_p6_topology.CadCoreP6TopologyTest
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线 docs/CADCore7.0/README.md docs/CADCore方案/细化方案/10-P7-PartDesign常用生态.md
git diff --check
```

如果 S4 只做 no-code publication closure，不运行 `cmake --build build`，只运行文档短跑和 relevant focused unittest。

## 完成标准

- S4 route 已落实为实现收口或 no-code blocked 发布。
- docs / tests / expected / capability 口径一致。
- 队列推进到 S5。
