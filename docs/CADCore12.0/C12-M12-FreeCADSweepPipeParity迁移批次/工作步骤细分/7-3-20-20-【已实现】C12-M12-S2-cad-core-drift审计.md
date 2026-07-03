# 【已实现】C12-M12 S2 cad-core drift 审计

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

## 关闭记录

- live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=ba87b82038`（`ba87b82038 文档：关闭 C12-M12 S1 源权威复核`），baseline status clean。
- 已审计 current landing：`cad-core/src/part_design/feature_pipe.cpp`、`cad-core/src/part/topo_shape_expansion.cpp`、`cad-core/src/part/part_sweep.cpp`、`cad-core/tests/test_p7_features.py`、`cad-core/tests/test_p8_features.py`。
- `c12m12_sweep_source_matrix.tsv` 中 C12M12-SRC-006..010 已标记 reviewed，记录了 PartDesign Pipe executor、shared PipeShell builder、Part Sweep wrapper 与 P7/P8 focused tests 的当前覆盖面。
- `c12m12_sweep_drift_audit.tsv` 中 C12M12-DRIFT-001..008 已归类：现有 Standard/Frenet、cap/sewing、Auxiliary/Binormal、Part Sweep wrapper 控制用例归为 `current_covered`；用户失败样例归为 `needs_oracle`；invalid planar rebuild 只作为 `implementation_candidate_after_s3`；profile relocation 与 mesh-only repair 归为 `product_contract_or_non_goal`。
- `c12m12_sweep_oracle_matrix.tsv` 已写明 S3 优先级：P0 为用户失败最小 fixture，P1/P2 复用现有 c4m2、c5m3、c51m4、c3m4、c4m1、c5m10、c5m12、c6m4 控制用例，P3 只作为 mesh response gate。
- `C12M12-BLOCKER-301` 已关闭，`C12M12-VAL-201` 已记录通过；本步骤未修改 `cad-core/src`、fixtures、expected、tests 或 capability wording。

## 非目标

- 不做代码修复。
- 不删除现有 tests / expected。
- 不改变 capability wording。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n "spine|Frenet|Auxiliary|Binormal|PipeShell|MakePipeShell|Sewing|Simulate" cad-core/src/part_design/feature_pipe.cpp cad-core/src/part/topo_shape_expansion.cpp cad-core/src/part/part_sweep.cpp
```
