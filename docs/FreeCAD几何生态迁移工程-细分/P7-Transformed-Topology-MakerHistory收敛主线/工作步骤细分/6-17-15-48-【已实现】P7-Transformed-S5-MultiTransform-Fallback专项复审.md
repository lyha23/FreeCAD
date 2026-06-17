# 【已实现】P7 Transformed S5 MultiTransform / Fallback 专项复审

## 目标

专项复审 MultiTransform 的 transform 组合语义、Scaled diagonal 语义、Whole shape support 路径和 fallback 删除边界。S5 只裁决 S6 前是否还有独立 composition / fallback blocker；不写 C++，不改 expected。

## live 基线

- `pwd`：`/home/user/Chili3DProject/FreeCAD`
- `HEAD`：`5ebb3e2081`
- `git log -1 --oneline`：`5ebb3e2081 test: 补齐 ExternalGeometry 状态机原生 oracle`
- `git status --short -uall`：进入 S5 前已有 AGENTS / DESIGN / cad-core / P5P6 / expected 等非本轮改动，P7 主线和矩阵为未跟踪文件；本轮只修改 P7 S5 文档、P7 两个入口和 S5 相关 TSV，不 reset、不 revert、不提交。

## FreeCAD 依据

| FreeCAD 源码 | 关键语义 | S5 结论 |
| --- | --- | --- |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureMultiTransform.cpp::MultiTransform::getTransformations()` | 第一组 transformation 作为 base；后续非 Scaled 子特征走 `nt * ot` multiplication method | cad-core 已按同序组合，不新增 composition blocker |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureMultiTransform.cpp::MultiTransform::getTransformations()` | Scaled 子特征走 diagonal method；`oldTransformations.size() % newTransformations.size()` 不整除时报 divisor error | cad-core 已按 slice COG 重建 scale transform；divisor 保持 structured diagnostic |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureScaled.cpp::Scaled::getTransformations()` | Scaled 返回含 identity 的 transformation list；以 first original COG 为中心，Whole shape 空 originals 时使用默认原点 | cad-core 子模板先取非 identity list 再 `prependIdentity()`，Whole shape 传空 originals，语义等价 |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::isMultiTransformChild()` | MultiTransform 子特征模板不独立执行成最终 Shape | cad-core `transformation_template`、runtime template set 和 collector target skip 均已覆盖 |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute()` | `Whole shape` 使用 support shape 复制并 fuse，不消费 hidden originals 作为 original feature replay | cad-core `applyWholeShapeTransforms()` 解析 Body / BaseFeature support，再复制 support transforms |

## cad-core 审计结论

| 文件 | 证据 | S5 判断 |
| --- | --- | --- |
| `cad-core/src/part_design/feature_multi_transform.cpp` | `childTemplateTransforms()` 只接受 transformed child 且要求 `TransformMode=Features`；非 Scaled 走 `newTransform * oldTransformations[oldIndex]`；Scaled 走 divisor check、sliceLength 和 COG diagonal；末尾删除 identity | MultiTransform composition 与 FreeCAD 对齐；`multi-transform-linear-mirror` / `multi-transform-whole-shape` 的剩余失败不是 composition 前置 blocker |
| `cad-core/src/part_design/feature_scaled.cpp` | `scaledTransforms()` 使用 first original AddSubShape COG；Whole shape 传空 originals 保持原点；模板执行时只发布 `transformation_template` | Scaled diagonal regression 保持，不能把 Whole shape topology mismatch 归成 Scaled 参数缺口 |
| `cad-core/src/part_design/feature_transformed.cpp` | `publishTransformationTemplate()` 不发布 shape；`applyFeatureTransforms()` / `applyWholeShapeTransforms()` 用 `TransformN` owner 调 `transformedCopy()` 和 boolean history；Whole shape report resolved support owner | child template skip Shape golden 与 Whole shape support path 已有实现证据 |
| `cad-core/src/part/topo_shape.cpp` | `namedShapeForTransformedCopy()` 复制 / retag source ElementMap 并传播 nested history，注释明确不从 result geometry derive ownership | fallback 不应落到几何猜测；S6 应继续补 topology / ElementMap 主路径 |
| `cad-core/src/adapters/c_api/c_api.cpp` | capabilities maker_history 含 `transformed_pattern_addsub_ownership` / `transformed_pattern_full_history`，producer matrix `transformed.status=covered` 且 remaining 为空 | ownership / fallback release evidence 保持 supported/covered |
| `cad-core/tests/test_p7_features.py` | MultiTransform tests 覆盖 non-Scaled composition、Scaled diagonal、divisor diagnostic、Whole shape support 和 child `transformation_template` | 保持 focused regression；S6 修 topology backendGap 时不得删除这些约束 |

## fixture / collector 复核

| 项 | S5 结论 |
| --- | --- |
| `multi-transform-linear-mirror` | FreeCAD topology 已采为 E24/F12/V16，cad-core 当前 E56/F28/V32；composition / fallback 无新增 blocker，进入 S6 topology backendGap |
| `multi-transform-scaled-diagonal` | expected 已冻结 E36/F18/V24 且当前测试通过；保持 scaled-diagonal regression |
| `multi-transform-scaled-divisor-known-gap` | 无 native expected；当前输出 `invalid_length` / `Transformations` / message 含 `divisor`，保持 diagnostic，不转几何实现 |
| `multi-transform-whole-shape` | expected 只冻结 MultiTransform bbox / volume；collector target list 跳过 `Transformations` 子模板 Shape，剩余 mismatch 进入 support-backed topology backendGap |
| transformation child template | `collect_freecad_expected.py::target_names()` 收集 MultiTransform `Transformations` 为 `transformation_templates`，Body member auto target 插入时排除这些 child；cad-core runtime 同步发布 `transformation_template=true` |

## 禁止模式 rg 结论

```bash
rg -n "fixture|fixtures/p7|geometry[-_ ]?match|match.*geometry|source index|source_index|sourceIndex|source[_A-Za-z]*index|transform index|transform_index|transformIndex|transform[_A-Za-z]*index|bbox|volume|area|length|Bnd|GProp|Mass\\(|LinearProperties|SurfaceProperties|VolumeProperties|guess|heuristic|fallback|prun" \
  cad-core/src/part_design/feature_multi_transform.cpp cad-core/src/part_design/feature_transformed.cpp cad-core/src/part_design/feature_scaled.cpp \
  cad-core/src/part/topo_shape.cpp cad-core/tests/test_p7_features.py cad-core/src/adapters/c_api/c_api.cpp
