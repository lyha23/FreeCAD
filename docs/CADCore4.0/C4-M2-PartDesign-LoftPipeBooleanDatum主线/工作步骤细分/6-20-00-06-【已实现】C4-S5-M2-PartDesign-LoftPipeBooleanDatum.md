# 【已实现】C4-S5 M2 PartDesign Loft / Pipe / Boolean / Datum

## 目标

把 PartDesign Loft、Pipe、Boolean、Datum / Attachment 的 4.0 目标范围拆成可实现批次。只纳入前端 graph/recompute/topo naming 所需语义。

## 必读文件

- `docs/CADCore4.0/C4-M2-PartDesignFeatureFamily总览/6-19-23-55-C4-M2PartDesignFeatureFamily补完方案.md`
- `docs/CADCore4.0/矩阵/cadcore4_source_candidates.tsv`
- `src/Mod/PartDesign/App/FeatureLoft.cpp`
- `src/Mod/PartDesign/App/FeaturePipe.cpp`
- `src/Mod/PartDesign/App/FeatureBoolean.cpp`
- `src/Mod/PartDesign/App/Body.cpp`
- `src/Mod/PartDesign/App/Datum*.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `cad-core/src/part_design`

## 产物

- C4-M2 fixture/oracle rows for Loft / Pipe / Boolean / Datum。
- 必要时拆出后续单 family 包，不在一个步骤里强行全实现。
- Diagnostics：unsupported selection、missing target、invalid placement、unsupported parameter。
- Capability metadata 同步 remaining boundary。

## 非目标

- 不把所有 PartDesign feature 设为 4.0 blocker。
- 不迁移 GUI-only Attachment editor。
- 不在 adapter 层做 Body / topo 修正。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 完成口径

每个 family 进入 supported/deferred/non-goal 之一；supported 项有 FreeCAD source、fixture、Body chain、topo history 和 focused test 证据。

## 完成记录

- Supported：`PartDesign::Boolean` 的 `Type=Fuse/Cut/Common`、`Group` Body tool、`BaseFeature`、Body replacement Tip replay、single-solid diagnostic 和 `maker_history:boolean`。新增 `cad-core/src/part_design/feature_boolean.*`、registry、collector、capability metadata、`partdesign-boolean-{cut,fuse,common}-body-tool` native expected 和 `partdesign-boolean-deferred-diagnostics`。
- Supported existing：DatumPoint / DatumLine / DatumPlane / DatumCS placement、DatumLine / DatumCS downstream references、Body Origin role relink。已有 `p7/datum-coordinate-system-reference-axis`、`p7/datum-coordinate-system-sketch-support`、`c3m5/body-origin-link-placement` 证据；本步只同步矩阵和 capability 状态。
- Follow-up closure：PartDesign Loft 已由 `6-20-00-14-【已实现】C4-S5A-M2-PartDesign-Loft单族主线.md` 收口为 expected-backed first slice；PartDesign Pipe 已由 `6-20-00-15-【已实现】C4-S5B-M2-PartDesign-Pipe单族主线.md` 收口为 expected-backed first slice；Datum Attachment pressure 仍拆到 `6-20-00-16-C4-S5C-M2-DatumAttachment压力.md`，仅在产品需要非 GUI AttachEngine 压力时执行。
- Non-goal：GUI Attachment editor、ViewProvider / TaskPanel、Boolean LinkStage3-only Compound / Section 类型、跨请求 attachment session。不在 adapter 层做 Body / topo 修正。
