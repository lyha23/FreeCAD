# 【已实现】P7 Transformed Topology / MakerHistory 收敛主线总入口

本文是 `docs/FreeCAD几何生态迁移工程-细分` 下的 P7 transformed family 专项主线，来源于 `docs/CADCore方案/细化方案/10-P7-PartDesign常用生态.md` 中“Mirrored / Pattern / Scaled / MultiTransform 部分 native oracle 只冻结 bbox / volume”的缺口。S6 已把本主线收敛为 checked-in topology_counts 和 focused regression。

## 主线目标

- 审计 P7 transformed family 里只冻结 bbox / volume 的 native expected：Mirrored、LinearPattern、PolarPattern、Scaled、MultiTransform。
- 区分三类状态：缺 FreeCAD topology oracle 的 `notCollected`、已有 FreeCAD oracle 且 cad-core 不匹配的 `backendGap`、已有实现但需要发布前拓扑 / history 闸门的 `releaseGate`。
- 把可实现缺口落到 `cad-core/src/part_design/feature_transformed.cpp`、各 transformed 子 feature、`cad-core/src/part/topo_shape.cpp` 和 focused P7 tests；不得从 fixture 输出倒推业务逻辑。

## 当前基线

- Mirrored / LinearPattern / PolarPattern / Scaled / MultiTransform 的基础几何路径已可运行，P7 tests 已覆盖 transform mode、originals、body tip、source alias、terminal history 和部分 topology_counts。
- S3 已采集 bbox / volume-only 清单的 FreeCAD topology oracle；S6 已关闭 collected mismatch，并把 target topology_counts 写入 checked-in expected。
- S6 关闭的 target 包括：`mirrored-pad-datum-plane`、`mirrored-fillet-support-transform`、`mirrored-dressup-chain-support-transform`、`linear-pattern-pad-datum-line`、`linear-pattern-pad-sketch-axis`、`linear-pattern-pad-two-directions`、`linear-pattern-whole-shape-body-prefix-support`、`polar-pattern-whole-shape-body-prefix-support`、`scaled-whole-shape`、`multi-transform-linear-mirror`、`multi-transform-whole-shape`。
- `polar-pattern-whole-shape` 被 collector 拒绝为缺 Body / BaseFeature lifecycle 的 standalone geometry-equivalent 用例，继续保留 nonGoal 边界。
- 已有 topology_counts 的对照包括：`mirrored-refine-true`、S6 关闭的 11 个 target、`linear-pattern-pad-pocket-multi-original`、`linear-pattern-pocket-subtractive-original`、`linear-pattern-custom-spacings`、`linear-pattern-spacing-pattern`、`linear-pattern-whole-shape-refined-prefix-support`、`polar-pattern-pad-datum-line`、`polar-pattern-pad-sketch-axis`、`polar-pattern-spacing-pattern`、`scaled-pad-factor-two`、`multi-transform-scaled-diagonal`。

## 证明链条

