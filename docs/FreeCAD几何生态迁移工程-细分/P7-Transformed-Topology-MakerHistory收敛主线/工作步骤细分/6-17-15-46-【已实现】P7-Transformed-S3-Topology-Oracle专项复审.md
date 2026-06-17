# 【已实现】P7 Transformed S3 Topology Oracle 专项复审

## 目标

审计当前只冻结 bbox / volume 的 transformed fixture，决定能否采集 FreeCAD topology_counts，并只把 cad-core 当前已匹配 FreeCAD topology 的 expected 写成硬验收。S3 不写 C++，不从 cad-core 输出倒推 topology golden。

## live 基线

- `pwd`：`/home/user/Chili3DProject/FreeCAD`
- `HEAD`：`5ebb3e2081`
- `git log -1 --oneline`：`5ebb3e2081 test: 补齐 ExternalGeometry 状态机原生 oracle`
- `git status --short -uall`：进入 S3 前已有 P5/P6 相关脏改和 P7 主线未跟踪文件；本轮只修改 P7 S3 文档 / 矩阵和两个已匹配 expected，不 reset、不 revert、不提交。

## FreeCAD 依据

| 语义 | FreeCAD 依据 | 审计重点 |
| --- | --- | --- |
| Transformed execute | `FeatureTransformed.cpp::Transformed::execute()` | result shape 是否来自 `makeElementFuse` / `makeElementCut` 后的 solid |
| Refine | `FeatureRefine.cpp`; `src/Mod/Part/App/modelRefine.cpp` | refine=true topology 是否走 RefineModel history |
| native collector | `cad-core/tools/collect_freecad_expected.py` | 是否能创建 Body / Datum / transformed feature lifecycle 并采 topology_counts |

## 复核口径

- FreeCAD oracle：`python3 tools/collect_freecad_expected.py fixtures/p7/<fixture>.json --out /tmp/cadcore-p7-s3-fc/<fixture>.freecad.json --freecadcmd /home/user/.local/bin/freecadcmd`
- bbox / volume 复核：同一 collector 使用 `--check`，除 standalone `polar-pattern-whole-shape` 被 lifecycle guard 拒绝外均通过。
- cad-core 口径：`env CAD_CORE_TEST_LEGACY_OUTPUT=1 build/cad-core recompute fixtures/p7/<fixture>.json --output /tmp/cadcore-p7-s3-core/<fixture>.result.json`，只统计顶层 `subshapes[object]` 的 `Edge` / `Face` / `Vertex`，不使用 raw CLI `results[]`。

## 最终清单

