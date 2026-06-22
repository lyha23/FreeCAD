# 【已实现】C51X exact blocker 后续 freeze 收口总结

## 结论

C51X 后续包已关闭：C51 freeze 后的 exact blockers 已逐项归类，未恢复旧 C5 broad deferred。Groove UpTo 与 Pipe law / tangent 继续保持 exact blocker；Datum AttachEngine 已支持 expected-backed selected DatumPoint、DatumLine line-family、C5-M14 DatumPoint ProximityPoint 和 C5-M15 Datum3DPlane 子批次。

## Supported

- DatumPoint `Vertex`、`OnEdge`、`CenterOfMass`、`ProximityPoint1`、`ProximityPoint2` selected MapMode。
- FreeCAD 依据：`src/Mod/Part/App/Attacher.cpp::AttachEnginePoint::_calculateAttachedPlacement()` 中 `mm0Vertex`、`mm0OnEdge`、`mm0CenterOfMass`；`src/Mod/Part/App/Attacher.cpp::AttachEngineLine::_calculateAttachedPlacement()` 中 `mm1TwoPoints`、`mm1Intersection`、`mm1Proximity`。
- cad-core 落点：`cad-core/src/part_design/datum_attachment.h`。
- fixture / expected：`cad-core/fixtures/c51m5/partdesign-datum-point-single-input-modes.json` 与 expected；`cad-core/fixtures/c51m5/partdesign-datum-point-proximity-modes.json`、diagnostic fixture 与 expected；`cad-core/fixtures/c51m5/partdesign-datum-line-family-modes.json`、diagnostic fixture 与对应 expected。
- capability：`part_design.datum_attachment.status=supported_c51x_selected_attach_engine_with_datum_line_family`。

## Still Exact Blockers

- `partdesign_groove_upto_brepfeat_cut_native_failure`：当前 FreeCADCmd 1.2.0 revision 20260519 对 Groove UpToFirst / UpToFace 仍报 `Groove: Revolution: Up to face: Could not revolve the sketch!`。
- `partdesign_pipe_transformation_laws_source_commented`：`FeaturePipe.cpp::Pipe::execute()` 中 Linear / S-shape / Interpolation law branch 仍为 source-commented blocker。
- `partdesign_pipe_spine_tangent_source_commented`：`FeaturePipe.cpp::Pipe::buildPipePath()` 中 continuous edge expansion 仍缺 ledger，保持 source-backed blocker。
- `datum_attach_engine_remaining_modes`：已移除 `Vertex`、`OnEdge`、`CenterOfMass`、`ProximityPoint1`、`ProximityPoint2`、`TwoPointLine`、`IntersectionLine`、`ProximityLine`、`Translate`、`TangentPlane`、`ThreePointsPlane`、`ThreePointsNormal`；剩余 `IntersectionPoint`、Focus、curve frame/curvature、Folding、conic landmarks 等 mode family 后续按独立 oracle/input contract 分包。

## Synced Artifacts

- Docs / matrices：README、主线总入口、实现方案、`cadcore51_followup_{scope_review_matrix,blocker_queue,oracle_fixture_matrix,validation_matrix}.tsv`。
- Code：`cad-core/src/part_design/datum_attachment.h`、`cad-core/tools/collect_freecad_expected.py`。
- Tests：`cad-core/tests/test_p7_features.py`、`cad-core/tests/test_adapters.py`。

## 验收

- 本轮短跑：`git diff --check -- docs/CADCore5.1-PartDesign-剩余deferred语义实现 cad-core`。
- 矩阵检查：`awk -F '\t'` followup TSV 列数一致性。
- 阶段回归：`cd cad-core && cmake --build build && python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters`。
