# C51-S5 Datum AttachEngine 与输入驱动写回

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