| fixture | FreeCAD topology oracle | cad-core legacy subshapes | S3 结论 |
| --- | --- | --- | --- |
| `mirrored-pad-datum-plane` | `Mirrored` / `Body` E12/F6/V8 | `Mirrored` / `Body` E20/F10/V12 | collected oracle mismatch，expected 不写 topology；S6 落 `feature_mirrored` / `feature_transformed` / `topo_shape` |
| `mirrored-fillet-support-transform` | `Mirrored` / `Body` E18/F8/V12 | `Mirrored` / `Body` E26/F12/V16 | collected oracle mismatch；S6 落 Mirrored + DressUp SupportTransform ownership |
| `mirrored-dressup-chain-support-transform` | `Mirrored` / `Body` E29/F13/V18 | `Mirrored` / `Body` E39/F18/V23 | collected oracle mismatch；S4 继续审计链式 ownership，S6 落 C++ |
| `linear-pattern-pad-datum-line` | `LinearPattern` / `Body` E12/F6/V8 | `LinearPattern` / `Body` E28/F14/V16 | collected oracle mismatch；S6 落 `feature_linear_pattern` / `feature_transformed` / `topo_shape` |
| `linear-pattern-pad-sketch-axis` | `LinearPattern` / `Body` E12/F6/V8 | `LinearPattern` / `Body` E28/F14/V16 | collected oracle mismatch；expected 不写 topology |
| `linear-pattern-pad-two-directions` | `LinearPattern` / `Body` E24/F12/V16 | `LinearPattern` / `Body` E56/F28/V32 | collected oracle mismatch；S6 处理 two-direction topology ownership |
| `linear-pattern-custom-spacings` | `LinearPattern` / `Body` E36/F18/V24 | `LinearPattern` / `Body` E36/F18/V24 | matched；已写入 `topology_counts` |
| `linear-pattern-spacing-pattern` | `LinearPattern` / `Body` E48/F24/V32 | `LinearPattern` / `Body` E48/F24/V32 | matched；已写入 `topology_counts` |
| `linear-pattern-whole-shape-body-prefix-support` | `LinearPattern` / `Body` E12/F6/V8 | `LinearPattern` / `Body` E28/F14/V16 | collected oracle mismatch；S6 处理 Whole shape Body prefix support |
| `polar-pattern-whole-shape-body-prefix-support` | `PolarPattern` / `Body` E12/F6/V8 | `PolarPattern` / `Body` E32/F16/V18 | collected oracle mismatch；S6 落 `feature_polar_pattern` / `feature_transformed` |
| `polar-pattern-whole-shape` | collector 拒绝：缺 Body Group / BaseFeature lifecycle | 未作为 native golden 比对 | 保持 nonGoal / standalone geometry-equivalent boundary，不写 native topology expected |
| `scaled-whole-shape` | `Scaled` E12/F6/V8 | `Scaled` E21/F9/V14 | collected oracle mismatch；S6 落 `feature_scaled` / `feature_transformed` |
| `multi-transform-linear-mirror` | `MultiTransform` / `Body` E24/F12/V16 | `MultiTransform` / `Body` E56/F28/V32 | collected oracle mismatch；S5 继续审计组合语义，S6 落 `feature_multi_transform` |
| `multi-transform-whole-shape` | `MultiTransform` E12/F6/V8 | `MultiTransform` E28/F14/V16 | collected oracle mismatch；expected 不写 topology |
| `multi-transform-scaled-diagonal` | 已有 expected E36/F18/V24 | cad-core E36/F18/V24 | existing matched control；本轮不重写 |

## 回写结果

- `linear-pattern-custom-spacings.freecad.json` 和 `linear-pattern-spacing-pattern.freecad.json` 已补 `topology_counts`。
- mismatch 行没有写入 topology expected，避免引入必失败测试。
- `p7_transformed_scope_review_matrix.tsv`：SCOPE-001 到 SCOPE-005 从 oracle-pending 改为 collected oracle backendGap；matched spacing fixture 在 SCOPE-002 中记录为 regression。
- `p7_transformed_blocker_queue.tsv`：P7T-BLOCK-001 到 P7T-BLOCK-004 改为 S6 backend implementation 队列。
- `p7_transformed_backend_gap_classification.tsv`：P7T-BG-001 改为 collected-oracle-backendGap，明确 S6 C++ 落点：`feature_transformed`、`feature_mirrored`、`feature_linear_pattern`、`feature_polar_pattern`、`feature_scaled`、`feature_multi_transform`、`topo_shape`。
- `p7_transformed_non_goal_registry.tsv`：保留并具体化 `polar-pattern-whole-shape` standalone geometry-equivalent 边界。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_p7_linear_pattern_custom_spacing_list_controls_steps tests.test_p7_features.CadCoreP7FeatureTest.test_p7_linear_pattern_spacing_pattern_controls_steps
```

已运行结果：`Ran 2 tests`，`OK`。TSV 字段数检查和 `git diff --check` 作为本轮完成验收执行。

本轮完成验收结论：

- 更新后的 `linear-pattern-custom-spacings` 与 `linear-pattern-spacing-pattern` collector `--check` 均通过。
- P7 全部 TSV 字段数检查无输出。
- Focused unittest：`Ran 2 tests`，`OK`。
- `git diff --check` 无输出。

## 非目标

- 不在 S3 修改 topology 断言阈值。
- 不放宽 expected 来适配当前 cad-core。
- 不把 OCCT 版本导致的 bbox_delta 当成 topology 语义问题。
- 不标 S4、S5、S6 或整条 P7 主线已实现。
