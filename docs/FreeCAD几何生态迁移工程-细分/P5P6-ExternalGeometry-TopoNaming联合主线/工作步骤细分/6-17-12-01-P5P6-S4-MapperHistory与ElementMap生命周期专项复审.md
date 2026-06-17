# 【已实现】P5P6-S4 MapperHistory 与 ElementMap 生命周期专项复审

## 目标

P5P6-S4 只裁决 request-local `MapperHistory` / `ElementMap` / `ReferenceShadow` 生命周期边界。它不写 C++、不采 oracle、不改 fixture expected，也不把 FreeCAD desktop 的持久 document / shape cache 迁入无状态 CAD Core。

## live 基线复核

| 项 | 复核结果 |
| --- | --- |
| `pwd` | `/home/user/Chili3DProject/FreeCAD` |
| `git rev-parse --short HEAD` | `baae7d9419` |
| `git log -1 --oneline` | `baae7d9419 docs: 完成 P5P6 S3 ExternalGeometry 状态机复审` |
| 初始非本步 dirty | `AGENTS.md`、`DESIGN.md`、`cad-core/CMakeLists.txt` 已在本步骤开始时存在 dirty；S4 不编辑、不暂存、不提交这些文件。 |

## 输入

- P5P6 主线入口、步骤总览、S0-S3 已实现文档和 live TSV 矩阵。
- `docs/CADCore方案/细化方案/09-P6-TopoNaming主路径.md`
- `docs/CADCore方案/细化方案/13-ExternalGeometry-TopoNaming下一阶段主线.md`
- FreeCAD authority：`src/App/ElementMap.cpp`、`MappedName.cpp`、`GeoFeature.cpp`、`PropertyLinks.cpp`、`src/Mod/Part/App/TopoShapeExpansion.cpp`、`PropertyTopoShape.cpp`、`PartFeature.cpp`。
- cad-core 只读 evidence：`cad-core/include/cad_core/part/topo_shape_mapper.h`、`topo_shape.h`、`cad-core/src/part/topo_shape.cpp`、`cad-core/src/runtime/recompute.cpp`、`cad-core/tests/test_p6_topology.py`、`test_adapters.py`、`test_diagnostics.py`。

## FreeCAD 依据

| 源码入口 | S4 使用的语义 |
| --- | --- |
| `~/Chili3DProject/FreeCAD/src/App/ElementMap.cpp::addName()` / `find()` / `findAll()` | `ElementMap` 保存 indexed name 到 mapped name 的 request-local 命名账本，并通过 child map、postfix、hash/dehash 扩展查找；多名映射只说明可追溯关系，不自动证明旧引用可唯一写回。 |
| `~/Chili3DProject/FreeCAD/src/App/ElementMap.cpp::getElementHistory()` / `traceElement()` | mapped name 的 tag / postfix chain 可被追踪成 history intermediates；遇到循环、外部 tag 或缺对象时停止，不能把不完整 history 伪造成唯一 ElementMap target。 |
| `~/Chili3DProject/FreeCAD/src/App/MappedName.cpp::MappedName::findTagInElementName()` | `;:H` tag、长度、shape type 和 recursive lookup 是 mapped postfix / source key 的语法依据。 |
| `~/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap()` | 先消费 `mapper.modified()` / `mapper.generated()`，再构造 generated / modified / upper / lower element names；这是 cad-core `MapperHistoryEvent -> ElementMap / diagnostic` 的 request-local 主路径依据。 |
| `~/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::MapperMaker` / `MapperHistory` | `MapperMaker` 适配 `BRepBuilderAPI_MakeShape`，`MapperHistory` 适配 `BRepTools_History` / `BRepTools_ReShape` / `ShapeFix_Root` history。S4 只裁决消费边界，FaceMaker / WireJoiner producer 仍留给 S5。 |
| `~/Chili3DProject/FreeCAD/src/App/GeoFeature.cpp::resolveElement()` 与 `~/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp::PropertyLinkBase::_updateElementReference()` | 旧 subname 更新通过 `ElementNamePair`、`ShadowSub.oldName/newName` 和 link property writeback 完成；cad-core 对应为 request-local `elementReferenceUpdates`，不是 adapter 业务逻辑。 |
| `~/Chili3DProject/FreeCAD/src/Mod/Part/App/PropertyTopoShape.cpp` | FreeCAD desktop 会保存 shape / ElementMap / hasher 并按版本触发 recompute；cad-core 无状态架构只借鉴语义，不迁移跨请求 `NamedShape` / `ElementMap` / BREP cache。 |

## S4 裁决

