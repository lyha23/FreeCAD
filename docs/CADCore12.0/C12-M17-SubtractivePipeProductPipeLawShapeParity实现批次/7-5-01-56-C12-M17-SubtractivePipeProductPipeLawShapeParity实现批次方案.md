# C12-M17 SubtractivePipe product PipeLaw Shape parity 实现批次方案

## 目标

修正 `SubtractivePipe` product PipeLaw 的主形状发布语义：主响应必须发布 FreeCAD post-cut feature `Shape`，同时保持 PipeLaw 采样能力和几何共线 BSpline 轴引用为 CAD Core 产品扩展。

## 决策

1. `SubtractivePipe product PipeLaw publishes toolShape` 是待修复偏差。
2. `PartDesign axis accepts geometrically linear BSpline / non-Line curves` 是用户选择保留的产品扩展，本包不改为 strict FreeCAD parity。
3. Product PipeLaw 的正确边界是：Transformation / LawSamples 是 CAD Core product extension；feature `Shape` lifecycle 仍按 FreeCAD `Pipe::execute()`。

## S0 live 基线与偏差边界冻结

冻结当前 `HEAD`、dirty boundary、C12-M16 队列状态、capability 非原生语义文档中的两类偏差，以及用户对两类偏差的选择：

- 修复 `SubtractivePipe product PipeLaw` 主 `Shape`。
- 保留几何共线 BSpline / 非 Line 轴引用。

S0 不改 C++、fixtures、expected、tests 或 capability source。

