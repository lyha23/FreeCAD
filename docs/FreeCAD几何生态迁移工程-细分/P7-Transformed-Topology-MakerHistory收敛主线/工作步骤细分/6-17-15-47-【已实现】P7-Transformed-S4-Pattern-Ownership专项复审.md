# 【已实现】P7 Transformed S4 Pattern Ownership 专项复审

## 目标

复审 transformed copy、AddSubShape slot ownership、source alias retag 和 terminal history 是否已有足够发布证据。S4 不补 oracle、不改 expected、不写 C++；S3 已把 topology mismatch 打入 S6，本轮只裁决是否还有独立 ownership backendGap。

## live 基线

- `pwd`：`/home/user/Chili3DProject/FreeCAD`
- `HEAD`：`5ebb3e2081`
- `git log -1 --oneline`：`5ebb3e2081 test: 补齐 ExternalGeometry 状态机原生 oracle`
- `git status --short -uall`：进入 S4 前已有 AGENTS / DESIGN / cad-core / P5P6 / expected 等非本轮改动，P7 主线和矩阵为未跟踪文件；本轮只改 P7 S4 文档、P7 两个入口和 S4 相关 TSV，不 reset、不 revert、不提交。

## FreeCAD 调用链

| 语义 | FreeCAD 依据 | S4 结论 |
| --- | --- | --- |
| Features mode ownership | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute()` | `Features` 模式逐个 original 调 `feature->getAddSubShape(fuseShape, cutShape)`，再对 add slot fuse、sub slot cut；ownership 应来自 AddSubShape slot，不来自几何猜测 |
| DressUp SupportTransform | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getAddSubShape()` | `SupportTransform` 跳过连续 DressUp，回到前一个 `FeatureAddSub` support，并把 add / sub slot 写入 compound |
| Fillet / Chamfer maker | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureFillet.cpp`; `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureChamfer.cpp` | DressUp result 仍走 `makeElementFillet` / `makeElementChamfer` maker history，slot cache 是 transformed 消费路径 |
| transform ElementMap | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementTransform()` | transform 后 `copyElementMap(tmp, op)`；source alias retag 不应按 bbox / area / output order 推断 |
| boolean terminal history | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::makeElementFuse()` / `makeElementCut()`; `MapperMaker`; `MapperHistory` | boolean 后通过 maker generated / modified 和 terminal deleted / split 传播历史 |

## cad-core 审计结论

| 文件 | 证据 | S4 判断 |
| --- | --- | --- |
| `cad-core/src/part_design/feature_transformed.cpp` | `applyFeatureTransforms()` 按 original 逐 slot replay：add slot 先 `transformedCopy()` 再 Fuse，sub slot 先 `transformedCopy()` 再 Cut；`transformedCopy()` 调 `part::namedShapeForTransformedCopy()` | ownership 来自 AddSubShape slot 和 topo helper，未发现 geometry-match / fixture-name 分支 |
| `cad-core/src/part_design/feature_dress_up.cpp` | `resolveSupportTransformFeature()` 按 FreeCAD 跳过链式 DressUp；`cacheDressUpAddSubShape()` 对 additive / subtractive support 分别写 add / sub NamedShape slot | DressUp SupportTransform slot ownership 已有实现证据 |
| `cad-core/src/part/topo_shape.cpp` / `include/cad_core/part/topo_shape.h` | `namedShapeForTransformedCopy()` 复制 source ElementMap、retag `source.owner.ElementN`，并 `propagateNestedSourceHistory()` + `addMergeHistory()`；boolean 走 `namedShapeForMakerHistory()` | transformed copy、source alias retag、terminal split/deleted、merge history 已有 topo 层主路径 |

## 测试覆盖

| 必查项 | 已有约束 |
| --- | --- |
| `TransformN.` aliases | `test_p7_mirrored_features_mode_fuses_transformed_additive_original`、`assert_transformed_pattern_ownership()` |
| original feature stable aliases | `Pad.*` / Sketch source aliases 在 P7 transformed ownership tests 中检查 |
| DressUp `SupportTransform` slot | `test_p7_mirrored_features_mode_consumes_dressup_support_transform_cache` |
| chained DressUp | `test_p7_mirrored_features_mode_consumes_chained_dressup_support_transform_cache`、`test_c3m5_chained_dressup_pattern_history_keeps_support_transform_slot` |
| multi-original add / sub | `test_p7_linear_pattern_replays_multi_original_add_and_sub_slots` |
| terminal split / deleted | `test_p7_transformed_copy_preserves_terminal_stable_history` |
| merge history | `assert_transformed_pattern_ownership()` 与 terminal stable history test 均检查 `history_consumed:merge` |
| capabilities | `test_c_api_capabilities_exposes_web_contract_facts` 覆盖 `transformed_pattern_addsub_ownership`、`transformed_pattern_full_history`、producer matrix `transformed.status=covered` |

精确测试名 `test_capabilities_include_p7_partdesign_transformed_history` 不存在；已用 `rg -n "test_capabilities_include_p7_partdesign_transformed_history" cad-core/tests/test_adapters.py cad-core/tests/test_p7_features.py` 复核无输出。

## 禁止模式 rg 结论

```bash
rg -n "mirrored-pad|mirrored-fillet|mirrored-dressup|linear-pattern|polar-pattern|scaled-whole|multi-transform|fixture|fixtures/p7" \
  cad-core/src/part_design/feature_transformed.cpp cad-core/src/part_design/feature_dress_up.cpp \
  cad-core/src/part/topo_shape.cpp cad-core/include/cad_core/part/topo_shape.h
