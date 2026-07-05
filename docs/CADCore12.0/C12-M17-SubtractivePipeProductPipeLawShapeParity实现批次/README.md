# C12-M17 SubtractivePipe product PipeLaw Shape parity 实现批次

C12-M17 处理 `docs/capability/7-5-00-14-cad-web-background非FreeCAD原生语义边界.md` 中用户明确选择收敛的第一类偏差：`SubtractivePipe` 位于 Body 内且使用 CAD Core product PipeLaw 时，主 `Shape` lifecycle 必须回到 FreeCAD `PartDesign::Pipe` 的 post-cut feature `Shape`。

本包只修正主 `Shape` / mesh / subshapes / namedShape / bbox / volume 的发布语义。CAD Core product PipeLaw 本身仍保留为产品扩展；PartDesign 轴引用接受几何共线 BSpline / 非 Line 曲线也按用户要求继续支持，并在本包中冻结为 `cad_core_product_extension` 非目标，不改成严格 FreeCAD parity。

## 当前判断

- 最终状态：`implemented_freecad_main_shape_parity_product_law_retained`。S5 发布闸门已关闭，C12-M17 队列关闭后只应输出 markdown 表头。
- C12-M13 已经关闭普通 AdditivePipe / SubtractivePipe 的 `AddSubShape`、`rawShape` 与 Boolean lifecycle：pre-boolean tool cache 留在 `AddSubShape`，feature `Shape` 在有 base 时发布 Fuse / Cut 后结果。
- C12-M17 S3 已关闭 product PipeLaw 对主响应 shape 的覆盖；主对象 response、mesh、subshapes、namedShape、bbox 和 volume 均跟随 post-cut `featureShape`。
- S4 公开口径：PipeLaw `LawSamples` / `Linear` / `S-shape` 仍是 CAD Core product extensions；Body `SubtractivePipe` product PipeLaw 主 `Shape` lifecycle 是 FreeCAD-compatible post-cut feature `Shape`。
- `PartDesign` axis `allowGeometricallyLinearCurve=true` 继续保留。它不是本包要修的 bug，而是明确保留的 CAD Core 产品扩展。

## S0 live 基线

