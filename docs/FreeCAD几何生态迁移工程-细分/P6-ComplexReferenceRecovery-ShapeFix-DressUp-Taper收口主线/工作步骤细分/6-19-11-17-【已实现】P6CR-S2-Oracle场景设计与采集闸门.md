# P6CR-S2 Oracle 场景设计与采集闸门【已实现】

## 目标

把 S1 候选变成 checked-in FreeCAD expected，或明确记录为什么当前无法采集。

## 实施要求

- 优先复用 `cad-core/tools/collect_freecad_expected.py` 和现有 `fixtures/p6` / `fixtures/c3m5` / `fixtures/p3b` 路线。
- collector 不支持时先补 collector / probe；不要跳到 C++。
- expected 必须记录旧引用、当前引用、diagnostic / recovery / deleted / split 结论。
- 若 FreeCADCmd / OCCT 环境不匹配，只能记录为环境阻塞或兼容性探测，不能替代正式 expected。

## 输出

- 新增或更新 fixture / expected 列表。
- 回写 blocker queue：每类 producer 转 `supported`、`backendGap`、`diagnostic_expected`、`nonGoal` 或继续 `notCollected`。

## 验收

```bash
cd cad-core
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd python3 tools/collect_freecad_expected.py <fixture-json> --check
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest
```

## 完成结论

- S2 只做 oracle / collector 裁决，没有修改 C++、adapter 或 collector。
- `P6CR-CAND-003` ShapeFix/ReShape 关闭为 `collectorGap`：`FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/c3m1/shapefix-delete-small-edge.json --check` 可启动 FreeCADCmd，但 collector 返回 `fixture has no Objects`。当前 c3m1 ShapeFix case 是 simple schema，collector 没有 native ShapeFix / `removeSmallEdges` DocumentObject，也没有能把旧 `Source.Edge1` + `StableSubList` 接到 `BRepTools_ReShape` 删除历史后的 observer route；因此不写 expected，不进入 backendGap。
- `P6CR-CAND-004` DressUp/Refine 已采集：新增 `cad-core/fixtures/p6/sketch-external-edge-stable-chamfer-refine.json` 和 `cad-core/fixtures/p6/expected/sketch-external-edge-stable-chamfer-refine.freecad.json`。FreeCADCmd `--check` 通过；expected 记录 `ProbeSketch` 有 1 条外部几何，`Missing/Detached/Frozen/Sync` 计数均为 0。
- `P6CR-CAND-006` two-sided taper 已采集：新增 `cad-core/fixtures/p6/sketch-external-edge-stable-two-sides-taper.json` 和 `cad-core/fixtures/p6/expected/sketch-external-edge-stable-two-sides-taper.freecad.json`。FreeCADCmd `--check` 通过；expected 记录 `ProbeSketch` 有 1 条外部几何，`Missing/Detached/Frozen/Sync` 计数均为 0。
- S2 同步新增 `矩阵/p6_complex_reference_recovery_oracle_collection.tsv`，并回写 `source_candidates`、`scope_review_matrix`、`blocker_queue` 中 `012/022/032` 与 `002..004` 状态。
- S3 只消费已采集的 DressUp/Refine 与 two-sided taper expected 做 cad-core mismatch 分类；ShapeFix 必须先补 collector / probe 才能进入 mismatch 分类。

## 本轮验收

```bash
cd cad-core
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/p6/sketch-external-edge-stable-chamfer-refine.json --check
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/p6/sketch-external-edge-stable-two-sides-taper.json --check
cd ..
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P6-ComplexReferenceRecovery-ShapeFix-DressUp-Taper收口主线 cad-core/fixtures/p6
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/FreeCAD几何生态迁移工程-细分/P6-ComplexReferenceRecovery-ShapeFix-DressUp-Taper收口主线/矩阵/*.tsv
```
