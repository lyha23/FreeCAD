# 【已实现】P7 Transformed S6 Oracle 实现与发布闸门

## 目标

消费 S2-S5 留下的 P7 transformed topology `backendGap`，关闭 Mirrored、LinearPattern、PolarPattern Whole shape、Scaled Whole shape 和 MultiTransform 的 collected FreeCAD topology mismatch。S6 不扩大到 standalone `polar-pattern-whole-shape` native golden，也不触碰 P5/P6 expected。

## FreeCAD 依据

| 语义 | FreeCAD authority | 本轮结论 |
| --- | --- | --- |
| PartDesign refine 默认值 | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRefine.cpp::FeatureRefine()` | `Refine` 初始化来自 `GetBool("RefineModel", true)` |
| transformed 执行收口 | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute()` | `getTransformedCompShape()` 跳过 identity，执行 fuse/cut 后调用 `supportShape = refineShapeIfActive((supportShape))` |
| 全局参数默认值 | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Feature.cpp::getPDRefineModelParameter()` | `RefineModel` 默认 true |

## 实现结果

- `cad-core/src/runtime/feature_executor.cpp` / `.h` 新增 PartDesign `FeatureRefine` 读取入口：显式 `Refine=false` 仍 no-op，缺失 `Refine` 可按 FreeCAD `RefineModel=true` 语义读取。
- `cad-core/src/part_design/feature_transformed.cpp` 在 transformed family final-result refine 调用点使用 PartDesign 缺省 true；Pad/Pocket/Body/DressUp 的非本轮 missing-Refine 行为保持不变，避免把 P3/P6/C3M5 expected 纳入 S6。
- `cad-core/tests/test_p7_features.py` 的 P7 transformed ownership 断言允许 source aliases 通过 ElementMap、非 indexed history 或 mapper_history 可见；TransformN copied alias 仍要求进入 ElementMap。
- 已用 `/home/user/.local/bin/freecadcmd` 重新采集 11 个 target fixture 的 FreeCAD topology expected；checked-in expected 只写入当前 cad-core 已匹配的 `topology_counts`，未写 `polar-pattern-whole-shape` standalone native expected。

## Expected 更新

写入 `topology_counts` 的 target：

- `mirrored-pad-datum-plane`
- `mirrored-fillet-support-transform`
- `mirrored-dressup-chain-support-transform`
- `linear-pattern-pad-datum-line`
- `linear-pattern-pad-sketch-axis`
- `linear-pattern-pad-two-directions`
- `linear-pattern-whole-shape-body-prefix-support`
- `polar-pattern-whole-shape-body-prefix-support`
- `scaled-whole-shape`
- `multi-transform-linear-mirror`
- `multi-transform-whole-shape`

额外 P7 transformed expected 修正：`mirrored-whole-shape` 的 `Pad.` source alias 改按 history source prefix 验证，因为 final-result RefineModel 后不再保证该 alias 直接留在 ElementMap。

## 矩阵裁决

- `P7T-SCOPE-001` 到 `P7T-SCOPE-005`：由 `backendGap` 转为 `supported`，S3 collected topology mismatch 已由 transformed final-result refine 闭合。
- `P7T-BLOCK-001` 到 `P7T-BLOCK-004`：关闭，保留 focused regression。
- `P7T-BLOCK-005` / `P7T-SCOPE-006`：S4/S5 已支持，本轮无新增 ownership/fallback backendGap。
- `P7T-NG-005`：`polar-pattern-whole-shape` standalone geometry-equivalent boundary 保持 nonGoal。

## 验证

本轮已执行：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_refine_false_is_feature_refine_noop \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_mirrored_features_mode_fuses_transformed_additive_original \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_mirrored_features_mode_consumes_dressup_support_transform_cache \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_mirrored_features_mode_consumes_chained_dressup_support_transform_cache \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_linear_pattern_features_mode_fuses_additive_originals_by_extent \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_linear_pattern_uses_sketch_construction_axis \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_linear_pattern_combines_two_directions \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_linear_pattern_custom_spacing_list_controls_steps \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_linear_pattern_spacing_pattern_controls_steps \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_linear_pattern_whole_shape_uses_body_prefix_support \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_polar_pattern_whole_shape_fuses_transformed_support \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_polar_pattern_whole_shape_uses_body_prefix_support \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_scaled_whole_shape_scales_support_around_origin \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_multi_transform_combines_linear_pattern_and_mirror \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_multi_transform_scaled_child_uses_diagonal_composition \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_multi_transform_scaled_divisor_gap_is_explicit \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_multi_transform_whole_shape_uses_support_and_child_transforms
```

结果：`Ran 17 tests`，`OK`。

```bash
python3 -m unittest tests/test_expected_fixtures.py
```

结果：全量 expected 仅剩 unrelated `p7/hole-supported-model-thread-counterbore` 体积差 `1.0089350496400584e-05` 超过 `1e-05` 容差；该项不属于 transformed S6，本轮未修改。

```bash
python3 - <<'PY'
# selected transformed expected checks
PY
```

结果：`selected expected checks OK: 12 fixtures`。

## 非目标

- 不实现 Assembly solver。
- 不把跨请求 shape cache / BREP 作为 transformed ownership 证据。
- 不把 topology 命名顺序差异单独判为失败。
- 不把 standalone `polar-pattern-whole-shape` 写成 native topology expected。
