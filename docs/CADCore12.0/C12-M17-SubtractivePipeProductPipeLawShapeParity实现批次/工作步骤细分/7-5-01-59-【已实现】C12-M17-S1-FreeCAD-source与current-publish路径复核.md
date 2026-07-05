# 【已实现】C12-M17 S1 FreeCAD source 与 current publish 路径复核

## 目标

复核 FreeCAD `Pipe::execute()` 的 feature `Shape` 生命周期与 cad-core 当前 product PipeLaw 发布路径，确认 S2/S3 的 red path 和 C++ 落点。

## 必读文件

- `../README.md`
- `../7-5-01-56-C12-M17-SubtractivePipeProductPipeLawShapeParity实现批次方案.md`
- `src/Mod/PartDesign/App/FeaturePipe.cpp`
- `src/Mod/PartDesign/App/FeatureAddSub.cpp`
- `cad-core/src/part_design/feature_pipe.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_adapters.py`
- `cad-core/fixtures/c6m3/expected/partdesign-pipe-interpolation-law-subtractive-product.freecad.json`
- `../矩阵/c12m17_subtractive_pipe_product_shape_source_matrix.tsv`
- `../矩阵/c12m17_subtractive_pipe_product_shape_scope_matrix.tsv`

## 操作

1. 记录 FreeCAD `AddSubShape`、Cut/Fuse、`rawShape` 和 `Shape.setValue(...)` 调用顺序。
2. 记录 current `featureShape` / `toolShape` / `publishedShape` / `publishToolContractShape` 的关系。
3. 复核 current C6-M3 expected 和 focused tests 是否把主响应 tool shape 固化成 product contract。
4. 复核 axis `allowGeometricallyLinearCurve=true` 的 current landing，确认本包不改。
5. 更新 source / scope / contract / validation 矩阵。
6. 将本步骤重命名为 `【已实现】`。

## 执行记录

- live 基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=474097e0f6`，`git log -1 --oneline` 为 `474097e0f6 feat: 补齐引用轴和基准平面契约`；起点 dirty boundary 沿用 S0，不改 production C++、fixtures、expected、tests、adapters 或 FreeCADCmd。
- FreeCAD `src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::execute()` 调用顺序已复核：先把 swept result shell 转 solid，再 `AddSubShape.setValue(result.makeElementCompound(...))` 保存 pre-boolean tool cache；无 base 且 subtractive 时直接报 `Pipe: There is nothing to subtract from`；有 base 时按 Additive/Subtractive 选择 `Part::OpCodes::Fuse` / `Part::OpCodes::Cut`，执行 `boolOp.makeElementBoolean(...)`，随后 `rawShape = boolOp`、refine、`Shape.setValue(getSolid(boolOp))`，最终 feature `Shape` 仍是 post-boolean result。
- FreeCAD `src/Mod/PartDesign/App/FeatureAddSub.cpp::FeatureAddSub::getAddSubShape()` 已复核：Additive 把 `AddSubShape.getShape()` 写入 `addShape`，Subtractive 写入 `subShape`；`updatePreviewShape()` 对 subtractive 的 removed volume preview 使用 base/tool `Common`，不改变 downstream cache 语义。
- current `cad-core/src/part_design/feature_pipe.cpp::executePipeFeature()` 落点已定位：`toolShape` 是 pipe tool solid，`featureShape` 初始等于 `toolShape`；Body prefix 存在时先以 `Fuse/Cut` 生成 refined `featureShape` / `featureNamedShape`；随后 `publishToolContractShape = bodyPrefix && !additive && usesCadCoreProductPipeLaw(pipeLaw)` 会让 `publishedShape` / `publishedNamedShape` 改选 `toolShape` / `toolNamedShape`。`context.shapes[object.name]` 仍保存 `featureShape`，`context.addSubShapes[object.name].subtractive` 仍保存 `toolShape`，因此 S2/S3 红路径只在主 response publish surface。
- current C6-M3 expected/test 已锁定 tool-shape 主响应：`partdesign-pipe-interpolation-law-subtractive-product.freecad.json` 标记 `freecad_native_parity=false`，并把 `SubtractivePipeInterpolationProduct` 的 bbox、topology counts、volume 固定为 removed tool；`test_c6m3_partdesign_pipe_interpolation_law_products_publish_contract_metadata` 通过 `assert_object_matches_expected` 校验 object fields、bbox、volume、subshapes 和 named shape。capability / adapter 当前主要锁定 PipeLaw product extension、fixture 列表和 supported wording，未单独提供“主 response 必须是 toolShape”的 source authority。
- `allowGeometricallyLinearCurve=true` current landing 已复核：Extrude reference axis、Revolution/Groove reference axis、PolarPattern rotation axis 使用 shared `edge_axis` helper 并允许 geometrically linear non-Line curve；LinearPattern direction 仍保持 strict straight-line boundary。本包保留该 product extension，不把它改成 strict FreeCAD parity，也不在 S3/S4 删除。

## 关闭条件

- FreeCAD source authority 可追溯。
- current mismatch landing 可定位到 `publishToolContractShape`。
- S2 red fixture / expected 迁移面明确。

## 非目标

- 不修改 production C++。
- 不刷新 expected。
- 不移除 axis product extension。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'AddSubShape|OpCodes::Cut|Shape.setValue|getAddSubShape' src/Mod/PartDesign/App/FeaturePipe.cpp src/Mod/PartDesign/App/FeatureAddSub.cpp
rg -n 'publishToolContractShape|usesCadCoreProductPipeLaw|publishedShape|featureShape|toolShape|allowGeometricallyLinearCurve' cad-core/src/part_design/feature_pipe.cpp cad-core/src/part_design/feature_extrude.cpp cad-core/src/part_design/feature_revolved.cpp cad-core/src/part_design/feature_transformed.cpp
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M17-SubtractivePipeProductPipeLawShapeParity实现批次/工作步骤细分 --format markdown
git diff --check
```
