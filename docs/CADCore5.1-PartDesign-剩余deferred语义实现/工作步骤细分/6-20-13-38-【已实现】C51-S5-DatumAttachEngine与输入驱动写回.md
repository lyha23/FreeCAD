# 【已实现】C51-S5 Datum AttachEngine 与输入驱动写回

## 目标

实现非 GUI Datum AttachEngine selected map modes，并通过 request input 的 `StableSubList`、`ShadowSub`、`ReferenceShadow`、旧 subname 和当前 graph，实现 AttachmentSupport shadow-sub writeback 建议。

## 必读

- `src/Mod/Part/App/AttachExtension.cpp`
- `src/Mod/Part/App/Attacher.cpp`
- `src/Mod/Part/App/Attacher.h`
- `src/App/PropertyLinks.cpp`
- `src/Mod/PartDesign/App/DatumPoint.cpp`
- `src/Mod/PartDesign/App/DatumLine.cpp`
- `src/Mod/PartDesign/App/DatumPlane.cpp`
- `src/Mod/PartDesign/App/DatumCS.cpp`
- `docs/CADCore5.1-PartDesign-剩余deferred语义实现/矩阵/cadcore51_input_contract_matrix.tsv`

## 工作内容

- 先关闭矩阵 child blockers：`C51-BLK-511` 3D/Plane selected modes、`C51-BLK-512` Line/Point selected modes、`C51-BLK-513` offset/reverse/path parameter、`C51-BLK-514` input-driven writeback；对应 oracle 为 `C51-ORC-511`..`514`，validation 为 `C51-VAL-511`..`514`。
- 选择首批非 GUI map modes：建议从 `FlatFace`、`ObjectXY/ObjectXZ/ObjectYZ`、`ObjectOrigin/ObjectX/ObjectY/ObjectZ`、`NormalToEdge` 开始，若风险过大可在 S5 内拆批。
- 在 `cad-core` 建 request-local AttachEngine：输入只来自 graph property，不读取 GUI/editor/session。
- 实现 AttachmentOffset、MapReversed、MapPathParameter 的组合语义。
- 将 `AttachmentSupport` link evidence 用于 subshape recovery，并通过 `documentObjectUpdates` / `elementReferenceUpdates` 返回写回建议。
- 更新 C API capability schema 和 adapters tests，明确 GUI editor 仍 non-goal。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.1-PartDesign-剩余deferred语义实现 cad-core
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 完成条件

- selected map modes 有 native expected-backed placement。
- AttachmentSupport shadow-sub writeback 不依赖后端状态；只依赖 request input evidence，并返回 graph update suggestions。
- downstream attached Datum 引用不再只稳定跳过。

## 实施记录

- FreeCAD 调用链：`AttachExtension.cpp::positionBySupport()` 读取 `AttachmentSupport`、`MapMode`、`AttachmentOffset`、`MapReversed`、`MapPathParameter`，调用 `AttachEngine::calculateAttachedPlacement(..., &subChanged)`；若 `subChanged`，通过 `AttachmentSupport.setValues(... getSubValues())` 写回。`Attacher.cpp` 中 `AttachEngine3D` 覆盖 `FlatFace/ObjectXY/ObjectXZ/ObjectYZ/NormalToEdge`，`AttachEngineLine` 将 `ObjectX/ObjectY/ObjectZ` remap 到 `ObjectYZ/ObjectXZ/ObjectXY`，`AttachEnginePoint` 将 `ObjectOrigin` remap 到 `ObjectXY`。`PropertyLinks.cpp::_updateElementReference()` 与 `PropertyLinkSubList::setValues()` 是 old/new shadow sub 和下游引用更新依据。
- cad-core 落点：`cad-core/src/part_design/datum_attachment.h` 实现 request-local selected AttachEngine placement、StableSubList/ShadowSub subname recovery 和 `documentObjectUpdates` 建议；`datum_plane.cpp`、`datum_line.cpp`、`datum_point.cpp`、`datum_coordinate_system.cpp` 消费计算 placement 并更新本轮 `context.globalPlacements`；`tools/collect_freecad_expected.py` 补 Datum payload expected 采集；`c_api.cpp` 同步 capability schema。
- 覆盖范围：`cad-core/fixtures/c51m5/partdesign-datum-selected-mapmodes.json` 覆盖 `FlatFace`、`ObjectXY/ObjectXZ/ObjectYZ`、`ObjectOrigin`、`ObjectX/ObjectY/ObjectZ`、`NormalToEdge`；`partdesign-datum-offset-reverse-writeback.json` 覆盖 `AttachmentOffset`、`MapReversed/Reverse`、`MapPathParameter/Parameter` 与 input-driven AttachmentSupport writeback。
- 写回边界：只基于本次 request graph 的 `AttachmentSupport`、`SubList`、`StableSubList`、`ShadowSub`、`ReferenceShadow`、旧 subname 和当前 shape；`ReferenceShadow.brep` 仍只能作为单 subshape recovery evidence；后端不保存 session/cache，输出为 `documentObjectUpdates` / `elementReferenceUpdates` 建议。
- exact blocker：剩余未覆盖 map modes 归入 `datum_attach_engine_remaining_modes`，包括 Translate、TangentPlane、Frenet/Concentric、ThreePoints/Folding、Directrix/Asymptote、Normal/Binormal/TangentU/V、TwoPoint/Intersection/Proximity、Focus/OnEdge/Center/Vertex 等 mode-specific 分支；不再保留 broad AttachEngine deferred。GUI Attachment editor / ViewProvider / TaskPanel / visual resize 仍是 `C51-NG-002` non-goal。
