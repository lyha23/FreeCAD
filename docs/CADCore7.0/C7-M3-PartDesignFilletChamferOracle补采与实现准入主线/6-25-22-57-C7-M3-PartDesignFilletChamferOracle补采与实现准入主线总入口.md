# C7-M3 PartDesign Fillet Chamfer Oracle 补采与实现准入主线总入口

## 结论

C7-M3 的下一步是补采 oracle，不是直接实现。C7-M2 release gate 已把旧 P7 残余口径拆清：3 个 rows 缺 FreeCAD expected，因此不能发布 supported，也不能证明 active backend gap。C7-M3 必须先采 FreeCAD oracle，再用当前 `cad-core` 跑同族 parity，最后才决定是否进入 C++ 实现。

## 上游状态

- C7-M2 队列为空，最终提交为 `d678462e20 文档：完成 C7-M2 S5 发布闸门`。
- C7-M2 没有 `backend_gap_requires_implementation`，未改 C++、fixtures、expected、tests、topo/history 或 adapter schema。
- S0 已冻结当前 live 基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=d678462e20`；开始状态只有目标文档改动（root README modified、C7-M3 包 untracked），没有无关源码、fixture、expected 或 test 改动。
- C7-M1/C7-M2 队列为空，C7-M3 队列在 S0 后推进到 S1。
- C7-M2 留下的 3 个 oracle pending rows：
  - `C7M2-GAP-101 -> C7M3-SCOPE-101`：Fillet multi-edge / `UseAllEdges`。
  - `C7M2-GAP-203 -> C7M3-SCOPE-102`：Chamfer `FlipDirection=true`。
  - `C7M2-GAP-301 -> C7M3-SCOPE-103`：DressUp chain stale `ReferenceShadow` / Base recovery。

## FreeCAD 调用链

- Fillet：`src/Mod/PartDesign/App/FeatureFillet.cpp::Fillet::execute()` 读取 `Radius` / `UseAllEdges`，`UseAllEdges=true` 时取 `getSubTopoShapes(TopAbs_EDGE)`，否则走 `DressUp::getContinuousEdges(baseShape)`，最后调用 `shape.makeElementFillet(baseShape, edges, Radius, Radius)`。
- Chamfer：`src/Mod/PartDesign/App/FeatureChamfer.cpp::Chamfer::execute()` 读取 `ChamferType` / `Size` / `Size2` / `Angle` / `FlipDirection` / `UseAllEdges`，再调用 `shape.makeElementChamfer(...)`；`migrateFlippedProperties()` 会调整旧文件 `FlipDirection`。
- DressUp recovery：`src/Mod/PartDesign/App/FeatureDressUp.cpp::getBaseObject()`、`getContinuousEdges()`、`getAddSubShape()` 共同决定 Base LinkSub、连续边、AddSubShape cache 和 SupportTransform source base。

## cad-core 落点

- `cad-core/tools/collect_freecad_expected.py`：oracle 采集入口，S2 必须复核能否覆盖本包新 fixtures。
- `cad-core/fixtures/p7/`：C7-M3 新 fixture 优先落这里；expected 必须来自 FreeCAD oracle。
- `cad-core/src/part_design/feature_fillet.cpp`：若 Fillet oracle 证明 active gap，S4 才能修改。
- `cad-core/src/part_design/feature_chamfer.cpp`：若 FlipDirection=true oracle 证明 active gap，S4 才能修改。
- `cad-core/src/part_design/feature_dress_up.cpp` 与 `cad-core/src/topo/`：若 stale `ReferenceShadow` recovery 证明 active gap，必须走正式 reference/topo/history 能力，不允许输出端猜测。

## 步骤队列

1. S0【已实现】：冻结 C7-M3 live baseline 与 C7-M2 oracle pending rows。
2. S1：设计 oracle fixtures 与 FreeCAD collector route。
3. S2：采集 FreeCAD oracle 或记录 native oracle blocker。
4. S3：运行 cad-core parity 并裁决 implementation gate。
5. S4：按 S3 gate 实现或 no-code publication closure。
6. S5：release gate，清空队列并同步 README / 矩阵。

## 验收入口

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线 docs/CADCore7.0/README.md
git diff --check
```
