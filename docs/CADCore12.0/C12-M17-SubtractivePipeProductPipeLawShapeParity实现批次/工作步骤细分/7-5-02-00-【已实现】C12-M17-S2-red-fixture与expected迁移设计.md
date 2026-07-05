# 【已实现】C12-M17 S2 red fixture 与 expected 迁移设计

## 目标

先锁定 focused test / expected 迁移边界，确保 S3 不是按 fixture 名或体积常量做输出修正。

## 必读文件

- `../README.md`
- S1 已实现后的 step 文档和矩阵更新
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_adapters.py`
- `cad-core/fixtures/c6m3/partdesign-pipe-interpolation-law-subtractive-product.json`
- `cad-core/fixtures/c6m3/expected/partdesign-pipe-interpolation-law-subtractive-product.freecad.json`
- `cad-core/fixtures/c12m13/expected/partdesign-pipe-subtractive-lifecycle.freecad.json`
- `../矩阵/c12m17_subtractive_pipe_product_shape_contract_matrix.tsv`
- `../矩阵/c12m17_subtractive_pipe_product_shape_implementation_matrix.tsv`

## 操作

1. 决定复用 C6-M3 fixture 还是新增 C12-M17 focused fixture。
2. 将 expected wording 迁移为 product PipeLaw extension + FreeCAD-compatible post-cut main shape。
3. 补 red test：主对象 response 来自 `featureShape`，`AddSubShape` 仍为 removed tool cache。
4. 保留 PipeLaw metadata / LawSamples 断言，证明 product extension 未被删除。
5. 保留 axis product extension 的非目标记录。
6. 更新 contract / implementation / validation 矩阵。
7. 将本步骤重命名为 `【已实现】`。

## 执行记录

- S2 决定复用 `cad-core/fixtures/c6m3/partdesign-pipe-interpolation-law-subtractive-product.json`，不新增 C12-M17 fixture。理由是该 fixture 已同时覆盖 Body 内 `SubtractivePipeInterpolationProduct`、`Transformation=Interpolation` + `LawSamples` product PipeLaw、Body replay 和主 pipe response，能直接把 S1 定位的 `publishToolContractShape` 红路径收敛到一个 focused case。
- C12-M13 `partdesign-pipe-subtractive-lifecycle.freecad.json` 已作为生命周期依据复核：普通 SubtractivePipe 的 feature `Shape` 与 Body 都是 post-cut result，`AddSubShape` 保留 removed tool cache。因此 C6-M3 expected 迁移时不再把 tool-shape 主响应称为 native parity 或 tool contract oracle。
- `cad-core/fixtures/c6m3/expected/partdesign-pipe-interpolation-law-subtractive-product.freecad.json` 已迁移 wording：`LawSamples PipeLaw` 是 CAD Core product extension，但 `SubtractivePipe` main `Shape` lifecycle 应为 FreeCAD-compatible post-cut feature `Shape`。`SubtractivePipeInterpolationProduct` 的 expected 主对象 `shape`、bbox、volume、topology counts、mesh summary、subshape surface 和 named-shape source prefixes 改为 post-cut result；`Body` expected 继续锁定 cut replay。
- `cad-core/tests/test_p7_features.py::test_c6m3_partdesign_pipe_interpolation_law_products_publish_contract_metadata` 保留 PipeLaw metadata / LawSamples / `cad_core_product_contract` 断言，证明 product extension 未删除；subtractive 分支继续断言 Body tip 与 `replayed_subtractive_features`，并在 expected green 后比较 pipe main response 与 Body post-cut response 的 shape / bbox / volume / subshapes / mesh summary。
- S2 未修改 production C++、adapter/capability production wording、FreeCADCmd、axis product extension 或 product PipeLaw support。
- S2 focused validation 当前按预期 red：`python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c6m3_partdesign_pipe_interpolation_law_products_publish_contract_metadata` 失败于 `assert_object_matches_expected(..., "c6m3", "partdesign-pipe-interpolation-law-subtractive-product")`，核心断言为 `AssertionError: 'occt_solid' != 'occt_compound' : object_fields.shape`。这是 S3 要关闭的 red path：当前主 pipe response 仍是 removed tool `toolShape`，expected 已要求 post-cut feature shape。
- S2 轻量验证已通过：队列从 S3 开始；C12-M17 TSV 字段数检查无输出；`git diff --check` 无输出。

## 关闭条件

- red/green surface 能约束主 `Shape` 和 `AddSubShape` 分离。
- expected wording 不再把主 tool shape 称为 native parity。
- 下一步可进入 S3 C++ 实现。

## 非目标

- 不按 fixture 名称分支。
- 不用 bbox / volume 常量补业务逻辑。
- 不改 axis product extension。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c6m3_partdesign_pipe_interpolation_law_products_publish_contract_metadata
cd ..
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M17-SubtractivePipeProductPipeLawShapeParity实现批次/工作步骤细分 --format markdown
git diff --check
```
