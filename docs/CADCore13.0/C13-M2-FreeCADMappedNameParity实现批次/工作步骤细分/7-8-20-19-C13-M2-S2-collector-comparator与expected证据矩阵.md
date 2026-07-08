# C13-M2 S2 collector comparator 与 expected 证据矩阵

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