```text
声明口径
  -> FreeCAD transformed source candidates
  -> bbox/volume-only expected 审计
  -> topology oracle / maker-history scope review
  -> Mirrored / Pattern / Scaled / MultiTransform 专项复审
  -> oracle 采集或 backendGap 落 C++
  -> P7 transformed 发布闸门
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| Transformed 主执行 | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute()` | `Features` 模式逐个 original 调 `getAddSubShape()` 后 transform / fuse / cut；`Whole shape` 模式 transform support shape |
| Mirrored | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureMirrored.cpp::Mirrored::getTransformations()` | DatumPlane 或 planar face 生成 mirror transform |
| LinearPattern | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureLinearPattern.cpp` | Extent / Spacing / Spacings / SpacingPattern / 双方向生成 transform list |
| PolarPattern | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePolarPattern.cpp` | DatumLine / shape Edge / Sketch axis 生成 rotation transform list |
| Scaled | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureScaled.cpp::Scaled::getTransformations()` | 以第一个 original AddSubShape 质心为中心，Whole shape 时使用默认原点 |
| MultiTransform | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureMultiTransform.cpp::MultiTransform::getTransformations()` | 非 Scaled 子特征做乘法组合；Scaled 子特征做 diagonal 组合 |
| transformed topo history | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementTransform()` | transform 后复制 ElementMap，而不是按几何猜 ownership |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| transformed executor | `cad-core/src/part_design/feature_transformed.cpp` | Features / Whole shape common flow、AddSubShape transform、boolean merge、result publish |
| 子 feature | `cad-core/src/part_design/feature_mirrored.cpp`; `feature_linear_pattern.cpp`; `feature_polar_pattern.cpp`; `feature_scaled.cpp`; `feature_multi_transform.cpp` | 生成 FreeCAD 对齐的 transform list 和模板语义 |
| topo history | `cad-core/src/part/topo_shape.cpp`; `cad-core/include/cad_core/part/topo_shape.h` | `namedShapeForTransformedCopy()`、source alias retag、terminal / merge history 传播 |
| expected collector | `cad-core/tools/collect_freecad_expected.py` | 补 topology_counts / native oracle 采集边界 |
| tests | `cad-core/tests/test_p7_features.py`; `cad-core/tests/test_expected_fixtures.py`; `cad-core/tests/test_adapters.py` | focused transformed parity、expected fixture parity、capability gap 声明 |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-17-15-42-P7-Transformed工作步骤总入口.md` | S0-S6 执行索引 |
| S0 声明口径 | `工作步骤细分/6-17-15-43-【已实现】P7-Transformed-S0-声明口径与live基线复核.md` | 冻结当前声明、排除项和状态词典 |
| S1 源码候选 | `工作步骤细分/6-17-15-44-【已实现】P7-Transformed-S1-FreeCAD源码候选矩阵.md` | 生成 FreeCAD / cad-core 候选证据 |
| S2 范围准入 | `工作步骤细分/6-17-15-45-【已实现】P7-Transformed-S2-范围准入与blocker矩阵.md` | 分类 scope、blocker、nonGoal 和 backendGap |
| S3 oracle 审计 | `工作步骤细分/6-17-15-46-【已实现】P7-Transformed-S3-Topology-Oracle专项复审.md` | 审计 bbox/volume-only expected 和 topology oracle |
| S4 ownership 审计 | `工作步骤细分/6-17-15-47-【已实现】P7-Transformed-S4-Pattern-Ownership专项复审.md` | 已审计 AddSubShape slot、transform source alias 和 terminal history；ownership release evidence supported/covered |
| S5 MultiTransform / fallback | `工作步骤细分/6-17-15-48-【已实现】P7-Transformed-S5-MultiTransform-Fallback专项复审.md` | 已审计组合语义和发布前 fallback 边界；无新增 S6 前置 blocker |
| S6 发布闸门 | `工作步骤细分/6-17-15-49-【已实现】P7-Transformed-S6-Oracle实现与发布闸门.md` | 已消费 blocker，落 C++ / expected / focused tests |
| source candidates | `矩阵/p7_transformed_source_candidates.tsv` | FreeCAD source 候选 |
| scope review | `矩阵/p7_transformed_scope_review_matrix.tsv` | 语义项状态矩阵 |
| blocker queue | `矩阵/p7_transformed_blocker_queue.tsv` | 可执行 blocker 队列 |
| non-goal registry | `矩阵/p7_transformed_non_goal_registry.tsv` | 非目标与 reopen 条件 |
| backend gap classification | `矩阵/p7_transformed_backend_gap_classification.tsv` | backendGap / lifecycle boundary 聚合 |

当前 S0-S6 均已实现。S6 用 transformed final-result `FeatureRefine` 缺省 true 闭合 S3 collected topology backendGap，SCOPE-001 到 SCOPE-005 转为 supported；S4/S5 ownership、fallback 和 composition evidence 保持 supported/covered。`polar-pattern-whole-shape` standalone geometry-equivalent 用例仍保持 nonGoal，不作为 native topology golden。
