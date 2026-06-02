# M6 Sketch InternalShape 与 ReferenceShadow 边界

## 目标

让 Sketch 层只保留 FreeCAD 调用顺序，不再承担 WireJoiner result-wire 推理、几何 history 合成或 ReferenceShadow 泛化恢复。

M6 是上层边界守护：底层 M1-M4 没完成时，Sketch 层也不能补规则替它们过 fixture。

## FreeCAD 依据

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::buildInternals()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::getInternalElementMap()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::makeShapeWithElementMap()`

ReferenceShadow 边界依据是仓库已批准规则：`ReferenceShadow.brep` 只能保存被引用单个 subshape 的旧几何快照，用作引用恢复证据，不能作为建模输入或完整对象 BREP 状态。

## cad-core 落点

- `cad-core/src/geometry/sketch_internal_builder.cpp`
- `cad-core/src/features/sketch_object.cpp`
- `cad-core/src/features/feature_extrude.cpp`
- `cad-core/src/topo/named_shape.cpp`
- `cad-core/src/topo/reference_matcher.cpp`

## 当前基线

已完成：

- `SketchInternalBuilder` 不再调用会改写 `wireInfo` 的后置 probe。
- `copiedResultWireGraphProbeForSketchInternals()` 命名路径已删除。
- `resultWireEvidence_` 与 evidence 追加旁路已删除。
- Profile 选择只在 open splitter 产生多个 InternalFace 时才要求 `Profile.SubList`。
- ReferenceShadow 恢复已收窄到 `topo::recoverReferenceShadowSubshape()` / `referenceShadowMatchesCurrentSubshape()`。
- ReferenceShadow 成功恢复时会输出 `reference_recovery = "reference_shadow_single_subshape"`。

仍需守护：

- `SketchInternalBuilder` 不能因为 M2/M3 未完成，重新复制 bounded face edge。
- `SketchObject` 不能按 T/cross/overlap/dangling 这类 fixture 形态补 subshape。
- ReferenceShadow 不能替代 FaceMaker / WireJoiner / MapperHistory。

## 目标流程

```text
FaceMakerBuildFace result
  -> WireJoiner.addShape(raw sketch edges)
  -> WireJoiner.getOpenWires(openWires, "SKF")
  -> result.makeElementCompound({result, openWires})
  -> NamedShape consumes FaceMakerHistory + WireJoinerHistory
```

## 必收切片

1. `SketchInternalBuilder` 只串联 FaceMaker result 与 WireJoiner open wires。
2. `SketchObject` 只负责属性语义、InternalShape metadata 和 response context。
3. Profile / ExternalGeometry 先走 `NamedShape` / `ElementMap`。
4. ReferenceShadow 只在 ElementMap 无法恢复时作为单 subshape 证据。
5. ReferenceShadow 失败必须给出 ambiguous / deleted / type mismatch 等结构化诊断。

## 边界

M6 不负责：

- WireJoiner owner 生命周期。属于 M1。
- openWireCompound 导出。属于 M2。
- generated result-wire identity。属于 M3。
- topo history consumer。属于 M4。

## 非目标

- 不在 `SketchInternalBuilder` 中按 partial overlap、T/cross、cycle、endpoint touch 判断 result-wire。
- 不在 `SketchObject` 中按 fixture 名称修 subshape。
- 不把 ReferenceShadow 扩展成完整 BREP cache 或建模输入。

## 验收

完成条件：

- `cad-core/src/geometry/sketch_internal_builder.cpp` 不出现 result-wire ownership 推理。
- `cad-core/src/features/sketch_object.cpp` 只透传 WireJoiner / FaceMaker history，不发明 history。
- ReferenceShadow 只保存和匹配单 subshape。
- 所有 ReferenceShadow 调用点都是 ElementMap-first。

建议检查：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD
rg "partial overlap|T-junction|cross|cycle|endpoint|ReferenceShadow" cad-core/src/geometry/sketch_internal_builder.cpp cad-core/src/features/sketch_object.cpp cad-core/src/features/feature_extrude.cpp cad-core/src/topo
```
