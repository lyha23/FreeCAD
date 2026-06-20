# CADCore5.1 PartDesign 剩余 deferred 语义实现

本包承接 `docs/CADCore5.0-PartDesign-高价值剩余语义` freeze 后用户明确要求重开的剩余项：除 GUI Attachment editor 外，C5 deferred / non-goal 边界全部进入 C5.1 实现计划。

## 入口

- 总入口：`6-20-13-37-CADCore5.1-PartDesign剩余deferred语义总入口.md`
- 一揽子方案：`6-20-13-37-【已实现】CADCore5.1-PartDesign剩余deferred语义一揽子方案.md`
- Freeze 收口总结：`6-20-13-38-【已实现】C51-S6-freeze收口总结.md`
- 范围矩阵：`矩阵/cadcore51_scope_review_matrix.tsv`
- blocker 队列：`矩阵/cadcore51_blocker_queue.tsv`
- oracle / fixture 矩阵：`矩阵/cadcore51_oracle_fixture_matrix.tsv`
- request input contract：`矩阵/cadcore51_input_contract_matrix.tsv`
- non-goal registry：`矩阵/cadcore51_non_goal_registry.tsv`
- 验收矩阵：`矩阵/cadcore51_validation_matrix.tsv`

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.1-PartDesign-剩余deferred语义实现/工作步骤细分 --format markdown
```

## 当前状态

- C51-S0 已把 C5 freeze 剩余边界重开并细分到 package-local 矩阵；`C51-BLK-000` 已关闭。
- C51-S1 已关闭 Revolution / Groove broad deferred：Revolution UpTo、Profile.SubList/InternalFace、FeatureFirst、DatumLine/App::Line/Sketch AxisN 有 checked-in fixture/expected/test/capability；Groove UpTo 保留 exact native BRepFeat blocker `partdesign_groove_upto_brepfeat_cut_native_failure`。
- C51-S2 已关闭 Boolean Compound / Section：Compound 按 Part `TopoShape::makeElementBoolean(Compound)` 产品化为 Body replacement，受 `AllowCompound` 约束；Section 按 Part Section maker 产品化为 standalone edge/wire result，进入 Body Tip 时返回 exact `partdesign_body_tip_non_solid`，无交集返回 `no_intersection`。
- C51-S3 已关闭 Loft advanced：Closed/multi-section 与 multi-wire ordering 的 C5 known-gap expected 已转 active parity，并镜像到 `cad-core/fixtures/c51m3`；MapperThruSections / MapperSewing history 由 topo `NamedShape` / `ElementMap` 路径传播，不走 executor 输出别名修正。
- C51-S4 已关闭 Pipe advanced broad deferred：Fixed/Auxiliary/Binormal、Round corner、selected spine / multisection path、front/back MapperSewing 进入 supported；`Transformation=Linear/S-shape/Interpolation` 与 `SpineTangent` / `AuxiliarySpineTangent` 保留为 FreeCAD 源码注释支撑的 exact blockers，不再是 broad deferred。
- C51-S5 已关闭 Datum AttachEngine broad deferred：首批非 GUI selected modes 覆盖 `FlatFace`、`ObjectXY/ObjectXZ/ObjectYZ`、`ObjectOrigin`、`ObjectX/ObjectY/ObjectZ`、`NormalToEdge`，`AttachmentOffset`、`MapReversed/Reverse`、`MapPathParameter/Parameter` 组合进入 supported；AttachmentSupport shadow-sub writeback 只返回 request-local `documentObjectUpdates` / `elementReferenceUpdates` 建议。剩余未覆盖 AttachEngine modes 是 `datum_attach_engine_remaining_modes` exact blocker，不再是 broad deferred。
- C51-S6 已完成 freeze 收口：S0-S5 父项均在 child rows closed 或 exact blocker 化后关闭，`C51-BLK-601` 关闭，队列应为空。
- Remaining exact blockers：`partdesign_groove_upto_brepfeat_cut_native_failure`、`partdesign_pipe_transformation_laws_source_commented`、`partdesign_pipe_spine_tangent_source_commented`、`datum_attach_engine_remaining_modes`。
- Attachment non-goal 只限 GUI Attachment editor / ViewProvider / TaskPanel / visual resize；跨请求后端 attachment session 仍是全局 non-goal，AttachmentSupport writeback 只能返回 request-local graph update suggestions。
