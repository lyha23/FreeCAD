# 【已实现】P7 Transformed S1 FreeCAD 源码候选矩阵

## 目标

从 FreeCAD transformed family 源码建立候选矩阵，只记录 source authority、语义轴和 cad-core 落点，不在 S1 判定 supported / backendGap。

## FreeCAD 依据

| 语义轴 | FreeCAD 源码入口 | 需要读取的关键短句 / 字段 |
| --- | --- | --- |
| common execute | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute()` | `getTransformations(originals)`、`getAddSubShape(fuseShape, cutShape)`、`makeElementTransform`、`makeElementFuse`、`makeElementCut` |
| mirrored transform | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureMirrored.cpp::Mirrored::getTransformations()` | DatumPlane / planar face mirror transform |
| linear pattern transforms | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureLinearPattern.cpp` | `Occurrences`、`Occurrences2`、`SpacingPattern`、`Spacings`、two directions |
| polar pattern transforms | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePolarPattern.cpp` | axis、angle、spacing pattern |
| scaled transforms | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureScaled.cpp::Scaled::getTransformations()` | first original centre of mass、factor、occurrences |
| multi transform composition | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureMultiTransform.cpp::MultiTransform::getTransformations()` | multiplication method、Scaled diagonal method、divisor error |
| topo transform history | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementTransform()` | transform 后复制 ElementMap |

## 扫描轴

1. Transform list 是否包含 identity，以及 cad-core 是否只复制非 identity transform。
2. Features 模式是否逐 original replay AddSubShape add / sub slot。
3. Whole shape 模式是否使用 BaseFeature / Body prefix support，而不是隐藏 `Originals`。
4. transformed copy 是否保留 source element map、nested history、terminal split / deleted 和 merge history。
5. FreeCAD native expected collector 是否能采 topology_counts；不能采时必须保留 notCollected。

## candidate TSV 字段

```text
candidate_id	source_file	freecad_symbol	semantic_axis	source_evidence	cad_core_landing	scope_hint	next_step
```

## seed candidate routes

- `P7T-CAND-001` 到 `P7T-CAND-006` 对应 FreeCAD transformed family 类。
- `P7T-CAND-007` 对应 DressUp / AddSubShape slot ownership。
- `P7T-CAND-008` 对应 `TopoShape::makeElementTransform()` 和 ElementMap copy。

## 必须回写的矩阵行

- `p7_transformed_source_candidates.tsv`：每个 candidate 必须有 FreeCAD 源文件、cad-core 落点和 next_step。
- `p7_transformed_scope_review_matrix.tsv`：S1 只建议 scope_hint，不直接改 current_status。

## 完成结果

- `P7T-CAND-001` 到 `P7T-CAND-008` 已补齐本地 FreeCAD 源文件、symbol、语义轴、源码短句 evidence、cad-core landing、scope_hint 和 next_step。
- `p7_transformed_scope_review_matrix.tsv` 只补充了 `next_step` 的来源路线说明，保留 S0 的 `current_status` 不变；S1 不判定 supported / backendGap。
- 已复核 cad-core 当前真实落点为 `cad-core/src/part_design/*`、`cad-core/src/part/topo_shape.cpp` 和对应 public headers；未回写旧 `features/` 路径。

## 验收

```bash
awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' \
  docs/FreeCAD几何生态迁移工程-细分/P7-Transformed-Topology-MakerHistory收敛主线/矩阵/p7_transformed_source_candidates.tsv
```

## 非目标

- 不在 S1 修改 C++。
- 不在 S1 把候选直接判定为 backendGap。
- 不扫描 unrelated PartDesign Hole / Draft / Thickness 主线。
