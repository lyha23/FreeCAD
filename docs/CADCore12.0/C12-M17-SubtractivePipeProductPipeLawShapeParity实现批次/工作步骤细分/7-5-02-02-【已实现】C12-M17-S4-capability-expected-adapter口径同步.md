# 【已实现】C12-M17 S4 capability / expected / adapter 口径同步

## 目标

同步公开口径：PipeLaw 仍是 CAD Core product extension，但 `SubtractivePipe` 主 `Shape` lifecycle 已回到 FreeCAD-compatible post-cut feature shape。

## 必读文件

- `../README.md`
- S3 已实现后的 step 文档和矩阵更新
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_adapters.py`
- `cad-core/fixtures/c6m3/expected/partdesign-pipe-interpolation-law-subtractive-product.freecad.json`
- `docs/capability/7-5-00-14-cad-web-background非FreeCAD原生语义边界.md`

## 操作

1. 更新 capability wording：保留 PipeLaw product extension，移除主 tool-shape 发布偏差。
2. 更新 adapter capability assertions。
3. 更新 expected wording / metadata，区分 product PipeLaw extension 与 main-shape parity。
4. 记录 capability 边界文档后续同步建议；若本步直接修改该文档，必须只移除第一类偏差，保留 axis product extension。
5. 更新 validation / blocker / implementation 矩阵。
6. 将本步骤重命名为 `【已实现】`。

## 执行记录

- `cad-core/src/runtime/capability_contract.cpp` 保留 `Transformation=Linear PipeLaw`、`Transformation=S-shape PipeLaw`、`Transformation=Interpolation LawSamples product contract` 三类 CAD Core product extension wording，并新增 `SubtractivePipe product PipeLaw main Shape lifecycle: FreeCAD-compatible post-cut feature Shape`，明确 S3 后 product PipeLaw provenance 不再改变主 `Shape` lifecycle。
- `cad-core/tests/test_adapters.py` 同步新增 adapter capability assertion；`c6m3/partdesign-pipe-interpolation-law-subtractive-product` 仍留在 fixture 列表中，作为 product PipeLaw fixture，而不是 native PipeLaw support。
- `cad-core/fixtures/c6m3/expected/partdesign-pipe-interpolation-law-subtractive-product.freecad.json` 已复核：expected wording 保留 `pipe_law=cad_core_product_extension`，同时记录 `main_shape_lifecycle=freecad_compatible_post_cut_feature_shape`，本步无需再改。
- `docs/capability/7-5-00-14-cad-web-background非FreeCAD原生语义边界.md` 已移除第一类当前非原生差异，把 Body `SubtractivePipe` product PipeLaw 主 `Shape` lifecycle 标为已整改；PartDesign 几何共线 BSpline / 非 Line 轴引用 section 保留为当前非原生 product extension。
- C12-M17 README、方案和矩阵已同步到 S4 closed 状态。

## 关闭条件

- capability / expected / adapter wording 一致。
- axis product extension 未被误删或改写为 native parity。
- 下一步只剩 S5 发布。

## 非目标

- 不扩大为全 capability 文档重写。
- 不改 C12-M13 / C12-M14 已发布结论，除非直接引用需要状态补充。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c6m3_partdesign_pipe_interpolation_law_products_publish_contract_metadata
cd ..
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M17-SubtractivePipeProductPipeLawShapeParity实现批次/工作步骤细分 --format markdown
git diff --check
```
