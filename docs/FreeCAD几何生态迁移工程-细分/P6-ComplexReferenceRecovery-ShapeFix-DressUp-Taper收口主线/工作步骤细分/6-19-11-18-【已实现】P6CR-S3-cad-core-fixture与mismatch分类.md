# P6CR-S3 cad-core fixture 与 mismatch 分类【已实现】

## 目标

用 S2 的 FreeCAD expected 对比 cad-core 当前输出，决定是否真的存在 backendGap。

## 分类规则

- `supported`：cad-core 已与 FreeCAD expected 对齐，只需要补测试或文档。
- `backendGap`：FreeCAD expected 清楚、cad-core 输出不一致，并且能定位到 topo / ShapeFix / DressUp / taper / runtime 层。
- `diagnostic_expected`：FreeCAD 也无法唯一恢复，cad-core 稳定 diagnostic 合理。
- `notCollected`：缺正式 expected 或 oracle 环境不可用。
- `nonGoal`：需要 GUI 重选、跨请求完整 BREP 或不符合 CAD Core 无状态边界。

## 输出

- focused tests 草案。
- 更新 scope / blocker / backend 分类。
- 只有 `backendGap` 行才允许进入后续 C++ 实现；本轮 S3 结论是 0 个 backendGap。

## 验收

```bash
cd cad-core
python3 -m unittest tests.test_p6_topology.CadCoreP6TopologyTest
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest -k "dressup or taper"
git diff --check
```

## 完成结论

- `P6CR-CAND-003` 保持 `notCollected`：S2 的 ShapeFix collectorGap 仍成立，没有 checked-in ShapeFix expected，因此不能转换为 backendGap。
- `P6CR-CAND-004` 分类为 `supported`：`cad-core` 当前输出与 `sketch-external-edge-stable-chamfer-refine.freecad.json` 对齐，`ProbeSketch` 1 条外部几何、无 Missing/Detached/Frozen/Sync/Defining 状态、无 diagnostics，内部 shape 为空。
- `P6CR-CAND-006` 分类为 `supported`：`cad-core` 当前输出与 `sketch-external-edge-stable-two-sides-taper.freecad.json` 对齐，`ProbeSketch` 1 条外部几何、无 Missing/Detached/Frozen/Sync/Defining 状态、无 diagnostics，内部 shape 为空。
- 新增 `../矩阵/p6_complex_reference_recovery_backend_mismatch_classification.tsv` 记录 fixture、expected、命令、实际结果、分类、owner layer 与 S4 action。
- `cad-core/tests/test_p6_topology.py` 新增 focused regression，直接覆盖两个 P6CR checked-in expected；本轮没有 C++、adapter 或 expected JSON 变更。
- S4 action：没有 backendGap 行进入产品 C++；S4 只需同步旧 P6 发布/能力文档并关闭队列。

## 本轮验收

```bash
cd cad-core
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest
python3 -m unittest tests.test_p6_topology.CadCoreP6TopologyTest
cd ..
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P6-ComplexReferenceRecovery-ShapeFix-DressUp-Taper收口主线 cad-core/tests cad-core/fixtures/p6
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/FreeCAD几何生态迁移工程-细分/P6-ComplexReferenceRecovery-ShapeFix-DressUp-Taper收口主线/矩阵/*.tsv
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P6-ComplexReferenceRecovery-ShapeFix-DressUp-Taper收口主线/工作步骤细分/6-19-11-19-【已实现】P6CR-S4-实现与发布闸门.md --format markdown
```