- S0 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=474097e0f6`（`474097e0f6 feat: 补齐引用轴和基准平面契约`）。
- C12-M1..M16 live queue 均只输出 markdown 表头；C12-M17 起点第一条 pending 为 S0。
- 起点 dirty boundary 已冻结在 S0 step 文件中：已有 C12-M11 / M15 / M16 / capability / root README dirty docs 与未跟踪 C12-M17 package 文件并存，S0 只修改 C12-M17 docs / matrices 与 root README 中的 S0 状态。
- 用户决策冻结为：修复 `SubtractivePipe product PipeLaw` 主 `Shape`；保留几何共线 BSpline / 非 Line PartDesign 轴引用产品扩展。

## S1 source/current audit

- S1 已按 live source 关闭：FreeCAD `Pipe::execute()` 先写 `AddSubShape` pre-boolean tool cache，再按 Additive/Subtractive 执行 `Fuse` / `Cut`，`rawShape` 与 feature `Shape.setValue(...)` 写入 post-boolean result。
- `FeatureAddSub::getAddSubShape()` downstream 语义是从 `AddSubShape` 取 additive/subtractive tool cache，不从 feature 主 `Shape` 反推 removed tool。
- S1 当时的 cad-core 红路径定位到 `publishToolContractShape = bodyPrefix && !additive && usesCadCoreProductPipeLaw(pipeLaw)`：它只影响 product PipeLaw subtractive body response publish surface；`context.shapes` 与 `AddSubShape` cache 已分别保留 feature shape 与 tool cache。该红路径已由 S3 关闭。
- S1 当时的 C6-M3 expected/test 锁定了旧主响应差异；S2/S3 已将 expected/test 迁移到 product PipeLaw extension + FreeCAD-compatible post-cut main Shape。
- `allowGeometricallyLinearCurve=true` 已复核为保留的 product extension，S3/S4 不删除、不改 strict FreeCAD parity。

## S2 red fixture / expected boundary

- S2 已决定复用 `c6m3/partdesign-pipe-interpolation-law-subtractive-product`，不新增 C12-M17 fixture；该 case 同时覆盖 LawSamples product PipeLaw、Body replay、main pipe response 和 named-shape response surface。
- C6-M3 subtractive product expected 已从旧 product-contract main-response 差异口径迁移为“LawSamples PipeLaw 是 CAD Core product extension，但 SubtractivePipe main Shape lifecycle 应为 FreeCAD-compatible post-cut feature Shape”。
- Focused P7 test 保留 PipeLaw metadata / LawSamples / product-contract provenance 断言，同时要求 subtractive product main response 的 shape / bbox / volume / mesh / subshapes / namedShape 走 post-cut result，Body replay 和 `replayed_subtractive_features` 继续守住 removed-tool cache 语义。
- S2 intentionally red 已由 S3 关闭：focused test 原失败点是 `object_fields.shape` 实际 `occt_solid`、expected `occt_compound`；S3 关闭后主 response 覆盖不再由 product PipeLaw provenance 决定，PipeLaw product extension 与 axis extension 均保留。

## S3 implementation status

- S3 已关闭：`cad-core/src/part_design/feature_pipe.cpp` 不再让 product PipeLaw provenance 覆盖主响应 shape；`publishedShape` / `publishedNamedShape` 固定使用 post-cut `featureShape` / `featureNamedShape`。
- `context.addSubShapes[pipe].subtractive` 继续保存 pre-boolean `toolShape` / `toolNamedShape`，Body replay guard 通过。
- product PipeLaw mesh / subshapes 仍会构建，但现在从 post-cut `publishedShape` 构建。S3 同步修正 focused test 的 subtractive shape guard，并把 S2 expected 中 feature named-shape 的 base alias 前缀从 Body replay 的 `BasePad.*` 修正为 feature-level `Body.*`。
- S3 focused validation 通过：`cmake --build build` 与 `python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c6m3_partdesign_pipe_interpolation_law_products_publish_contract_metadata`。

## S4 public wording status

- S4 已在 `cad-core/src/runtime/capability_contract.cpp` 和 `cad-core/tests/test_adapters.py` 中新增并锁定 public capability wording：`SubtractivePipe product PipeLaw main Shape lifecycle: FreeCAD-compatible post-cut feature Shape`。
- `docs/capability/7-5-00-14-cad-web-background非FreeCAD原生语义边界.md` 已移除第一类当前非原生差异，把 SubtractivePipe product PipeLaw 主 `Shape` lifecycle 标为已整改，同时保留 PipeLaw `LawSamples` / `Linear` / `S-shape` product extensions。
- PartDesign 几何共线 BSpline / 非 Line 轴引用仍是当前保留的 CAD Core product extension / non-native boundary，不属于 C12-M17 主 `Shape` parity 修复范围。

## S5 release gate status

- S5 已发布最终状态 `implemented_freecad_main_shape_parity_product_law_retained`：主 `Shape` parity 与 PipeLaw product extension 共存。
- 本发布闸门只同步 release docs、root README 和矩阵；未新增生产 C++ 功能、expected、测试面、capability source 或 frontend work。
- S4 已通过 `cmake --build build`、adapter capability test 和 focused P7 test；S5 未触碰代码 / test / expected / capability source，因此不重跑 focused tests。
- `C12M17-BLOCKER-601` 已关闭，blocker queue 无 open row；重开条件只剩未来 focused regression、expected/current mismatch 或明确产品契约冲突。

## FreeCAD source authority

| 语义 | FreeCAD source | C12-M17 用法 |
| --- | --- | --- |
| Pipe tool cache | `src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::execute()` | `AddSubShape.setValue(result.makeElementCompound(...))` 表示 pre-boolean tool cache。 |
| Subtractive boolean | `src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::execute()` | base 存在时执行 `Part::OpCodes::Cut`，feature `Shape` 写入 post-cut 结果。 |
| Downstream add/sub cache | `src/Mod/PartDesign/App/FeatureAddSub.cpp::FeatureAddSub::getAddSubShape()` | downstream consumer 读取 `AddSubShape`，不是从主 `Shape` 反推 removed tool。 |
| Product PipeLaw boundary | `cad-core/src/part_design/feature_pipe.cpp::usesCadCoreProductPipeLaw()` | PipeLaw 采样能力保留为 CAD Core 产品契约，但不改变 FreeCAD 主形状生命周期。 |

## cad-core 落点

| 落点 | 角色 |
| --- | --- |
| `cad-core/src/part_design/feature_pipe.cpp` | 删除或降级 `publishToolContractShape` 对主响应 shape 的覆盖；主 `publishedShape` 固定使用 `featureShape`。 |
| `cad-core/fixtures/c6m3/expected/partdesign-pipe-interpolation-law-subtractive-product.freecad.json` | 从 product-contract oracle 改为记录 product PipeLaw extension + FreeCAD-compatible post-cut main shape。 |
| `cad-core/tests/test_p7_features.py` | 补/改 focused test，断言 product SubtractivePipe 主对象 volume / mesh / namedShape 为 post-cut feature shape，`AddSubShape` 仍保留 removed tool。 |
| `cad-core/src/runtime/capability_contract.cpp` | 保留 PipeLaw product extension wording；新增 `SubtractivePipe` product PipeLaw 主 `Shape` lifecycle 已是 FreeCAD-compatible post-cut feature shape 的 public wording。 |
| `cad-core/tests/test_adapters.py` | 同步 capability / C API surface 断言。 |

## 实现目标

- `SubtractivePipe` product PipeLaw 的主响应 `objects[pipe]`、`mesh[pipe]`、`subshapes[pipe]`、`named_shapes[pipe]`、bbox 和 volume 均来自 post-cut `featureShape`。
- `context.shapes[pipe]` 继续保存 `featureShape`，与主响应一致。
- `context.addSubShapes[pipe].subtractive` 继续保存 pre-boolean `toolShape`，供 Body replay / downstream AddSubShape consumer 使用。
- PipeLaw `Transformation=Interpolation LawSamples` 仍作为 CAD Core product extension 支持；product extension 的 provenance 不改变主 `Shape` lifecycle。
- capability / expected / tests 明确区分：`PipeLaw` 是产品扩展，`SubtractivePipe` 主 `Shape` 生命周期按 FreeCAD parity。

## 非目标

- 不取消 `Transformation=Linear`、`Transformation=S-shape` 或 `Transformation=Interpolation LawSamples` product PipeLaw 支持。
- 不把 PartDesign 几何共线 BSpline / 非 Line 轴引用改成 strict FreeCAD parity；本包保留该产品扩展。
- 不改 Part Sweep helper lifecycle、C12-M14 HelperLifecycle product contract 或 ORACLE-105。
- 不引入 persistent backend document/session/cache。
- 不用 fixture 名、bbox、volume 常量或输出顺序特判修正结果。
- 不处理前端 UI / my-chili3d consumer。

## 入口

- 总入口：`7-5-01-56-C12-M17-SubtractivePipeProductPipeLawShapeParity实现批次总入口.md`
- 方案：`7-5-01-56-C12-M17-SubtractivePipeProductPipeLawShapeParity实现批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M17-SubtractivePipeProductPipeLawShapeParity实现批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M17-SubtractivePipeProductPipeLawShapeParity实现批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M17-SubtractivePipeProductPipeLawShapeParity实现批次 docs/CADCore12.0/README.md
git diff --check
```

实现 focused：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c6m3_partdesign_pipe_interpolation_law_products_publish_contract_metadata
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

阶段收口候选：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests/test_mvp.py
```
