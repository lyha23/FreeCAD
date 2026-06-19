# PARTCONIC-S4 Part 消费者与 surface 裁决

## 目标

证明 Part conic edge 不只是孤立 edge fixture，而能进入至少一个 Part workbench 消费链路。优先验证 `Part::Extrusion` 消费 Hyperbola / Parabola edge；若 FreeCAD/cad-core 语义不允许，写清 blocked reason，不扩大到 RuledSurface / ProjectionOnSurface。

## 必读

- S3 已实现后的 fixture、expected 和测试。
- `src/Mod/Part/App/FeatureExtrusion.cpp`
- `cad-core/src/part/part_extrusion.cpp`
- `cad-core/tests/test_p8_features.py`
- `cad-core/fixtures/p8`
- `矩阵/part_conic_geometry_scope_review_matrix.tsv`
- `矩阵/part_conic_geometry_blocker_queue.tsv`

## 工作内容

1. 设计并实现 `part-conic-edge-extrusion` fixture，覆盖至少一种 conic edge 作为 Base 的 consumer 路径。
2. 如果 Hyperbola 和 Parabola 的 consumer 行为一致，优先两个都测；如果只选一个，矩阵必须说明代表性理由和下一批范围。
3. 验证 output shape label、subshape map、expected parity 和 diagnostics。
4. 裁决 “Part workbench conic surface” 发布口径：
   - 通过 consumer 只发布 edge-to-face/shell 这类已验证能力。
   - 未证明的 surface family 保持 non-goal。

## S4 live 结论

- 新增 `cad-core/fixtures/p8/part-conic-edge-extrusion.json` 与 `expected/part-conic-edge-extrusion.freecad.json`，同一 fixture 覆盖 Hyperbola 与 Parabola conic edge 进入 `Part::Extrusion` 的 consumer 路径。
- cad-core 仍不注册 fake `Part::Hyperbola` / `Part::Parabola`。`partGeometryCurveConsumers` 使用普通 `Part::Extrusion` object properties，先由 PartConicCurveDTO 生成 request-local edge，再用现有 executor 消费 `Base`。
- FreeCAD expected 使用 source-backed oracle：`Part.ArcOfHyperbola/Part.ArcOfParabola(...).toShape().extrude(vector)`；对应 `FeatureExtrusion.cpp::Extrusion::extrudeShape()` regular path `result.makeElementPrism(myShape, vec)`。
- `HyperbolaExtrusion` 与 `ParabolaExtrusion` 均为 `occt_face`，diagnostics 为空；consumer metadata 通过 `source_curve_kind` / `source_curve_type` / `source_part_geometry_type` 追溯 base curve。
- `PARTCONIC-BLOCK-006` 关闭。surface 发布口径只限已验证 edge-to-face `Part::Extrusion` consumer；RuledSurface、ProjectionOnSurface 与完整 surface family 仍保持 non-goal。

## 非目标

- 不实现 RuledSurface / ProjectionOnSurface。
- 不把 consumer 失败改成 output-side 修剪。
- 不用 fixture 名称分支。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures
```

完成后把本文件重命名为 `6-19-17-06-【已实现】PARTCONIC-S4-Part消费者与surface裁决.md`。