S0 已按 live repo 状态关闭：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=474097e0f6`（`474097e0f6 feat: 补齐引用轴和基准平面契约`）。C12-M1..M16 队列均为空，C12-M17 从 S0 推进到 S1；起点 dirty boundary 记录在 S0 step 文件中，后续实现改动不得混入既有 C12-M11 / M15 / M16 / capability dirty docs。

## S1 FreeCAD source 与 current publish 路径复核

复核并记录：

- `src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::execute()` 中 `AddSubShape.setValue(...)`、`Part::OpCodes::Cut`、`Shape.setValue(getSolid(boolOp))` 的顺序。
- `src/Mod/PartDesign/App/FeatureAddSub.cpp::FeatureAddSub::getAddSubShape()` 对 downstream tool cache 的消费语义。
- `cad-core/src/part_design/feature_pipe.cpp::executePipeFeature()` 中 `featureShape`、`toolShape`、`featureNamedShape`、`toolNamedShape` 和 `publishToolContractShape` 的发布路径。
- C6-M3 product PipeLaw fixture / expected / focused tests 在 S1 当时是否明确约束旧主响应差异。
- PartDesign axis current support 的落点和保留边界，避免 S3 误删产品扩展。

S1 关闭条件是 source authority、current landing 和 red path 都可定位。

S1 已按 live source 关闭：`Pipe::execute()` 的 `AddSubShape.setValue(...)` 是 pre-boolean tool cache，Body base 存在时随后执行 `Part::OpCodes::Fuse` / `Part::OpCodes::Cut`，`rawShape` 与 `Shape.setValue(...)` 写入 post-boolean feature result；`FeatureAddSub::getAddSubShape()` downstream 继续消费 `AddSubShape` 作为 add/sub tool cache。S1 当时定位到 product PipeLaw 主 response 红路径；C6-M3 expected/test 当时锁定旧主响应差异，capability/adapters 仅公开 PipeLaw product extension 和 fixture 列表。该红路径已由 S3 关闭，S4 只保留 product PipeLaw extension 口径。PartDesign `allowGeometricallyLinearCurve=true` 作为 product extension 已复核保留。

## S2 red fixture 与 expected 迁移设计

锁定 focused red/green 面：

- 复用或扩展 `cad-core/fixtures/c6m3/partdesign-pipe-interpolation-law-subtractive-product.json`。
- 将 expected 从旧 product-contract main-response 差异口径迁移为“product PipeLaw extension + FreeCAD-compatible post-cut main Shape”。
- focused test 必须同时断言：
  - `objects[SubtractivePipeInterpolationProduct]` 主 volume / bbox / mesh / subshapes / namedShape 来自 post-cut feature shape。
  - Body final shape 仍通过 subtractive replay。
  - AddSubShape / replayed subtractive features 仍保留 removed tool cache。
  - PipeLaw metadata / LawSamples 仍存在，product extension 未被删除。

如果无法从现有 expected 推出 post-cut 主 shape，应先用同一 FreeCAD / LibPack / OCCT baseline 采集或复用 C12-M13 lifecycle oracle，而不是按 volume 常量硬编码。

S2 已关闭为 `red_expected_locked_reuse_c6m3`：复用 C6-M3 subtractive product fixture，不新增 C12-M17 fixture；expected wording 改为“LawSamples PipeLaw 是 CAD Core product extension，但 SubtractivePipe main Shape lifecycle 应为 FreeCAD-compatible post-cut feature Shape”；focused P7 test 当前故意 red，失败点为 `object_fields.shape` 实际 `occt_solid`、expected `occt_compound`。S3 负责让该 red path green。

## S3 product PipeLaw 主 Shape 实现

允许修改：

- `cad-core/src/part_design/feature_pipe.cpp`
- 必要时补 `cad-core/include` 中已有 public 结构注释，但不新增跨请求状态。

实现原则：

- `publishedShape` 和 `publishedNamedShape` 默认使用 `featureShape` / `featureNamedShape`。
- 不再让 `publishToolContractShape` 覆盖主 response surface。
- `toolShape` 继续进入 `context.addSubShapes[object.name].subtractive`。
- 若需要发布 product-only removed tool preview，必须使用明确 product-only 字段或后续单独契约，不得塞回主 `Shape` / mesh / namedShape。
- 不因 LawSamples product extension 改变 `Shape` lifecycle。

S3 已关闭：`executePipeFeature()` 主 `publishedShape` / `publishedNamedShape` 固定发布 post-cut `featureShape` / `featureNamedShape`；product PipeLaw 强制 display topology 仍保留，但 mesh / subshapes 从 post-cut `publishedShape` 构建；`context.addSubShapes` 的 subtractive slot 继续保存 removed tool cache。Focused P7 test 已 green，且 S3 记录了 S2 expected 中 feature named-shape base alias 前缀应为 `Body.*`、不是 Body replay 的 `BasePad.*`。

## S4 capability / expected / adapter 口径同步

同步公开口径：

- `cad-core/src/runtime/capability_contract.cpp` 保留 product PipeLaw supported wording，并明确 subtractive product Law 主 `Shape` lifecycle 是 post-cut feature `Shape`。
- `cad-core/tests/test_adapters.py` 同步 capability assertions。
- C6-M3 expected wording 已由 S2 改成 product PipeLaw extension + main-shape parity；S4 只同步 capability / adapter public wording。
- `docs/capability/...非FreeCAD原生语义边界.md` 在 S4 直接移除第一类当前偏差，把 `SubtractivePipe` product PipeLaw 主 `Shape` lifecycle 记录为已整改。
- PartDesign axis extension 必须继续标为 product extension，不因 C12-M17 发布被误删或改写为 native parity。

S4 已关闭：capability contract 和 adapter test 新增 public wording `SubtractivePipe product PipeLaw main Shape lifecycle: FreeCAD-compatible post-cut feature Shape`；C6-M3 subtractive product fixture 继续作为 product PipeLaw fixture；`docs/capability` 当前非原生边界只保留 PartDesign 几何共线 BSpline / 非 Line 轴引用产品扩展。

## S5 发布闸门

发布前必须确认：

- C12-M17 队列关闭后只输出表头。
- focused P7 / adapter tests 通过。
- `SubtractivePipe product PipeLaw` 主 response 与 `context.shapes` 均为 post-cut feature shape。
- `AddSubShape` removed tool cache 和 Body replay 未回归。
- capability / expected / docs 明确：PipeLaw 是产品扩展，主 `Shape` lifecycle 是 FreeCAD parity。
- axis product extension 保留，且不被列为 C12-M17 blocker。

S5 已关闭为 `implemented_freecad_main_shape_parity_product_law_retained`：S3 已让主 product SubtractivePipe response、mesh、subshapes、bbox、volume 和 namedShape 跟随 post-cut `featureShape` / `featureNamedShape`；`AddSubShape` subtractive cache 仍保存 pre-boolean `toolShape` / `toolNamedShape`；S4 已同步 capability、adapter、expected wording 和 capability 边界文档。PipeLaw `LawSamples` / `Linear` / `S-shape` 仍是 CAD Core product extensions，几何共线 BSpline / 非 Line PartDesign axis 仍是保留的 product extension / non-native boundary。

S5 仅同步 package README、方案、总入口、root README、blocker / implementation / validation / contract / scope / non-goal 矩阵和本 step 记录；未改生产 C++、fixtures、expected、tests、capability source 或 frontend。S4 已通过 `cmake --build build`、adapter capability test 和 focused P7 test，S5 因未触碰代码面不重跑 focused tests。C12-M17 队列关闭后只应输出 markdown 表头，`C12M17-BLOCKER-601` 已关闭且 blocker queue 无 open row。

## 最小验证命令

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M17-SubtractivePipeProductPipeLawShapeParity实现批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M17-SubtractivePipeProductPipeLawShapeParity实现批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M17-SubtractivePipeProductPipeLawShapeParity实现批次 docs/CADCore12.0/README.md
git diff --check
```

代码实现后 focused 验证候选：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c6m3_partdesign_pipe_interpolation_law_products_publish_contract_metadata
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

普通步骤不要求全量 FreeCAD build 或全量 CI；只有 S5 发布或 touched shared runtime surface 时再考虑 `python3 -m unittest tests/test_mvp.py`。
