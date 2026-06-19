# C4-M2 PartDesign Feature Family 补完方案

## 目标

C4-M2 面向前端 CAD 运行时常用 PartDesign 建模链，把 C3.0 已有代表路径之外的 feature family 做源码审计、oracle 队列和必要实现补齐。

本阶段不追求完整 FreeCAD PartDesign Workbench，也不迁移 GUI-only 行为。只迁移能进入前端 `DocumentObject graph`、参与 recompute、产生 shape / topo naming / element reference update 的核心语义。

## 当前基线

C3.0 已覆盖：

- Pad / Pocket 主链、taper history、`UpToShape` 多面 LinkSubList。
- Body chain：Tip / BaseFeature / Group / Origin / Datum relink / AddSub replay。
- transformed / pattern history。
- DressUp 第一批：Fillet / Chamfer / Draft / Thickness 参数变体和 chain support。
- Hole 第一批：thread table、head cut、ModelThread、profile source mapper history 和 cut history。

## 候选缺口

这些能力必须先经过 C4-M0 审计，确认前端产品需要后再进入实现：

| family | 需要确认的语义 | FreeCAD 入口 |
| --- | --- | --- |
| Revolution / Groove | sketch profile 到旋转实体 / 切除、axis source、angle / reversed / symmetric、history | `src/Mod/PartDesign/App/FeatureRevolution.cpp`、`FeatureGroove.cpp` |
| PartDesign Loft / Pipe | profile list、spine / section、solid / additive / subtractive、Body Tip replacement | `FeatureLoft.cpp`、`FeaturePipe.cpp` |
| Boolean | Body / BaseFeature / multi-body shape selection、fuse / cut / common history | `FeatureBoolean.cpp`、`Body.cpp` |
| Datum / Origin 扩展 | datum plane / line / point placement、attachment、role relink 与 downstream feature selection | `Datum*.cpp`、`Body.cpp`、Attachment 相关源码 |
| DressUp 扩展 | Fillet / Chamfer / Draft / Thickness 更复杂选择和失败边界 | `FeatureDressUp.cpp`、`FeatureFillet.cpp`、`FeatureChamfer.cpp`、`FeatureDraft.cpp`、`FeatureThickness.cpp` |

## 实施原则

- 先确认 feature 是否在前端运行时目标内；低频或 GUI-only 行为只记录 non-goal。
- 每个 family 至少覆盖成功路径、失败 diagnostic、Body Tip replacement、source subname / ElementMap / MapperHistory。
- 不在 PartDesign executor 中绕过 `topo_shape` / `MapperHistory`，也不在 adapter 中修输出。
- 对已有 C3.0 covered family，只补真实新增语义，不重做已冻结 fixture。

## 推荐批次

| 批次 | 内容 | 验收重点 |
| --- | --- | --- |
| C4-M2-S1 | Revolution / Groove oracle-first | axis / angle / direction / additive-subtractive / history |
| C4-M2-S2 | PartDesign Loft / Pipe | multi-profile / spine / Body Tip replacement / maker history |
| C4-M2-S3 | Boolean family | source ownership、multi-body diagnostics、terminal history |
| C4-M2-S4 | Datum / Attachment 扩展 | placement、role relink、downstream reference stability |
| C4-M2-S5 | DressUp extension | complex selection、failure diagnostics、chain history |

## 非目标

- 不实现 PartDesign GUI 参数面板。
- 不迁移不进入 request graph 的编辑状态。
- 不把 FreeCAD 所有 PartDesign feature 一次性标成 4.0 必做；必须经 C4-M0 审计。

## 验收

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore4.0 cad-core
```

阶段回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 可执行包入口

- Revolution / Groove：`docs/CADCore4.0/C4-M2-PartDesign-RevolutionGroove审计主线/工作步骤细分/6-20-00-05-【已实现】C4-S4-M2-PartDesign-Revolution-Groove审计与oracle.md`
- Loft / Pipe / Boolean / Datum：`docs/CADCore4.0/C4-M2-PartDesign-LoftPipeBooleanDatum主线/工作步骤细分/6-20-00-06-【已实现】C4-S5-M2-PartDesign-LoftPipeBooleanDatum.md`；Loft / Pipe / Datum pressure 后续见同目录 S5A-S5C。
- source 矩阵：`docs/CADCore4.0/矩阵/cadcore4_source_candidates.tsv`
