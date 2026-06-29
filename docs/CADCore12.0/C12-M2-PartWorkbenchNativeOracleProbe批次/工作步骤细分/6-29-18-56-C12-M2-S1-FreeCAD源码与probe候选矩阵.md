# C12-M2 S1 FreeCAD 源码与 probe 候选矩阵

## 目标

为 Sweep、Filling、GeomPlate、Loft、ProjectOnSurface 建立 source authority 与历史证据索引。S1 只确认“可以去问 FreeCAD 哪个行为”，不判断 current cad-core 是否要改。

## 必读输入

- C12-M1 S5 / S6 已实现文件。
- CADCore5/6/11 中对应 family 的 S6 / release gate 文件。
- `cad-core/fixtures/c5m7`、`c5m8`、`c5m9`、`c5m10`、`c5m12`、`c5m13`、`c6m4`、`c6m5`、`c6m6`、`c6m7` 的相关 input/expected。
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/tools/probe_filling_s1_contract.py`
- `docs/temp/6-29-10-15-c11m1-s3-sweep-location-combined-probe-output.json`
- `docs/temp/6-29-12-27-c11m2-s3-filling-native-helper-probe-output.json`

## FreeCAD source 起点

- `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp`
- `src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp`
- `src/Mod/Part/App/AppPartPy.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `src/Mod/Part/App/TopoShapePyImp.cpp`

## 执行步骤

1. 对每个 family 写明 FreeCAD 源文件、类/函数、关键参数或短句，不接受“参考 FreeCAD”这种模糊依据。
2. 把已有 fixture expected、probe output、diagnostics 和 release gate 结论归入 source candidate matrix。
3. 区分三类证据：已存在 expected、历史 probe output、只有 no-code retained 结论。
4. 若某行没有 source authority，写入 blocker queue，不进入后续 probe。
5. 不运行采集、不修改 expected、不比较 current cad-core。

## 更新目标

- `矩阵/c12m2_partworkbench_native_oracle_source_candidates.tsv`
- `矩阵/c12m2_partworkbench_native_oracle_probe_matrix.tsv`
- `矩阵/c12m2_partworkbench_native_oracle_blocker_queue.tsv`
- 必要时更新 README 的 source 基线摘要。

## 验收命令

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次 docs/CADCore12.0/README.md
git diff --check
```

## 完成条件

每个 family 至少有一条 source candidate row；无 source authority 的行必须有 blocker，而不是继续进入 implementation 假设。
