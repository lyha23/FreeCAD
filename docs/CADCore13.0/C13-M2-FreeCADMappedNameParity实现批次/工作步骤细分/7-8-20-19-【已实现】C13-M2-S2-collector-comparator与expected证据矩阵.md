# 【已实现】C13-M2 S2 collector comparator 与 expected 证据矩阵

## 目标

把 collector expected schema/comparator 与 FreeCAD runtime authority 分开，形成 C13-M2 可测试合同。

## 必读文件

- S1 输出
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/fixtures/p2/expected/rect-pad-pocket.freecad.json`
- `cad-core/fixtures/c4m6/expected/topo-state-body-tip-stable-recovery.freecad.json`
- `cad-core/fixtures/p5/expected/sketch-internal-face.freecad.json`
- `cad-core/fixtures/p6/expected/up-to-face-stable-body-history.freecad.json`
- `cad-core/fixtures/p8/expected/app-link-box-face.freecad.json`

## 操作

1. 记录 `canonical_freecad_mapped_name()`、`topo_state_element_map_entry()`、`comparable_topo_naming_state()` 的合同。
2. 用 focused expected 抽样 `mappedName.raw/canonical`、`childElementMapKey`、`mapperHistoryIds`。
3. 更新 fixture / contract / validation matrix。

## 关闭条件

- C13-M2 必须字段、gap_allowed 字段、后续字段分类明确。
- focused fixtures 的 expected evidence 形态被矩阵记录。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
jq '.topoNamingState.objects | keys' cad-core/fixtures/p2/expected/rect-pad-pocket.freecad.json
git diff --check
```

## 关闭结果

- live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=5a12e7fdc6`（`5a12e7fdc6 docs: 关闭 C13-M2 S1 源码权威冻结`），起点 `git status --short -uall` 无输出。
- collector/comparator 合同已拆清：`canonical_freecad_mapped_name()` 只把 FreeCAD raw mapped name 中的 `:H...` / `;D...` 归一化为可比较 canonical 形态；`topo_state_element_map_entry()` 从 `stableSubname` 或 `rawFreecadMappedName` 构造 `mappedName.raw/canonical` 与固定 evidence schema；`comparable_topo_naming_state()` 对 mapped names、entry keys 和 producer FreeCAD/OCCT 版本做比较归一化。
- 这些是 expected/schema/comparator 合同，不是 runtime 业务 source；runtime source authority 仍以 S1 冻结的 FreeCAD `MappedName`、`ElementMap`、`TopoShapeExpansion` 和 `TopoShapeMapper` 调用链为准。
- focused expected 抽样已分类：`p2/rect-pad-pocket` 只有 `Body`、50 entries；`c4m6/topo-state-body-tip-stable-recovery` 只有 `Body`、26 entries；`p5/sketch-internal-face` 只有 `Sketch`、`indexed_only`、0 entries；`p6/up-to-face-stable-body-history` 只有 `ProbePad`、26 entries；`p8/app-link-box-face` 只有 `BoxLink`、`indexed_only`、0 entries。
- `p2` / `c4m6` / `p6` 有 FreeCAD `mappedName.raw/canonical` examples；`p5` / `p8` 是 no-fake-raw indexed-only 边界。
- 当前五个 focused expected 中 `childElementMapKey` 均为 `null`、`mapperHistoryIds` 均为空数组；它们只属于 schema / future S5 关注，不能在 S2 标绿为 implemented。
- `C13M2-SCOPE-105`、`C13M2-BLOCKER-301`、`C13M2-IMPL-003` 已关闭；`C13M2-SRC-008` 已标为 `comparator_contract_frozen_s2`。
- 本步未修改 C++ / Python runtime，未写 tests，未刷新 `cad-core-res`，未采集 FreeCAD expected，未关闭 S3-S6，未把 key/id 空证据写成 supported。

## S2 验证记录

- `rg -n "canonical_freecad_mapped_name|topo_state_element_map_entry|comparable_topo_naming_state" cad-core/tools/collect_freecad_expected.py` 通过，collector 合同入口可定位。
- `jq '.topoNamingState.objects | keys' cad-core/fixtures/p2/expected/rect-pad-pocket.freecad.json` 输出 `["Body"]`。
- `step_goal_queue.py docs/CADCore13.0/C13-M2-FreeCADMappedNameParity实现批次/工作步骤细分 --format markdown` 在本步关闭后应从 S3 开始。
- TSV 字段数校验和 `git diff --check` 已作为 S2 验收记录写入 validation matrix。
