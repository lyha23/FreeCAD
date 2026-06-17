# P5P6-S4 MapperHistory 与 ElementMap 生命周期专项复审

## 目标

P5P6-S4 单独裁决 request-local `MapperHistory` / `ElementMap` 生命周期进入本主线的边界，同时继续排除 FreeCAD desktop 的跨请求持久 document/cache 状态。

## FreeCAD 依据

- `~/Chili3DProject/FreeCAD/src/App/ElementMap.cpp`
- `~/Chili3DProject/FreeCAD/src/App/MappedName.cpp`
- `~/Chili3DProject/FreeCAD/src/App/GeoFeature.cpp`
- `~/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp`
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::MapperMaker` / `MapperHistory`
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/PropertyTopoShape.cpp`
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShape.cpp`

## 最终边界草案

| 范围 | P5P6 状态草案 | 收口说明 |
| --- | --- | --- |
| request-local MapperHistory event model | `backendGap` | 需要统一 identity / generated / modified / split / merge / deleted / ambiguous。 |
| request-local ElementMap lifecycle | `backendGap` | 需要 mapped postfix、source-prefixed key、child map、find/findAll/history trace。 |
| `ReferenceShadow.brep` 单 subshape evidence | `backendGap` | 只作为旧引用恢复证据，不作为建模输入。 |
| cross-request ElementMap / NamedShape persistence | `nonGoal` | 不改变无状态 CAD Core 架构。 |
| full BREP / topomap / mesh cache | `nonGoal` | 不保存跨请求几何结果。 |
| ambiguous split 自动唯一恢复 | `nonGoal` | 没有同类唯一 evidence 时只能 diagnostic。 |

## 必须回写的矩阵行

- `P5P6-SCOPE-001`：MapperHistory event model。
- `P5P6-SCOPE-002`：ElementMap child map / mapped postfix / source key。
- `P5P6-SCOPE-005`：unified resolver 的 ElementMap 输入。
- `P5P6-SCOPE-006`：ReferenceShadow evidence。
- `P5P6-NG-002`：persistent shape cache。
- `P5P6-NG-005`：ambiguous split 自动唯一恢复。

## 实现准入条件

进入 C++ 前必须先定：

- history event 字段：source object/subname、target object/subname、shape kind、relation、maker stage、evidence、recoverability。
- split / merge / deleted 终态如何进入 diagnostics。
- 哪些 relation 可写入可解析 `ElementMap`，哪些只能保留 history diagnostic。
- ReferenceShadow evidence 与 ElementMap 冲突时的优先级和输出。

## 验收

```bash
python3 - <<'PY'
import csv
from pathlib import Path
scope = Path('docs/FreeCAD几何生态迁移工程-细分/P5P6-ExternalGeometry-TopoNaming联合主线/矩阵/p5p6_scope_review_matrix.tsv')
with scope.open(newline='') as f:
    rows = list(csv.DictReader(f, delimiter='\t'))
assert any(r['scope_id'] == 'P5P6-SCOPE-001' for r in rows)
assert any(r['scope_id'] == 'P5P6-SCOPE-002' for r in rows)
PY
git diff --check
```

实现阶段再跑 touched topo/app tests 和 P5/P6 stable subname fixture。

## 非目标

- 不引入跨请求 `ElementMap` / `NamedShape` 持久状态。
- 不把完整 BREP 作为请求或响应持久字段。
- 不靠输出顺序、面积、长度或几何类型猜唯一恢复目标。
- 不让 adapter 承担 resolver 业务语义。
