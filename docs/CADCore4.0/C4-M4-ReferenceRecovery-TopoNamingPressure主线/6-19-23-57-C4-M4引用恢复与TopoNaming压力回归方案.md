# C4-M4 引用恢复与 TopoNaming 压力回归方案

## 目标

C4-M4 不恢复 C3.0 已关闭的 broad topo gap，而是在长期编辑链路上补压力回归：多个 producer、多个引用恢复机制、多个 feature-family 混合时，`MapperHistory -> ElementMap -> elementReferenceUpdates` 仍然稳定。

## 当前基线

C3.0 已有：

- ElementMap policy Drop / Propagate。
- child-map source range、recursive source range、postfix、hash key。
- ShapeFix、import、offset、section、DressUp、transformed、Hole、Loft、Sweep、Filling history。
- WireJoiner open-export / noOriginal / split / deleted 关系进入正式 history。
- source object rename、label rename、cross-document label、XLink missing / pending / unloaded diagnostics。

## 4.0 压力方向

| 方向 | 目的 |
| --- | --- |
| 多 producer 混合链 | Pad / Pocket / DressUp / Pattern / Boolean / Loft / Sweep 组合后仍能追 source ownership |
| import change lifecycle | STEP / IGES / BREP 重新导入、形状变化、source subname 失效时输出明确 update 或 diagnostic |
| rename + Link retag | object rename、label rename、Link retag、CopyOnChange deep copy 后稳定恢复 |
| ReferenceShadow fallback | 只用单 subshape snapshot 做引用恢复证据，不扩大为建模输入 |
| failure boundary | split / deleted / ambiguous relation 不写唯一 alias，要求前端重选或提示 |

## 实施批次

| 批次 | 内容 | 验收重点 |
| --- | --- | --- |
| C4-M4-S1 | 压力 fixture 矩阵 | 组合 producer、引用恢复、Link、Body Tip、PartDesign chain |
| C4-M4-S2 | import change recovery | changed / deleted / ambiguous subshape diagnostics |
| C4-M4-S3 | rename + Link retag lifecycle | `elementReferenceUpdates` 与 `documentObjectUpdates` 合并稳定 |
| C4-M4-S4 | terminal history publication | capabilities 暴露 relation、recoverability、needs_reselect |

S8 已将 C4-M4-S1/S2/S3/S4 的实现输入压到包内矩阵：`矩阵/topo_reference_pressure_matrix.tsv`。S9 按该矩阵落地 12 个 `cad-core/fixtures/c4m4/topo-reference-pressure-*.json`，覆盖 `updated`、`unchanged`、`needs_reselect`、`diagnostic_only` 四类结果，并固定 fixture 名、source ownership、terminal history、`elementReferenceUpdates`、`documentObjectUpdates`、ReferenceShadow 边界和验证命令。

S9 发布状态：

- `topo_reference_pressure` adapter capability 暴露 12 个 pressure fixtures、四类 classification、更新字段和稳定 diagnostic code；adapter 只发布契约元数据，不承接 topo naming 修正。
- `tests.test_p6_topology` 断言 `elementReferenceUpdates`、`documentObjectUpdates`、terminal history 和 diagnostic 定位字段。
- import missing stable key 被固定为 `deleted_stable_subname` / `subname_resolve_ambiguous` 等 stable diagnostic，不升级为当前面/边的唯一 alias。

## 非目标

- 不在 adapter 或 expected JSON 中重排 subshape 来伪造稳定性。
- 不把 split / ambiguous relation 写成唯一 alias。
- 不把完整 BREP 当作前端或后端长期状态。

## 验收

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore4.0 cad-core
```

阶段回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p6_topology tests.test_p7_features tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

## 可执行包入口

- 压力矩阵：`docs/CADCore4.0/C4-M4-ReferenceRecovery-TopoNamingPressure主线/工作步骤细分/6-20-00-09-【已实现】C4-S8-M4-TopoReference压力矩阵.md`
- 包内 pressure matrix：`docs/CADCore4.0/C4-M4-ReferenceRecovery-TopoNamingPressure主线/矩阵/topo_reference_pressure_matrix.tsv`
- 压力实现与发布：`docs/CADCore4.0/C4-M4-ReferenceRecovery-TopoNamingPressure主线/工作步骤细分/6-20-00-10-【已实现】C4-S9-M4-TopoReference压力实现与发布.md`
- fixture/oracle 矩阵：`docs/CADCore4.0/矩阵/cadcore4_fixture_oracle_matrix.tsv`
