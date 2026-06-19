# C4-S9 M4 Topo / Reference 压力实现与发布

## 目标

根据 C4-S8 压力矩阵补 fixture、diagnostics、ElementMap / MapperHistory / reference update contract，确保长期编辑链路稳定。

## 完成状态

S9 已按 `topo_reference_pressure_matrix.tsv` 落地 `C4M4-TR-PRESS-001..012`。12 个 fixture 均使用矩阵中的既定 fixture 名和分类字段；没有新增 split ownership 规则，也没有在 adapter 中修正 topo naming。

## 必读文件

- `docs/CADCore4.0/C4-M4-ReferenceRecovery-TopoNamingPressure主线/工作步骤细分/6-20-00-09-【已实现】C4-S8-M4-TopoReference压力矩阵.md`
- `docs/CADCore4.0/C4-M4-ReferenceRecovery-TopoNamingPressure主线/矩阵/topo_reference_pressure_matrix.tsv`
- `docs/CADCore4.0/矩阵/cadcore4_fixture_oracle_matrix.tsv`
- `cad-core/src/app`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/src/part/topo_shape_mapper.cpp`
- `cad-core/src/runtime/recompute.cpp`
- `cad-core/tests/test_p6_topology.py`
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_p8_features.py`

## 产物

- `cad-core/fixtures/c4m4/topo-reference-pressure-*.json`。
- Focused tests and adapter capability metadata。
- 文档同步：supported/deferred/non-goal 状态。

结果分类：

- `updated`：`C4M4-TR-PRESS-001/002/004/005/009/012`。
- `unchanged`：`C4M4-TR-PRESS-006`。
- `needs_reselect`：`C4M4-TR-PRESS-007/010/011`。
- `diagnostic_only`：`C4M4-TR-PRESS-003/008`。

S9 输入边界：先实现或明确延期 `topo_reference_pressure_matrix.tsv` 中的 `C4M4-TR-PRESS-001..012`。不得临场发明 fixture 名、分类字段、update 输出字段或新的 split ownership 规则；如必须新增场景，先回到 S8 矩阵追加独立行并同步全局矩阵。

## 非目标

- 不恢复 broad `complete_mapper_history`。
- 不在 adapter 中修正 topo naming。
- 不把 ambiguous relation 升级为 stable alias。
- 不绕过 S8 pressure matrix 新增 ad hoc fixture 场景。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p6_topology tests.test_p7_features tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

## 完成口径

压力 fixture 通过或被稳定 diagnostic 接管；capability 不出现旧 broad topo gap。S9 关闭 `C4M4-TR-BLK-002` / `C4-BLK-402` / `C4-ORC-401`，后续只允许在新矩阵行中扩展场景。
