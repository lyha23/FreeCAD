# 【已实现】C12-M2 S1 FreeCAD 源码与 probe 候选矩阵

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
- `src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp`
- `src/Mod/Part/App/PartFeatures.cpp`
- `src/Mod/Part/App/PartFeatures.h`
- `src/Mod/Part/App/FeatureProjectOnSurface.cpp`
- `src/Mod/Part/App/FeatureProjectOnSurface.h`

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

## S1 执行结论

本步 live baseline：

```text
pwd=/Users/li/Chili3DProject/FreeCAD
HEAD=5f4355f054
git log -1 --oneline=5f4355f054 docs: 冻结 C12-M2 S0 oracle 基线
git -c core.quotepath=false status --short -uall=<clean>
```

已回填 `C12M2-SRC-001..005`：Sweep、Filling、GeomPlate、Loft、ProjectOnSurface 均有 exact FreeCAD source authority、类/函数和关键短句。证据按 `existing expected`、`historical probe output`、`no-code retained` 三类写入 source/probe matrix：

- Sweep：`PartFeatures.cpp::Sweep::execute()` 只作为标准 DocumentObject baseline；`BRepOffsetAPI_MakePipeShellPyImp.cpp::add()` 的 Location overload、`setAuxiliarySpine()`、`setTolerance()` 是 wrapper probe authority。C11-M1 S3 JSON 仍是 `OCCError: NCollection_Array1::Value` / `notCollected` 证据。
- Filling：`AppPartPy.cpp::makeFilledFace()`、`TopoShapeExpansion.cpp::makeElementFilledFace()` 与 `BRepOffsetAPI_MakeFillingPyImp.cpp` 构成 helper/builder/wrapper authority。C11-M2 S3 JSON 只作为 helper lifecycle / diagnostic control / `notCollected` 证据。
- GeomPlate：`BuildPlateSurfacePyImp.cpp` 与 `CurveConstraintPyImp.cpp` 区分 InitialSurface / ProjectedCurve2d / G1 native-hidden / NotImplemented diagnostics；已有 C5/C6 expected 保持历史上下文。
- Loft：`PartFeatures.cpp::Loft::execute()` 只消费 `Sections.getValues()`，`TopoShapeExpansion.cpp::makeElementLoft()` 负责 `BRepOffsetAPI_ThruSections` 与 mapper；selected subelement 仍是 native-hidden retained evidence。
- ProjectOnSurface：`FeatureProjectOnSurface.cpp` 的 Projection item 顺序、project/filter/compound 调用链与 `TopoShapePyImp.cpp::getElementHistory/mapShapes/mapSubElement` 已写入 source authority；mapper/provenance 不从 bbox、输出顺序或 fixture 名推断。

`C12M2-BLOCKER-003` 已关闭为 `closed_s1_none_found`，表示本轮没有缺 source authority 却进入后续 probe 的行。稳定 native expected、request-local 边界和 current mismatch 仍交给 S2-S6；S1 未运行 FreeCADCmd/native probe，未修改 `cad-core/src`、fixtures、expected、tests、adapters 或 capability wording。
