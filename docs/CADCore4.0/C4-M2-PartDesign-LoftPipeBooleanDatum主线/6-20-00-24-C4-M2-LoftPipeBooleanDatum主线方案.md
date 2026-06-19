# C4-M2 Loft / Pipe / Boolean / Datum 主线方案

## 目标

把 PartDesign Loft、Pipe、Boolean、Datum family 从 broad feature gap 拆成产品目标内的可审计专题包。先确认哪些语义进入 cad-core，哪些仅保留 reference / placement / diagnostics。

## 范围

- FreeCAD 源码依据：`src/Mod/PartDesign/App/FeatureLoft.cpp`、`FeaturePipe.cpp`、`FeatureBoolean.cpp`、`Datum*.cpp`、`Body.cpp`。
- cad-core 落点：`cad-core/src/part_design`、`cad-core/src/app`、`cad-core/src/part/topo_shape*`、`cad-core/src/adapters/c_api/c_api.cpp`。
- 验收：`tests.test_p7_features`、`tests.test_expected_fixtures`、`tests.test_adapters`。

## 阶段

| 步骤 | 内容 |
| --- | --- |
| S0 | feature family source audit 与产品范围分类 |
| S1 | oracle / fixture / diagnostics 设计 |
| S2 | 第一批实现队列或 explicit deferred 边界 |
| S5A | PartDesign Loft 单族 follow-up：profile / Sections / Body history |
| S5B | PartDesign Pipe 单族 follow-up：Spine / Sections / PipeShell history |
| S5C | Datum Attachment pressure：active AttachmentSupport / MapMode 以稳定 deferred diagnostic 收口 |

## 非目标

- 不迁移 TaskPanel / ViewProvider。
- 不引入跨请求 document session。
- 不把 Datum GUI 编辑行为混入 cad-core；只保留 placement、link、reference 和 recompute 必需语义。

## C4-S5 完成状态

- Supported：`PartDesign::Boolean` 的 `Type=Fuse/Cut/Common`、`Group` Body tool、`BaseFeature`、Body replacement Tip replay、single-solid diagnostic 和 `maker_history:boolean`，对应 `cad-core/fixtures/c4m2/partdesign-boolean-{cut,fuse,common}-body-tool.json` 及 native expected。
- Supported first slice：`PartDesign::AdditiveLoft` / `PartDesign::SubtractiveLoft` 的 full-profile Sketch + one Sketch section Body replay，覆盖 `Profile`、`Sections`、`Ruled=false`、`Closed=false`、sewing/solidification、`AddSubShape` 和 Body fuse/cut history，对应 `cad-core/fixtures/c4m2/partdesign-loft-{additive,subtractive}-body.json` 及 native expected。
- Supported first slice：`PartDesign::AdditivePipe` / `PartDesign::SubtractivePipe` 的 full-profile Sketch + one Body-scope Sketch spine Body replay，覆盖 `Profile`、`Spine`、`Mode=Standard`、`Transformation=Constant`、`Transition=Transformed`、PipeShell maker history、`AddSubShape` 和 Body fuse/cut history，对应 `cad-core/fixtures/c4m2/partdesign-pipe-{additive,subtractive}-body.json` 及 native expected。
- Supported existing：DatumPoint / DatumLine / DatumPlane / DatumCS placement、DatumLine / DatumCS downstream references、Body Origin role relink，复用既有 `p7` / `c3m5` fixture 证据；S5C 增加 active Datum AttachmentSupport / MapMode deferred diagnostic，避免 overclaim AttachEngine solver。
- Deferred：PartDesign Loft 的显式 subelement selection、multi-section `Closed=true`、multi-wire ordering、AllowCompound / multi-solid fuse 和完整 sewing MapperHistory 传播仍需后续 expected / diagnostic owner；PartDesign Pipe 的 `Sections`、AuxiliarySpine、Binormal、scaling law、完整 `Transformation`、非默认 `Transition`、SpineTangent 和完整 sewing MapperHistory 传播仍需后续 expected / diagnostic owner；完整 Datum AttachEngine map-mode solver 仍需后续产品 owner。
- Non-goal：GUI Attachment editor、ViewProvider / TaskPanel、Boolean LinkStage3-only Compound / Section 类型、跨请求 attachment session。
