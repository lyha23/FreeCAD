# C12-M2 S5 Filling / GeomPlate / ProjectOnSurface Helper Mapper Probe 复审

## 目标

对 helper / wrapper / mapper 证据更重的 Filling、GeomPlate、ProjectOnSurface 执行 native probe 复审。S5 的重点是把 helper lifecycle 和 probe-only 证据拆清楚，不能把 helper 失败直接写成 cad-core backend gap。

## Filling 重点

- `AppPartPy.cpp::makeFilledFace()` 与 `BRepOffsetAPI_MakeFillingPyImp.cpp` 参数、support order、initial surface。
- C11-M2 retained 结论中的 helper blocker 必须单独分类。
- 只有稳定 native expected 加 current mismatch 才能写 `oracle_expected_ready`。

## GeomPlate 重点

- `TopoShapeExpansion.cpp` 中 projected curve2d、initial surface、G1 curve-on-surface 等行。
- 区分已有 expected、probe-only retained evidence 和 native wrapper 不可见。
- 不把 projection/wrapper crash 写成几何语义 mismatch。

## ProjectOnSurface 重点

- mapper / provenance / split trace 是否能作为 request-local expected 表达。
- 如果语义依赖内部 MapperHistory / native object 生命周期而无法采成稳定 artifact，记录 blocker。
- 不用输出排序或单 fixture bbox 倒推 source ownership。

## 执行步骤

1. 按 S3 schema 执行或设计三个 family 的 native probe。
2. 回写 artifact path、版本、输入摘要、输出摘要、失败分类和 close condition。
3. 对 stable expected 行建立 current cad-core comparison path；只在 mismatch 稳定时写 `oracle_expected_ready`。
4. 对 helper lifecycle、native-hidden、probe-only evidence 写 blocker，并明确下一步是否需要更窄 native probe 包。
5. 更新 non-goal registry，固化不进入 CAD Core 产品边界的行为。

## 更新目标

- `矩阵/c12m2_partworkbench_native_oracle_probe_matrix.tsv`
- `矩阵/c12m2_partworkbench_native_oracle_backend_gap_classification.tsv`
- `矩阵/c12m2_partworkbench_native_oracle_blocker_queue.tsv`
- `矩阵/c12m2_partworkbench_native_oracle_non_goal_registry.tsv`
- 必要时新增 S5 probe artifact。

## 验收命令

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次 docs/CADCore12.0/README.md
git diff --check
```

## 完成条件

Filling、GeomPlate、ProjectOnSurface 必须分别有稳定 expected、current-covered、明确 blocker 或 non-goal 结论；不得保留“历史上看起来不稳定所以应该实现”的空泛判断。

## 完成记录

- Filling：`helper_blocked`，artifact 为 `docs/temp/6-29-20-40-c12m2-s5-filling-helper-native-probe-output.json`。direct wrapper 与 simple helper boundary control 稳定，但 helper initial-surface / support-order / explicit params 仍 crash 或 timeout，S6 不比较。
- GeomPlate：projected curve2d + initial surface 为 `oracle_expected_ready`，artifact 为 `docs/temp/6-29-20-40-c12m2-s5-geomplate-native-probe-output.json`。G1 curve-on-surface 保持 `native_hidden`，no-initial-surface curve2d 不进入比较。
- ProjectOnSurface：mapper/provenance 为 `native_hidden`，artifact 为 `docs/temp/6-29-20-40-c12m2-s5-project-on-surface-native-probe-output.json`。native geometry 可 build，但 source-backed history/provenance 未暴露，禁止用输出顺序、bbox 或 fixture 名倒推 ownership。
- S5 验收已通过：队列刷新后只剩 S6，TSV field count 通过，trailing whitespace 无匹配，`git diff --check` 通过。
