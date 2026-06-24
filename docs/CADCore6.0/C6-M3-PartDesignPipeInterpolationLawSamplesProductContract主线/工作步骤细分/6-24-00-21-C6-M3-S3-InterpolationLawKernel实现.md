# C6-M3 S3 InterpolationLawKernel 实现

## 目标

实现或证明低层 Interpolation law kernel。S3 的重点是 `part/topo_shape_expansion` 层的可复用 law 能力，而不是在 `feature_pipe.cpp` 里拼输出。

## 必读输入

- S0-S2 已实现文档
- `cad-core/include/cad_core/part/topo_shape_expansion.h`
- `cad-core/src/part/topo_shape_expansion.cpp`
- OCCT law / PipeShell 相关 include 和现有 Linear / S-shape 实现
- `矩阵/c6m3_pipe_interpolation_law_blocker_queue.tsv`

## 实施内容

1. 扩展 law 数据结构以承接 Interpolation samples。
2. 在低层实现 sampled interpolation law 或记录 OCCT 能力 blocker。
3. 写最小 focused C++/Python test，证明 law 会影响 PipeShell 几何或明确 blocker。
4. 保持 PipeShell maker history / sewing history 不回退。
5. 更新矩阵和 S3 文档，完成后改名为 `6-24-00-21-【已实现】C6-M3-S3-InterpolationLawKernel实现.md`。

## 非目标

- 不在 executor 中硬编码 fixture 输出。
- 不改 adapter。
- 不处理 GUI law editor。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore6.0 cad-core
```
