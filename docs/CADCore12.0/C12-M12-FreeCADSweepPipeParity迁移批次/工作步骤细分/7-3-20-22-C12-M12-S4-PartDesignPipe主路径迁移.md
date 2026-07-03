# C12-M12 S4 PartDesign Pipe 主路径迁移

## 目标

在 S1-S3 均成立后，对 `PartDesign::AdditivePipe` / `SubtractivePipe` 做最小 FreeCAD parity 迁移。

## 必读文件

- `../矩阵/c12m12_sweep_drift_audit.tsv`
- `../矩阵/c12m12_sweep_oracle_matrix.tsv`
- `cad-core/src/part_design/feature_pipe.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/tests/test_p7_features.py`

## 操作

1. 按 S3 red cases 增加或修正 focused tests，确认红灯。
2. 收紧 PartDesign Pipe profile/spine/section link 解析，使其按 FreeCAD source 行为进入 shared PipeShell builder。
3. 删除或隔离 S2 证明的非 FreeCAD 行为：auto profile relocation、invalid planar rebuild、mesh-only repair。
4. 复刻 `Pipe::setupAlgorithm()` 对 mode、transition、auxiliary、binormal、support、law 的调用顺序。
5. 复刻 cap/sewing/solidification 与 Body add/cut history。
6. 更新 expected、diagnostics 与 validation matrix；将本步骤重命名为 `【已实现】`。

## 关闭条件

- S3 指向 PartDesign Pipe 的 mismatch 全部 red-to-green。
- focused tests 通过。
- 未引入 Part Sweep wrapper 行为回归。
- 非 FreeCAD 行为已删除、隔离或记录为 explicit product contract。

## 非目标

- 不改 Part Workbench Sweep wrapper，除非 shared builder 修改导致必须同步。
- 不新增前端字段。
- 不解决完整 Topological Naming。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features
```
