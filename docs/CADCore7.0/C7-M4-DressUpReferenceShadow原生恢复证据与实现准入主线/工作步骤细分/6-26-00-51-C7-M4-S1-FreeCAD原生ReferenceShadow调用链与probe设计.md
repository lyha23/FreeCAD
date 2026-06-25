# C7-M4 S1 FreeCAD 原生 ReferenceShadow 调用链与 probe 设计

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
