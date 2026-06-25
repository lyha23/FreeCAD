# 【已实现】C7-M3 S1 oracle fixture 设计与 collector 路径

## 目标

把 3 个 oracle pending rows 设计成可采集的 FreeCAD fixtures，并确认 `collect_freecad_expected.py` 的支持路径、预期输出字段、focused tests 和 blocker 分类。S1 仍然不新增 fixtures/expected/tests，不改 C++。

## live baseline

- `pwd=/Users/li/Chili3DProject/FreeCAD`
- `HEAD=a0a9799608`（`a0a9799608 文档：完成 C7-M3 S0 基线冻结`）
- 开始时 `git -c core.quotepath=false status --short -uall` 无输出。
- S1 只更新本包文档和矩阵；不新增 fixture JSON、expected JSON、tests，不运行 FreeCAD oracle。

## 必读

- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/fixtures/p7/`
- `cad-core/fixtures/c3m5/`
- `cad-core/tests/test_p7_features.py`
- `src/Mod/PartDesign/App/FeatureFillet.cpp`
- `src/Mod/PartDesign/App/FeatureChamfer.cpp`
- `src/Mod/PartDesign/App/FeatureDressUp.cpp`
- 本包 `README.md`、方案和 `矩阵/*.tsv`

## 动作

1. 已记录 live baseline 和队列状态。
2. 已为 Fillet multi-edge / `UseAllEdges` 设计 fixture 名、Base LinkSub、expected fields 和 collector command。
3. 已为 Chamfer `FlipDirection=true` 设计 fixture 名、参数组合和 expected fields；Two distances / Distance and Angle true-side 在 supported publication 前需要代表。
4. 已为 stale `ReferenceShadow` / Base recovery 设计最小 graph；但当前 collector 只会把 `StableSubList` 喂给 FreeCAD `PropertyLinkSub`，不会原生验证 `ShadowSub` / `ReferenceShadow`，因此 S2 必须补 collector 证据或记录 blocker。
5. 已更新 `oracle_plan`、`backend_gate`、`blocker_queue`、`source_authority`、`validation_matrix`。
6. 本文件标题标记为 `【已实现】`，文件名也随提交改名，队列推进到 S2。

## fixture 设计

### Fillet multi-edge selected EdgeN

- fixture：`cad-core/fixtures/p7/fillet-pad-multi-edge.json`
- Base LinkSub：`{"PropertyType":"App::PropertyLinkSub","value":"Pad","SubList":["Edge1","Edge2"]}`
- 参数：沿用 `fillet-pad-edge` 的 SketchPad/Pad/Body；`Radius=0.35`，`UseAllEdges=false`。
- FreeCAD 调用链：`FeatureFillet.cpp::Fillet::execute()` 读取 `UseAllEdges=false` 后走 `DressUp::getContinuousEdges(baseShape)`，再调用 `shape.makeElementFillet(baseShape, edges, Radius, Radius)`。
- expected 字段：`schema_version`、`reference`、`freecad_version`、`objects.Fillet.shape_summary.{bbox,volume,topology_counts}`、`objects.Body.shape_summary.{bbox,volume,topology_counts}`。
- collector command：
  ```bash
  cd /Users/li/Chili3DProject/FreeCAD/cad-core
  FREECADCMD=/path/to/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/p7/fillet-pad-multi-edge.json --out fixtures/p7/expected/fillet-pad-multi-edge.freecad.json
  ```

### Fillet UseAllEdges=true

- fixture：`cad-core/fixtures/p7/fillet-pad-use-all-edges.json`
- Base LinkSub：`{"PropertyType":"App::PropertyLinkSub","value":"Pad","SubList":["Edge1"]}`，用于证明 `UseAllEdges=true` 会覆盖 Base selection。
- 参数：沿用 `fillet-pad-edge` 的 SketchPad/Pad/Body；`Radius=0.2`，`UseAllEdges=true`。
- FreeCAD 调用链：`FeatureFillet.cpp::Fillet::execute()` 在 `UseAllEdges=true` 时直接取 `baseShape.getSubTopoShapes(TopAbs_EDGE)`。
- expected 字段：同 multi-edge，目标为 `objects.Fillet` 和 `objects.Body`。
- collector command：
  ```bash
  cd /Users/li/Chili3DProject/FreeCAD/cad-core
  FREECADCMD=/path/to/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/p7/fillet-pad-use-all-edges.json --out fixtures/p7/expected/fillet-pad-use-all-edges.freecad.json
  ```

### Chamfer FlipDirection=true

- Equal distance fixture：`cad-core/fixtures/p7/chamfer-pad-edge-flip-true.json`
- Base LinkSub：`{"PropertyType":"App::PropertyLinkSub","value":"Pad","SubList":["Edge1"]}`
- 参数：`ChamferType="Equal distance"`，`Size=0.5`，`UseAllEdges=false`，`FlipDirection=true`。
- expected 字段：`schema_version`、`reference`、`freecad_version`、`objects.Chamfer.shape_summary.{bbox,volume,topology_counts}`、`objects.Body.shape_summary.{bbox,volume,topology_counts}`。
- collector command：
  ```bash
  cd /Users/li/Chili3DProject/FreeCAD/cad-core
  FREECADCMD=/path/to/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/p7/chamfer-pad-edge-flip-true.json --out fixtures/p7/expected/chamfer-pad-edge-flip-true.freecad.json
  ```
- Two distances / Distance and Angle：需要 true-side 代表才能发布非等距 `FlipDirection=true` 支持。Equal distance 是最小 true flag smoke，但几何上可能对称，不能单独证明 `Size2` 或 `Angle` 被放到 FreeCAD true side。
- variant fixtures：
  - `cad-core/fixtures/c3m5/chamfer-two-distances-edge-flip-true.json`：基于 `chamfer-two-distances-edge`，`FlipDirection=true`。
  - `cad-core/fixtures/c3m5/chamfer-distance-angle-edge-flip-true.json`：基于 `chamfer-distance-angle-edge`，`FlipDirection=true`。
- variant collector commands：
  ```bash
  cd /Users/li/Chili3DProject/FreeCAD/cad-core
  FREECADCMD=/path/to/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/c3m5/chamfer-two-distances-edge-flip-true.json --out fixtures/c3m5/expected/chamfer-two-distances-edge-flip-true.freecad.json
  FREECADCMD=/path/to/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/c3m5/chamfer-distance-angle-edge-flip-true.json --out fixtures/c3m5/expected/chamfer-distance-angle-edge-flip-true.freecad.json
  ```

### DressUp stale ReferenceShadow/Base recovery

- fixture：`cad-core/fixtures/c3m5/dressup-reference-shadow-base-recovery.json`
- current graph：基于 `chained-dressup-pattern-history` 的最小 Body graph：`SketchPad -> Pad -> Fillet -> Chamfer -> Body`，`Chamfer.Base.value="Fillet"`，Body Tip 为 `Chamfer`。
- Base LinkSub 计划：
  ```json
  {
    "PropertyType": "App::PropertyLinkSub",
    "value": "Fillet",
    "SubList": ["OldFilletEdge1"],
    "StableSubList": ["Edge1"],
    "ShadowSub": [{"newName": "Edge1", "oldName": "OldFilletEdge1"}],
    "ReferenceShadow": [{
      "target": "Fillet",
      "targetId": 3,
      "property": "Shape",
      "shapeType": "Edge",
      "indexed": "OldFilletEdge1",
      "subname": "OldFilletEdge1",
      "stableSubname": "Edge1",
      "fingerprint": {}
    }]
  }
  ```
- 关系：`SubList` 是用户可见旧名；`StableSubList` 是要恢复到 current graph 中 `Fillet` 的稳定边；`ShadowSub` 是 FreeCAD `newName/oldName` 对；`ReferenceShadow` 是旧单 subshape 证据，S2 必须补 fingerprint 或 BREP 证据，不能仅靠空 fingerprint 当成功恢复。
- collector route：当前 `collect_freecad_expected.py` 的 `link_sub_value()` 只使用 `StableSubList` / `SubList`，会忽略 `ShadowSub` 和 `ReferenceShadow`。因此以下命令只能作为 geometry collection 路径；S2 若无法补完整 reference evidence，应把本 row 记为 collector blocker / `oracle_blocked`，不得宽松 fallback：
  ```bash
  cd /Users/li/Chili3DProject/FreeCAD/cad-core
  FREECADCMD=/path/to/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/c3m5/dressup-reference-shadow-base-recovery.json --out fixtures/c3m5/expected/dressup-reference-shadow-base-recovery.freecad.json
  ```
- expected 字段：若 collector 补齐，至少需要 `objects.Chamfer.shape_summary`、`objects.Body.shape_summary`，并在 expected 或 companion evidence 中记录 `Base` 的 recovered `SubList`、`StableSubList`、`ShadowSub`、`ReferenceShadow` 更新；若做不到，S2 blocker route，不能进入 S3 supported 裁决。

## 非目标

- 不实际新增 fixture/expected。
- 不运行 FreeCAD oracle 采集。
- 不跑 cad-core parity。
- 不裁决 backend gap。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/工作步骤细分 --format markdown
rg -n 'collect_freecad_expected|UseAllEdges|FlipDirection|ReferenceShadow|StableSubList|ShadowSub|FreeCADCmd' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线 cad-core/tools/collect_freecad_expected.py cad-core/tests/test_p7_features.py
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线 docs/CADCore7.0/README.md
git diff --check
```

## 通过条件

- 每个 row 都有 fixture 设计、collector path、expected 字段和 blocker route。
- ReferenceShadow recovery 设计没有宽松 fallback。
- 队列推进到 S2。
