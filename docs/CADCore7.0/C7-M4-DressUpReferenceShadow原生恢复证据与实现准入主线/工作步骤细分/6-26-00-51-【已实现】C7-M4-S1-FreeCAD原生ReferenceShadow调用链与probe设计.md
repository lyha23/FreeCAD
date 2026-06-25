# 【已实现】C7-M4 S1 FreeCAD 原生 ReferenceShadow 调用链与 probe 设计

## 目标

复核 FreeCAD native `PropertyLinkSub` / `ShadowSub` / `ReferenceShadow` 恢复调用链，设计一个不会绕过 `ShadowSub` / `ReferenceShadow` 的 probe。S1 只写文档和矩阵，不新增 fixtures/expected/tests，不运行 FreeCAD oracle，不改 C++。

## 必读文件

- `src/App/PropertyLinks.h`
- `src/App/PropertyLinks.cpp`
- `src/Mod/PartDesign/App/FeatureDressUp.cpp`
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/fixtures/c3m5/dressup-reference-shadow-base-recovery.json`
- `cad-core/fixtures/c3m5/expected/dressup-reference-shadow-base-recovery.freecad.json`
- `cad-core/tests/test_p6_topology.py`
- `cad-core/tests/test_p7_features.py`
- `docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/6-26-00-49-C7-M4-DressUpReferenceShadow原生恢复证据与实现准入方案.md`
- `docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/矩阵/*.tsv`

## 执行要点

1. 记录 live baseline 和 C7-M4 queue。
2. 记录 FreeCAD source authority：`PropertyXLink::Restore()`、`afterRestore()`、`onContainerRestored()`、`updateElementReference()`、`DressUp::getContinuousEdges()`、`DressUp::onChanged()`、`DressUp::getAddSubShape()`。
3. 复核 collector gap：`link_sub_value()` 是否仍用 `StableSubList` 替代 stale `SubList`。
4. 设计 native probe 输入、执行方式和输出字段。输出必须能区分 native restore 成功与 StableSubList-fed geometry 成功。
5. 更新 `c7m4_reference_shadow_probe_plan.tsv`、`source_authority.tsv`、`blocker_queue.tsv` 和方案 S1 小节。
6. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S2。

## Probe 设计要求

- 不能把 `Base=(Fillet, ["Edge1"])` 当作 native recovery。
- 必须保留 old subname 与 shadow pair：`oldName=OldFilletEdge1`、`newName=Edge1` 或 S1 复核后的等价 pair。
- 必须记录恢复前后 `SubList`、`ShadowSub`、`ReferenceShadow`、`Base.getShadowSubs()` 和 shape summary。
- 若 FreeCAD Python API 无法触发 native restore，应把原因写成 S2 blocker 的前置条件，而不是跳到 C++ 实现。

## S1 完成结论

- 本轮 live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=d2f530072c`（`d2f530072c 文档：完成 C7-M4 S0 基线冻结`），开始状态 `git status --short -uall` 无输出；C7-M4 队列从 S1 起步。
- `src/App/PropertyLinks.h::PropertyLinkBase::ShadowSub` 是 `ElementNamePair`，字段顺序为 `newName` / `oldName`；S2 输出必须显式记录 `newName=Edge1`、`oldName=OldFilletEdge1`。
- `src/App/PropertyLinks.cpp::PropertyXLink::Restore()` 从 `sub` 或 `count/Sub value` 读取 old subname，并读取 `shadowed` / `shadow` 后调用 `setValue(...)`；`PropertyLinkSub::Restore()` 对 `DressUp.Base` 的实际 `LinkSub` tag 使用同样的 `value` / `shadowed` / `shadow` 语义。
- `PropertyXLink` / `PropertyLinkSub` 的 `afterRestore()`、`onContainerRestored()`、`updateElementReference()` 会继续恢复 label reference、注册 element reference，并把 `_SubList` / `_ShadowSubList` 交给 `updateLinkReference()`。
- `src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getContinuousEdges()` 遍历 `Base.getShadowSubs()`，优先 `newName`，否则 `oldName`；`DressUp::onChanged()` 在 BaseFeature 同步时保留 `Base.getSubValues(false)` 与 `Base.getShadowSubs()`；`DressUp::getAddSubShape()` 在 `SupportTransform=true` 时沿 support traversal 跳过连续 DressUp。
- `cad-core/tools/collect_freecad_expected.py::link_sub_value()` 仍是 `list_field(value, "StableSubList", "SubList")`，因此现有 collector 是 StableSubList-fed bypass，只能作为 S2 负控，不能关闭 native oracle blocker。

## S2 probe 方案

1. 以 `cad-core/fixtures/c3m5/dressup-reference-shadow-base-recovery.json` 的对象链为输入语义，构造 `SketchPad -> Pad -> Fillet -> Chamfer -> Body`，生成一个 baseline FCStd；不要在 patch 后执行 `Chamfer.Base = (Fillet, ["Edge1"])`。
2. 在 FCStd 的 `Document.xml` 中定位 `Chamfer.Base` 的实际 property tag。若是 `LinkSub`，把 `Sub value` patch 为 `OldFilletEdge1` 并写入 `shadow="Edge1"`；若实际序列化为 `XLink`，使用等价的 `sub="OldFilletEdge1"`、`shadow="Edge1"`。保留 JSON sidecar 中的 `ReferenceShadow`，只作为旧 subshape 证据记录，不把 BREP 当建模输入。
3. 用 FreeCAD document restore 入口重新打开 patch 后的 FCStd，先记录 recompute 前字段：FreeCAD version / revision、property tag、XML attrs、`Base.getSubValues()`、`Base.getSubValues(false)`、`Base.getSubValues(true)`、`Base.getShadowSubs()`、`SubList`、`ShadowSub`、`ReferenceShadow` sidecar。
4. 触发 `doc.recompute()`，再记录同一组 Base 字段以及 `Chamfer.Shape` / `Body.Shape` 的 shape summary（shape type、bbox、volume、topology counts 或等价字段）。
5. 另外运行现有 `collect_freecad_expected.py` StableSubList-fed 命令作为负控。只有 native restore run 同时满足：XML 仍带 `OldFilletEdge1`、未重新用 Python 赋值 `Edge1`、`Base.getShadowSubs()` 含 `newName=Edge1` / `oldName=OldFilletEdge1`、recompute shape 成功，才可进入 `native_oracle_collected`。若只有负控 geometry 成功，仍是 `native_oracle_blocked`。
6. 若 S2 无法执行或观察 FreeCAD restore/update 生命周期（例如 FreeCADCmd 不可用、FCStd/XML patch 无法 reopen、无法访问 `Base.getShadowSubs()`、无法定位 `Chamfer.Base` property tag），应写 `native_oracle_blocked`；若 restore 可执行但 ShadowSub 为空或新旧引用无法恢复，应写 `native_not_supported`，仍不得打开 C++ gate。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/工作步骤细分 --format markdown
rg -n 'PropertyXLink::Restore|afterRestore|onContainerRestored|updateElementReference|getShadowSubs|ReferenceShadow|StableSubList|ShadowSub|link_sub_value' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线 docs/CADCore7.0/README.md
git diff --check
```

## 完成标准

- S2 有明确可执行的 native probe 或明确的 collector blocker 条件。
- S1 没有修改 C++、fixtures、expected 或 tests。
- 队列推进到 S2。
