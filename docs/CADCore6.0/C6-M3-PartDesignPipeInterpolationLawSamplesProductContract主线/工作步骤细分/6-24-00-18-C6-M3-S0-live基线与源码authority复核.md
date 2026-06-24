# C6-M3 S0 live 基线与源码 authority 复核

## 目标

复核 C6-M3 开始前的 live 基线、C6-M1/C6-M2 关闭状态、current Interpolation diagnostic boundary、FreeCAD enum-only source authority 和 capability remaining gap。S0 不改代码、不改 fixture。

## 必读输入

- `docs/CADCore6.0/README.md`
- C6-M1 / C6-M2 已实现总入口和 S6 文档
- `cad-core/src/part_design/feature_pipe.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_adapters.py`
- `src/Mod/PartDesign/App/FeaturePipe.cpp`

## 实施内容

1. 记录 `pwd`、HEAD、last commit、工作区状态。
2. 复跑 C6-M1 / C6-M2 队列，确认均为空。
3. grep current Interpolation boundary：`product_contract_required`、`LawSamples`、capability remaining gap。
4. 读取 FreeCAD `FeaturePipe.cpp`，记录 Interpolation 只有 enum、Linear / S-shape 注释 law、没有 Interpolation 执行分支。
5. 更新 source candidates / scope review / blocker queue 的 S0 baseline 行。
6. 完成后将本文件改名为 `6-24-00-18-【已实现】C6-M3-S0-live基线与源码authority复核.md`。

## 非目标

- 不定义 LawSamples 最终 schema，留给 S1。
- 不改 C++。
- 不新增 fixture。
- 不修改 C6-M1/C6-M2 关闭结论。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'Interpolation|LawSamples|product_contract_required|remaining_gaps' cad-core/src/part_design/feature_pipe.cpp cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_p7_features.py
for f in docs/CADCore6.0/C6-M3-PartDesignPipeInterpolationLawSamplesProductContract主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
git diff --check -- docs/CADCore6.0
```
