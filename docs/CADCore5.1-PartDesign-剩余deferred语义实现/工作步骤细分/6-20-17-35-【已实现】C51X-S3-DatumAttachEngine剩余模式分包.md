# C51X-S3 Datum AttachEngine 剩余模式分包

## 目标

把 `datum_attach_engine_remaining_modes` 从一个大 exact blocker 拆成可实现的 mode family。每个 family 都要独立 source audit、native expected、input contract、placement/writeback 验证。

## 必读

- `src/Mod/Part/App/Attacher.cpp`
- `src/Mod/Part/App/Attacher.h`
- `src/Mod/Part/App/AttachExtension.cpp`
- `src/App/PropertyLinks.cpp`
- `cad-core/src/part_design/datum_attachment.h`
- `cad-core/fixtures/c51m5`

## 工作内容

- 按 AttachEngine 子类和输入证据拆包，不按 UI 下拉列表一次性实现全部 mode。
- 推荐顺序：curve frame family、two-point/intersection/proximity line family、three-point/folding plane family、curve normal/binormal/tangent family、focus/edge/center/vertex point family。
- 每个 family 先采 native expected，再补 `datum_attachment.h` placement 逻辑。
- AttachmentSupport writeback 仍只基于 request graph 的 `StableSubList`、`ShadowSub`、`ReferenceShadow` 和当前 shape；后端不得保存 session。
- 对无法稳定采集 oracle 的 mode 保持 exact blocker，并在矩阵写清输入缺口。

## 完成记录

- 已实现 DatumPoint 单输入子批次：`Vertex`、`OnEdge`、`CenterOfMass`。
- FreeCAD 依据：`src/Mod/Part/App/Attacher.cpp::AttachEnginePoint::_calculateAttachedPlacement()` 的 `mm0Vertex`、`mm0OnEdge`、`mm0CenterOfMass`。
- cad-core 落点：`cad-core/src/part_design/datum_attachment.h`，新增 point placement、edge parameter point 和 inertial center helpers。
- oracle / fixture：`cad-core/fixtures/c51m5/partdesign-datum-point-single-input-modes.json` 与 `expected/partdesign-datum-point-single-input-modes.freecad.json`。
- collector 兼容性：`cad-core/tools/collect_freecad_expected.py` 对当前 FreeCADCmd 中 datum wrapper 缺少 `getPoint/getDirection/getXAxis` 的情况补 `Placement` fallback。
- 剩余 Focus / Intersection / Proximity / TwoPoint、curve frame、three-point/folding 等 mode family 继续作为 split exact blockers。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.1-PartDesign-剩余deferred语义实现 cad-core
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 完成条件

- 至少一个 mode family 进入 supported，或全部保持 exact blocker 且原因是 native/oracle/input 证据不足。
- 不把 GUI Attachment editor、ViewProvider 或 visual resize 纳入实现范围。