```

结论：未发现 P7 MultiTransform / transformed ownership 主路径按 fixture 名、geometry-match、source index、transform index、bbox / volume / area / length 猜 ownership。命中分类如下：

- `fixture` / `fixtures`：测试和 capability JSON 的清单字段；audited C++ 主路径无 fixture 名分支。
- `bbox` / `volume`：`publishTransformedResult()` metadata 和测试断言；未参与 ownership 或 topology 归属判断。
- `GProp` / `VolumeProperties`：FreeCAD 对齐的 Scaled / MultiTransform first original COG 计算；不是 bbox/volume 反推 ownership。
- `fallback`：属性读取默认值、C API 能力说明、Hole / Thread / Sketch InternalShape 诊断名；未命中 MultiTransform ownership fallback。
- `source_edge_index` / `sourceIndex`：`topo_shape.cpp` 的 Sketch InternalShape / WireJoiner evidence；不是 transformed family ownership。
- `transformedIndex` / `TransformN`：只用于生成稳定 owner 名和测试别名，不作为分支条件或输出排序修正。
- `geometry matching`：仅命中 topo 注释，表达禁止从 summary counts 或 geometry matching 推断 InternalFace ownership。

## 矩阵裁决

- `P7T-SCOPE-005`：保持 `backendGap`，但 S5 结论明确为 topology backendGap；MultiTransform non-Scaled multiplication、Scaled diagonal、divisor diagnostic、template skip 和 Whole shape support path 均无新增 S6 前置 blocker。
- `P7T-BLOCK-004`：S6 只需修 collected mismatch 的 topology backendGap，大概率落在 common transformed / refine / topo path；`feature_multi_transform.cpp` 作为 regression guard，只有 S6 证明 composition-specific gap 时才改。
- `P7T-SCOPE-006` / `P7T-BG-002`：ownership / fallback release evidence 维持 supported/covered；不新增 C++ landing。
- `P7T-BG-001`：继续聚合 S3 collected topology mismatch；S5 不扩大 blocker。
- `P7T-NG-003`：完整 transformed 参数全集仍为 nonGoal；unsupported child type、missing transformation 和 invalid divisor 继续输出 structured diagnostic。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_multi_transform_combines_linear_pattern_and_mirror \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_multi_transform_scaled_child_uses_diagonal_composition \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_multi_transform_scaled_divisor_gap_is_explicit \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_multi_transform_whole_shape_uses_support_and_child_transforms
```

结果：`Ran 4 tests`，`OK`。

P7 全部 TSV 字段数检查无输出；`git diff --check` 无输出。

## 非目标

- 不实现 MultiTransform 的 additive method 草案。
- 不把 divisor error 改成自动近似修复。
- 不把 MultiTransform 子模板当作普通独立 transformed feature 发布。
- 不把 S6 topology backendGap 宣称为已实现。
