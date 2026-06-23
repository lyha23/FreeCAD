# 【已实现】C5-M19-S2 scope 准入与 nonGoal 矩阵

状态：`done_scope_routed`

## 分类规则

| 分类 | 条件 | C5-M19 结果 |
| --- | --- | --- |
| `backendGap` | FreeCAD 有可执行 route，cad-core 缺 DTO/helper/fixture/test/capability | 无 |
| `source_audited_nonGoal` | enum/name 存在，但无 `modeRefTypes` 或执行 case | `TangentU`、`TangentV`、`IntersectionPoint` |
| `supported_guard` | 已支持模式需要避免误降级 | `IntersectionLine` |
| `nonGoal` | GUI/session/full BREP 或 Part surface 后续方向 | 保留 |

## scope 结论

| scope | 结论 | 说明 |
| --- | --- | --- |
| `C5M19-SCOPE-101` | `source_audited_nonGoal` | `TangentU` 无 `modeRefTypes[mm1TangentU]` 和 switch case |
| `C5M19-SCOPE-102` | `source_audited_nonGoal` | `TangentV` 无 `modeRefTypes[mm1TangentV]` 和 switch case |
| `C5M19-SCOPE-201` | `source_audited_nonGoal` | `IntersectionPoint` 无 `modeRefTypes[mm0Intersection]` 和 switch case |
| `C5M19-SCOPE-301` | `supported_guard` | `IntersectionLine` 是 `mm1Intersection`，保持 supported |
| `C5M19-SCOPE-501` | `done_release_gate` | capability exact blocker 删除，non-goal 文案发布 |

## 必须回写的矩阵行

- `c5m19_datum_attach_engine_scope_review_matrix.tsv`
- `c5m19_datum_attach_engine_blocker_queue.tsv`
- `c5m19_datum_attach_engine_non_goal_registry.tsv`
- `c5m19_datum_attach_engine_backend_gap_classification.tsv`
- 根矩阵 `C5-SCOPE-1901`、`C5-BLK-1901`、`C5-NG-022`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
for f in docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M19-DatumAttachEngineSourceAuditCloseout主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
rg -n 'C5M19-SCOPE-101|C5M19-SCOPE-102|C5M19-SCOPE-201|C5M19-SCOPE-301|source_audited_nonGoal' docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M19-DatumAttachEngineSourceAuditCloseout主线/矩阵
```

## 非目标

- 不创建 `backendGap` 行。
- 不创建 fixture/oracle 行来支撑不可选模式。
