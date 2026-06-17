# P6 MakerHistory S4 DressUp / Refine 传播专项复审

## 目标

复核 DressUp AddSubShape、RefineModel、transformed consumer 和后续 Body boolean / Link retag 是否共同保留 MakerHistory。S4 只裁决传播边界，不扩大到所有 Fillet / Chamfer / Draft / Thickness 参数全集。

## 执行结论

2026-06-17 已完成 S4 复审：`P6MH-SCOPE-003` 裁决为 `supported`。当前支持范围是 DressUp AddSubShape slot、Fillet / Chamfer refine 后处理、RefineModel modified / deleted / generated 传播、transformed copy、SupportTransform 连续 DressUp 以及 transformed / pattern 链路中的 history 传播。

证据链：FreeCAD `DressUp::getAddSubShape()` 的 SupportTransform skip 语义、`makeElementRefine()` 的 `GenericShapeMapper`、`FeatureTransformed.cpp::Transformed::execute()` 的 Features / WholeShape 消费路径，分别落到 cad-core `cacheDressUpAddSubShape()`、`applyDressUpRefine()`、`namedShapeForRefineHistory()`、`namedShapeForTransformedCopy()` 和 transformed executor。`test_p7_features.py` 覆盖 DressUp refine、AddSubShape slot、chained DressUp pattern history、transformed terminal split/deleted；`test_adapters.py` 约束 `producer_matrix.dressup.remaining == []` 且 `producer_matrix.transformed.remaining == []`。

本结论不扩大为完整 GUI 参数全集或 standalone diagnostic-only oracle；复杂 split / deleted 引用恢复仍保留在 `P6MH-SCOPE-005 = notCollected`，只有后续 oracle 证明 mismatch 时才进入 S6 C++。

## FreeCAD 依据

| 入口 | 关键点 |
| --- | --- |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getAddSubShape()` | SupportTransform 跳过连续 DressUp，生成 additive / subtractive compound cache |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureFillet.cpp` | `makeElementFillet()` 后 ShapeFix tolerance 与 refine |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureChamfer.cpp` | `makeElementChamfer()` 后 ShapeFix tolerance 与 refine |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRefine()` | `GenericShapeMapper` 消费 Refine modified / deleted / generated |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp` | transformed family 消费 DressUp `getAddSubShape()` |

## cad-core 复核点

| 文件 | 检查项 |
| --- | --- |
| `cad-core/src/part_design/feature_dress_up.cpp` | Fillet / Chamfer / Draft / Thickness history metadata 和 AddSubShape slot |
| `cad-core/src/part_design/feature_dress_up_support.h` | SupportTransform cache / chain route |
| `cad-core/src/part/topo_shape.cpp` | RefineModel / boolean / transformed copy / merge history 传播 |
| `cad-core/tests/test_p7_features.py` | DressUp、chain DressUp、SupportTransform、transformed history focused tests |
| `cad-core/tests/test_adapters.py` | `producer_matrix.dressup` 和 `transformed` remaining gap |

## 范围裁决

| scope | S4 需要裁决 |
| --- | --- |
| `P6MH-SCOPE-003` | DressUp history 是 supported、releaseGate、notCollected 还是 backendGap |
| `P6MH-SCOPE-005` | DressUp 后 split / deleted / merge 是否还能稳定诊断或恢复 |
| `P6MH-SCOPE-006` | 文档 / capability 发布是否要回写 |

## 必须回写的矩阵行

- `P6MH-SCOPE-003`
- `P6MH-BLOCK-003`
- `P6MH-BG-003`

## 验收标准

- 必须列出当前 covered 的 DressUp 子项和仍不纳入的参数全集。
- 如果 `producer_matrix.dressup.remaining=[]` 被接受，必须把 S4 结论路由到 releaseGate / supported，而不是继续写 backendGap。
- 如果发现 mismatch，必须给出目标 fixture、expected 差异、C++ landing 和禁止 shortcut。
- 执行：

```bash
rg -n "DressUp|SupportTransform|AddSubShape|makeElementFillet|makeElementChamfer|refine|transformed" cad-core/src/part_design cad-core/src/part cad-core/tests/test_p7_features.py cad-core/tests/test_adapters.py
rg -n "DressUp::getAddSubShape|SupportTransform|makeElementFillet|makeElementChamfer|refineShapeIfActive" src/Mod/PartDesign/App src/Mod/Part/App
git diff --check
```

## 非目标

- 不补 Fillet / Chamfer / Draft / Thickness 所有 GUI 参数组合。
- 不靠 transformed executor 猜 DressUp ownership。
- 不把 standalone diagnostic-only DressUp fixture 伪装成 native oracle。