| 范围 | S4 状态 | 裁决 |
| --- | --- | --- |
| `P5P6-SCOPE-001` MapperHistory event schema | `supported` | 保持 S2 live matrix 结论。cad-core 已有 `MapperHistoryEvent` 的 source/target、shape kind、relation、maker stage、evidence、recoverability 字段，并有 split / deleted / merge diagnostics checked-in evidence；S4 不把旧 seed 草案回退成 backendGap。 |
| `P5P6-SCOPE-002` ElementMap child map / mapped postfix / source key | `supported` | 保持 supported。child map、mapped postfix alias、source-prefixed stable key 和 policy propagation 已有源码与测试证据；后续 producer gap 必须喂入这套账本，而不是重定义 ElementMap 生命周期。 |
| `P5P6-SCOPE-003` unified resolver / ReferenceShadow writeback | `supported` | 保持 supported。runtime 已先走 current `NamedShape.elementMap`，再用 `ReferenceShadow` fingerprint / 单 subshape BREP evidence 校验和更新 `SubList` / `StableSubList` / `ShadowSub` / `ReferenceShadow`。 |
| `ReferenceShadow.brep` | evidence-only | 只允许作为旧单个 subshape snapshot evidence；成功恢复后可刷新该单 subshape shadow。不得把它扩大为建模输入、完整对象 BREP、前端长期几何状态或后端跨请求缓存。 |
| 跨请求 `ElementMap` / `NamedShape` / topomap / mesh / shape cache | `nonGoal` | 继续由 `P5P6-NG-002` 排除。`DocumentObject graph` 是唯一真实数据；请求结束后的几何结果不能作为后端长期状态。 |
| ambiguous split 自动唯一恢复 | `nonGoal` | 继续由 `P5P6-NG-005` 排除。只有 `MapperHistory` / `ElementMap` 能证明同类唯一 replacement 时才可恢复；一对多 split 继续输出 `split_stable_subname` / `subname_resolve_ambiguous` 类 diagnostic。 |
| 完整对象 BREP protocol | `nonGoal` | 继续由 `P5P6-NG-006` 排除。完整对象 BREP 不进入请求/响应长期协议，也不作为跨请求几何状态。 |
| `P5P6-SCOPE-012` `Profile.StableSubList=InternalFaceN` without `ReferenceShadow` | `unsupported` | 继续保持 unsupported diagnostic。当前没有 FreeCAD evidence 证明 request-local `InternalFaceN` 可以直接升级成持久 stable selector；若 S5/S6 通过 FaceMaker / WireJoiner producer 和 ElementMap 证明可解析，再重新开 scope。 |
| `P5P6-SCOPE-013` fallback retirement | `releaseGate` | 继续保持 releaseGate。S4 只确认 fallback 不能替代生命周期主路径；删除或保留临时 fallback 的证据审计等 S5/S6。 |
| MapperHistory / ElementMap backendGap | none | S4 不新增 lifecycle backendGap。live backendGap 仍只保留 FaceMaker concrete producer 和 WireJoiner vertex multiplicity parity。 |

## 矩阵回写

- `p5p6_scope_review_matrix.tsv`：`P5P6-SCOPE-001/002/003` 保持 supported；`P5P6-SCOPE-012` 保持 unsupported；`P5P6-SCOPE-013` 保持 releaseGate。
- `p5p6_blocker_queue.tsv`：`P5P6-BLOCK-005` 继续作为 unsupported diagnostic / capability 队列；`P5P6-BLOCK-006` 继续作为 S5/S6 fallback release gate。
- `p5p6_non_goal_registry.tsv`：`persistent_shape_cache`、`automatic_unique_recovery_for_ambiguous_split`、`full_object_brep_protocol` 保持 nonGoal，并保留 reopen condition。
- `p5p6_backend_gap_classification.tsv`：不加入 MapperHistory / ElementMap lifecycle gap；FaceMaker / WireJoiner producer gap 继续要求喂入既有 MapperHistory / ElementMap 主路径。
- 主线入口和步骤总览只把 S4 标为已实现；S5-S6 保持待执行。

## 后续队列

| 队列 | 保留原因 |
| --- | --- |
| `P5P6-BLOCK-005` | `InternalFaceN` stable selector without `ReferenceShadow` 仍缺 FreeCAD evidence；发布前必须有 diagnostic/capability 说明，或由 S5/S6 改为 ElementMap-backed supported。 |
| `P5P6-BLOCK-006` | summary-only、geometry-match、diagnostic-only fallback 是否可删，必须等 FaceMaker / WireJoiner producer 复审和 S6 发布闸门。 |

## 验收

```bash
rg -n "MapperHistory|MapperMaker|makeShapeWithElementMap|addName|findAll|getElementHistory|traceElement|ReferenceShadow|MappedName" src/App/ElementMap.cpp src/App/MappedName.cpp src/App/GeoFeature.cpp src/App/PropertyLinks.cpp src/Mod/Part/App/TopoShapeExpansion.cpp src/Mod/Part/App/PropertyTopoShape.cpp src/Mod/Part/App/PartFeature.cpp
python3 - <<'PY'
import csv
from pathlib import Path
root = Path('docs/FreeCAD几何生态迁移工程-细分/P5P6-ExternalGeometry-TopoNaming联合主线/矩阵')
with (root / 'p5p6_scope_review_matrix.tsv').open(newline='') as f:
    scope = {row['scope_id']: row for row in csv.DictReader(f, delimiter='\t')}
for sid in ['P5P6-SCOPE-001', 'P5P6-SCOPE-002', 'P5P6-SCOPE-003', 'P5P6-SCOPE-012', 'P5P6-SCOPE-013']:
    assert sid in scope, sid
with (root / 'p5p6_blocker_queue.tsv').open(newline='') as f:
    blockers = list(csv.DictReader(f, delimiter='\t'))
for row in blockers:
    assert row['scope_id'] in scope, row
with (root / 'p5p6_non_goal_registry.tsv').open(newline='') as f:
    non_goals = {row['exclusion_axis']: row for row in csv.DictReader(f, delimiter='\t')}
for axis in ['persistent_shape_cache', 'full_object_brep_protocol', 'automatic_unique_recovery_for_ambiguous_split']:
    assert non_goals[axis]['reopen_condition'], axis
PY
git diff --check
```

## 非目标

- 不写 C++。
- 不采 FreeCAD oracle。
- 不引入跨请求 `ElementMap` / `NamedShape` / topomap / mesh / shape cache。
- 不把完整对象 BREP 作为请求、响应或长期状态。
- 不靠输出顺序、面积、长度、几何类型或 fixture 名称猜唯一恢复目标。
- 不让 adapter 承担 resolver 业务语义。