```

结论：无输出；S4 审计文件中未发现按 fixture 名称分支。

```bash
rg -n "bboxForShape|volumeForShape|bbox|volume|area|length|LinearProperties|SurfaceProperties|VolumeProperties|GProp|Mass\\(|SquareDistance|Distance\\(" \
  cad-core/src/part_design/feature_transformed.cpp cad-core/src/part_design/feature_dress_up.cpp \
  cad-core/src/part/topo_shape.cpp cad-core/include/cad_core/part/topo_shape.h
```

结论：命中仅为能力输出的 `bbox` / `volume` metadata、Sketch axis 长度校验、wire helper 的端点距离选择，以及 topo header 中“不要用 bbox / area / output order”的注释；未发现用面积、长度、bbox、volume 做 transformed ownership。

```bash
rg -n "source[_A-Za-z]*index|transform[_A-Za-z]*index|transformedIndex|std::sort|sort\\(|stable.*order|output.*order|subshape.*order|element.*order|geometry[-_ ]?match|match.*geometry|guess|heuristic|fallback|prun" \
  cad-core/src/part_design/feature_transformed.cpp cad-core/src/part_design/feature_dress_up.cpp \
  cad-core/src/part/topo_shape.cpp cad-core/include/cad_core/part/topo_shape.h
```

结论：`transformedIndex` 只用于生成 `TransformN` owner 名；`source_edge_index` 命中属于 Sketch InternalShape / WireJoiner history evidence；`fallback` 命中是 enum / property 默认值和已标注的非 transformed ownership 边界。未发现用 source index、transform index、输出排序或 stable subname 排序修正 ownership。

## 矩阵裁决

- `P7T-SCOPE-006`：S4 收敛为 `supported`。ownership release evidence 已由源码和 focused tests 支撑；这不代表 P7 transformed topology 全完成，S3 打开的 topology backendGap 仍由 S6 修复。
- `P7T-BLOCK-005`：ownership 部分关闭，不新增 C++ landing；S5 已确认 MultiTransform fallback / composition 无新增 blocker，S6 仍消费 S3 topology backendGap。
- 未发现 concrete S4 backendGap；不修改 C++，不改 expected。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_mirrored_features_mode_fuses_transformed_additive_original \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_mirrored_features_mode_consumes_dressup_support_transform_cache \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_mirrored_features_mode_consumes_chained_dressup_support_transform_cache \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_linear_pattern_replays_multi_original_add_and_sub_slots \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_transformed_copy_preserves_terminal_stable_history
```

结果：`Ran 5 tests`，`OK`。

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

结果：`Ran 1 test`，`OK`。

P7 全部 TSV 字段数检查无输出；`git diff --check` 无输出。

## 非目标

- 不重复采 S3 topology oracle。
- 不把 topology mismatch 写成 S4 ownership backendGap。
- 不标 S5、S6 或整条 P7 主线已实现。
