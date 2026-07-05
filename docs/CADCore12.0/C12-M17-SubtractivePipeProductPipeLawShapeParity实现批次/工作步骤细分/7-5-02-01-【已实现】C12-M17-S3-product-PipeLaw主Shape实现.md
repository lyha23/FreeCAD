# 【已实现】C12-M17 S3 product PipeLaw 主 Shape 实现

## 目标

实现 `SubtractivePipe` product PipeLaw 主响应发布 post-cut `featureShape`，同时保留 `AddSubShape` removed tool cache 和 PipeLaw product extension。

## 必读文件

- `../README.md`
- S1 / S2 已实现后的 step 文档和矩阵更新
- `cad-core/src/part_design/feature_pipe.cpp`
- `cad-core/tests/test_p7_features.py`
- `cad-core/fixtures/c6m3/expected/partdesign-pipe-interpolation-law-subtractive-product.freecad.json`

## 操作

1. 调整 `executePipeFeature()` 中 `publishedShape` / `publishedNamedShape` 的选择，让主 response 使用 `featureShape` / `featureNamedShape`。
2. 保留 `context.addSubShapes[object.name]` 对 `toolShape` / `toolNamedShape` 的 subtractive cache 写入。
3. 确认 `context.shapes[object.name]` 与主 response 一致，均为 `featureShape`。
4. 跑 S2 focused tests，必要时补断言。
5. 更新 implementation / validation / blocker 矩阵。
6. 将本步骤重命名为 `【已实现】`。

## 执行记录

- S3 已在 `cad-core/src/part_design/feature_pipe.cpp::executePipeFeature()` 关闭主响应覆盖：删除 `publishToolContractShape` 对 `publishedShape` / `publishedNamedShape` 的选择，主对象 response、bbox、volume、solid_count 和 named shape 固定使用 post-cut `featureShape` / `featureNamedShape`。
- `context.shapes[object.name]` 继续保存 `featureShape`；`context.addSubShapes[object.name].subtractive` 继续保存 pre-boolean `toolShape` / `toolNamedShape`，Body replay 和 removed-tool cache 未改。
- 旧 product PipeLaw 分支曾同时强制构建 mesh/subshapes。S3 保留该响应面，但改为通过 `forceProductPipeLawDisplayTopology` 从 `publishedShape` 构建，确保 product SubtractivePipe 的 mesh / subshapes 来自 post-cut feature shape，而不是 tool shape。
- `LawSamples` / `pipe_law` metadata 未删除；PartDesign axis reference behavior 未改。
- S3 补正 `cad-core/tests/test_p7_features.py` 中 focused test 的直写 shape guard：Additive product PipeLaw 仍为 `occt_solid`，Subtractive product PipeLaw 与 S2 expected 一致为 post-cut `occt_compound`。
- S3 语义修复暴露一个 S2 expected 前缀错误：product SubtractivePipe 主 feature 的 post-cut named shape 与现有非 product `partdesign-pipe-subtractive-body` 一样使用 `Body.*` base aliases；Body replay 结果才使用 `BasePad.*` aliases。因此仅将 `SubtractivePipeInterpolationProduct` 的 named-shape expected 前缀从 `BasePad` 改为 `Body`，Body expected 保持不变。
- 验证时发现 `cad-core/build/CMakeCache.txt` 指向 `/Users/li/Chili3DProject/cad-web-background/cad-core`。已保留该生成目录为 `cad-core/build.cad-web-background-stale-20260705-0238`，重新配置当前 checkout 的 `cad-core/build` 后执行本步骤验证。

## 关闭条件

- focused red tests 变绿。
- Body replay 和 AddSubShape cache 未回归。
- product PipeLaw metadata 未丢失。

## 非目标

- 不删除 PipeLaw product extension。
- 不新增 product-only preview 字段；如需要，另开契约包。
- 不修改 axis reference behavior。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c6m3_partdesign_pipe_interpolation_law_products_publish_contract_metadata
cd ..
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M17-SubtractivePipeProductPipeLawShapeParity实现批次/工作步骤细分 --format markdown
git diff --check
```
