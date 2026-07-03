# C12-M12 S2 cad-core drift 审计

## 目标

对照 S1 source authority，找出当前 `cad-core` 与 FreeCAD Sweep / Pipe 的真实差异，并把差异归属到 S3 oracle、S4 PartDesign Pipe 或 S5 Part Sweep wrapper。

## 必读文件

- `../矩阵/c12m12_sweep_source_matrix.tsv`
- `../矩阵/c12m12_sweep_drift_audit.tsv`
- `cad-core/src/part_design/feature_pipe.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/src/part/part_sweep.cpp`
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_p8_features.py`

## 操作

1. 审计 PartDesign Pipe executor：profile/spine/section link、Body replay、add/cut、diagnostics。
2. 审计 shared PipeShell builder：mode、transition、tolerance、law、auxiliary、binormal、spine support、solid/sewing。
3. 审计 Part Sweep wrapper：section list、solid/frenet/transition、advanced DTO 与 response history。
4. 标记非 FreeCAD 行为候选，例如 auto relocation、invalid planar rebuild、mesh-only repair。
5. 对每个 drift row 写入 owner step、required oracle、close condition。
6. 将本步骤重命名为 `【已实现】`。

## 关闭条件

- drift matrix 每行都有 `status`、`owner_step`、`required_evidence` 和 `close_condition`。
- S3 需要采集的 native/current fixture 列表明确。
- 没有把 mesh response 问题误判成 BRep parity 结论。

## 非目标

- 不做代码修复。
- 不删除现有 tests / expected。
- 不改变 capability wording。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n "spine|Frenet|Auxiliary|Binormal|PipeShell|MakePipeShell|Sewing|Simulate" cad-core/src/part_design/feature_pipe.cpp cad-core/src/part/topo_shape_expansion.cpp cad-core/src/part/part_sweep.cpp
```
