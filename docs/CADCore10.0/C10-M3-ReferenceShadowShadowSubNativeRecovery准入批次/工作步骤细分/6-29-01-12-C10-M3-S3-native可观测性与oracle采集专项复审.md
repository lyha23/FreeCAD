# C10-M3-S3 native 可观测性与 oracle 采集专项复审

## 目标

判断 FreeCAD 原生在 FCStd / XML restore、ElementMap version update、link reference update 后，是否能稳定暴露 `ShadowSub`、`ReferenceShadow` 或 `getSubValues(false/true)` old/new recovery evidence。S3 只证明 native 可观测性；没有可观测证据时必须保持 `notCollected` / `diagnostic_retained`，不能进入 C++ 实现。

## FreeCAD 依据

| 语义 | 路径 | 必查符号 |
| --- | --- | --- |
| ShadowSub lifecycle | `src/App/PropertyLinks.cpp` | `PropertyLinkBase::updateElementReferences()`、`_updateElementReference()`、`PropertyLinkSub::afterRestore()` |
| old/new style sub values | `src/App/PropertyLinks.cpp` | `PropertyLinkSub::getSubValues(bool)`、`PropertyLinkSubList::getSubValues(bool)` |
| element resolve | `src/App/GeoFeature.cpp` | `GeoFeature::resolveElement()` |
| shape restore | `src/Mod/Part/App/PropertyTopoShape.cpp` | `PropertyPartShape::afterRestore()` |
| mapper history | `src/Mod/Part/App/TopoShape.cpp`、`TopoShapeMapper.cpp` | `makeShapeWithElementMap`、`MapperHistory` |

## 复审场景

| scope | 场景 | 预期输出 |
| --- | --- | --- |
| `C10M3-SCOPE-101` | FCStd / XML restore 后读取 link property 的 `SubList`、`ShadowSub`、`ReferenceShadow` | 证明字段是否 native observable；若不可观察，写 `notCollected`。 |
| `C10M3-SCOPE-102` | 对同一 link property 调用 `getSubValues(false)` 与 `getSubValues(true)` | 证明 old/new style subname 是否可通过 API 区分。 |
| `C10M3-SCOPE-101` | ElementMap version 改变后触发 reference update | 证明 oldName / newName recovery 是否有单目标 evidence。 |
| `C10M3-SCOPE-102` | split / deleted / merge negative control | 证明 FreeCAD 是否只给 diagnostic，避免把不唯一恢复写成 supported。 |

## 必须回写的矩阵行

- `C10M3-SCOPE-101`：关闭为 `native_observable`、`notCollected` 或 `diagnostic_retained`。
- `C10M3-SCOPE-102`：关闭为 `native_observable`、`notCollected` 或 `diagnostic_retained`。
- `C10M3-BLOCKER-301`：S3 完成后改为 `closed_s3` 或带 evidence 的 next blocker。
- `C10M3-CAT-101`：按 native evidence 改为 `native_observable`、`notCollected` 或 `diagnostic_retained`。
- 如果新增 collector / expected，必须记录采集 FreeCAD / LibPack / OCCT 基线。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n "updateElementReferences|_updateElementReference|getSubValues|afterRestore|resolveElement|makeShapeWithElementMap|MapperHistory" src/App/PropertyLinks.cpp src/App/GeoFeature.cpp src/Mod/Part/App/PropertyTopoShape.cpp src/Mod/Part/App/TopoShape.cpp src/Mod/Part/App/TopoShapeMapper.cpp
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore10.0/C10-M3-ReferenceShadowShadowSubNativeRecovery准入批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M3-ReferenceShadowShadowSubNativeRecovery准入批次/矩阵/*.tsv
git diff --check
```

若创建 native collector 或 fixture，则补充运行：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
FREECADCMD=${FREECADCMD:-FreeCADCmd} python3 tools/collect_reference_shadow_recovery.py --output fixtures/c10m3/native-reference-shadow-recovery.freecad.json
```

S3 验收通过后，必须在文件中记录 native probe 结果、环境限制或不可观察结论；S3 文件才能重命名为 `6-29-01-12-【已实现】C10-M3-S3-native可观测性与oracle采集专项复审.md`。

## 非目标

- 不比较 current cad-core，除非只是记录后续 S4 输入。
- 不修改 `cad-core/src` recovery 逻辑。
- 不用 geometry similarity、raw `FaceN`、bbox、面积或输出排序替代 native evidence。
- 不把 FreeCADCmd sandbox / Qt 启动失败当作 FreeCAD 行为失败；需要写明环境限制。
