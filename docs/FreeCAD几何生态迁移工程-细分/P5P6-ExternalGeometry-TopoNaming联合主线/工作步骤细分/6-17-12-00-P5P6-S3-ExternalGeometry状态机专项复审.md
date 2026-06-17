# 【已实现】P5P6-S3 ExternalGeometry 状态机专项复审

## 目标

P5P6-S3 只裁决 `ExternalGeometryExtension` 状态机的 request-local 边界。它不采 FreeCAD oracle、不写 C++、不改 fixture expected，也不把当前 cad-core focused test 伪装成 FreeCAD parity。

## live 基线复核

| 项 | 复核结果 |
| --- | --- |
| `pwd` | `/home/user/Chili3DProject/FreeCAD` |
| `git rev-parse --short HEAD` | `96ad379ba0` |
| `git log -1 --oneline` | `96ad379ba0 docs: 完成 P5P6 S2 范围准入矩阵` |
| 初始非本步 dirty | `AGENTS.md`、`DESIGN.md`、`cad-core/CMakeLists.txt` 已在本步骤开始时存在 dirty；S3 不编辑、不暂存、不提交这些文件。 |

## FreeCAD 依据

| 源码入口 | 本轮裁决使用的语义 |
| --- | --- |
| `~/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/ExternalGeometryExtension.h` | `Defining`、`Frozen`、`Detached`、`Missing`、`Sync` 是保存在 external geometry extension 上的 flags；`Defining` 注释为 `allow an external geometry to build shape`，`Sync` 注释为 `signal the intention to synchronize a frozen geometry`。 |
| `~/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/ExternalGeometryExtension.cpp` | `Ref`、`RefIndex`、`Flags` 会随 external geometry 保存 / 恢复 / copy。cad-core 只能把这些作为请求 graph 字段消费，不能把 FreeCAD desktop session 状态变成后端缓存。 |
| `~/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/ExternalGeometryFacade.*` | facade 只保证 geometry 上存在 `SketchGeometryExtension` 与 `ExternalGeometryExtension`，并提供 `copyFlags()`、`setRef()` 等访问器；Python facade/session API 不属于后端几何核心。 |
| `~/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp::SketchObject::addExternal()` | 新增 external link 时传入 `defining` / `intersection`，再通过 `rebuildExternalGeometry(extToAdd)` 初始化 projection / intersection 结果。 |
| `~/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp::SketchObject::rebuildExternalGeometry()` | 读取 `ExternalGeometry`、`ExternalTypes`、`ExternalGeo`、`Frozen`、`Sync`、`Defining`、`Missing`；`Frozen && !Sync` 插入 `refSet` 后跳过刷新；成功 rebuild 后清 `Sync`，按 `refSet` 决定 `Missing`；Missing pre-pass 通过 `GeoFeature::resolveElement()` 恢复旧 subname。 |
| `~/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::onExternalGeoChanged()` | `Detached` 会清空 geometry ref、清 `Detached/Missing`，并从 `ExternalGeometry` link 列表删除对应 ref。Detached 是 FreeCAD session 对 persisted `ExternalGeo` 的维护语义；cad-core 只能输出 request-local update 建议。 |
| `~/Chili3DProject/FreeCAD/src/App/GeoFeature.cpp::GeoFeature::resolveElement()` | 旧 subname 恢复必须由统一 resolver 提供 `newName/oldName`，不能在 sketcher consumer 中按几何类型、fixture 名称、split 顺序或 source index 猜。 |

## S3 裁决

| 范围 | S3 状态 | 裁决 |
| --- | --- | --- |
| ExternalGeometry projection / intersection baseline | `supported` | 保持 `P5P6-SCOPE-005` supported：edge / vertex / face / whole-shape、`ExternalTypes=Projection/Intersection/Both` 已有 P5 expected 和 focused tests。复杂 HLR / 非平面扩展不由 S3 放大。 |
| ExternalGeometry flags parse / capability | `supported` | 保持 `P5P6-SCOPE-006` supported：协议字段、`ExternalFlags` parse、capability 暴露和 request-local update payload 已存在；这只证明字段与能力通道，不证明五类状态 FreeCAD parity。 |
| source-prefixed Missing recovery | `supported` | 保持 `P5P6-SCOPE-004` supported：已有 FreeCADCmd oracle 的 source-prefixed recovery 和 C3M2 Missing focused recovery；deleted / unresolved state-machine parity 仍归 `P5P6-SCOPE-008`。 |
| `Defining` external profile | `notCollected` | 保持 `P5P6-SCOPE-007` notCollected。cad-core 有 `test_p5_external_geometry_defining_participates_in_profile`，但缺 FreeCAD oracle 区分 Defining 与 reference-only external geometry，不能写 supported。 |
| `Frozen` / `Frozen + Sync` / `Detached` / unresolved `Missing` | `notCollected` | 保持 `P5P6-SCOPE-008` notCollected。cad-core 有 request-local focused tests、native `ExternalGeo` tests 和 C3M2 snapshot fixtures，但缺 FreeCAD source-changed oracle，不能关闭 blocker。 |
| Missing / deleted recovery 路径 | resolver-only | Missing/deleted 必须先走 `GeoFeature::resolveElement()` 对应的统一 resolver、MapperHistory / ElementMap 和 `ReferenceShadow` evidence。没有唯一 evidence 时输出 stable diagnostic；不得在 `cad-core/src/sketcher/sketch_object_external.cpp` 中猜恢复目标。 |
| Python facade / GUI / live editing | `nonGoal` | 保持 `P5P6-NG-003`。后端只暴露 flags、diagnostics、mesh/subshape 和 update 建议；不迁移 FreeCAD Python facade、editor session、GUI command、TaskPanel、ViewProvider 或 live editing workflow。 |
| 完整 Sketcher solver | `nonGoal` | 保持 `P5P6-NG-001`。本主线只保留 solver-facing 输入、状态和 diagnostics，不迁移会移动几何的完整约束求解器。 |

## cad-core focused evidence

| evidence | 结论边界 |
| --- | --- |
| `cad-core/tests/test_p5_sketch.py::test_p5_external_geometry_defining_participates_in_profile` | 证明 cad-core 当前能让 `ExternalFlags=["Defining"]` 参与 profile，但不是 FreeCADCmd expected。 |
| `test_p5_external_geometry_frozen_and_detached_do_not_follow_source`、`test_p5_external_geometry_sync_refreshes_and_clears_sync_flag`、`test_p5_external_geometry_missing_recovery_clears_missing_flag` | 证明 request-local behavior 和 update payload 目前有 focused coverage，但不能替代 source-changed FreeCAD oracle。 |
| `test_c3m2_external_geometry_*` 与 `cad-core/fixtures/c3m2/sketch-external-*.json` | 证明 `ReferenceShadow.brep` / request-side `ExternalGeo` snapshot 的冻结、缺失和诊断通道存在；不证明 FreeCAD desktop 对 Frozen/Sync/Detached/Missing 的完整状态迁移 parity。 |
| `cad-core/fixtures/p5/expected/sketch-external-face-intersection.freecad.json`、`sketch-external-face-both.freecad.json`、`sketch-external-whole-box.freecad.json` | 证明 projection/intersection baseline 有 checked-in expected；不扩大到 Defining/Frozen/Sync/Detached/Missing oracle。 |

## Oracle 队列

S3 保留并细化两个未关闭 blocker：

| blocker | scope | 必采 oracle |
| --- | --- | --- |
| `P5P6-BLOCK-001` | `P5P6-SCOPE-007` | 原生 FreeCAD 中同一 external source 分别设置 Defining / reference-only，采集 `ExternalGeo` flags、construction / profile 参与、下游 Pad/Pocket profile 结果和诊断。 |
| `P5P6-BLOCK-002` | `P5P6-SCOPE-008` | 原生 FreeCAD 中采集 Frozen source-changed without Sync、Frozen+Sync source-changed、Detached source-changed、Missing object、Missing subshape、deleted target、snapshot missing / present 的 `ExternalGeo`、`ExternalGeometry` link、诊断和 recompute 行为。 |

FreeCAD expected 必须来自本地 FreeCAD 行为或 focused probe，不能从 cad-core 当前输出倒推。若 oracle 与 cad-core focused behavior 不一致，后续 S6 再把对应 scope 改为 `backendGap` 或 `unsupported`，并按 FreeCAD 调用链实现。

## 矩阵回写

- `p5p6_scope_review_matrix.tsv`：S3 仅细化 `P5P6-SCOPE-007` / `P5P6-SCOPE-008` 的 FreeCAD 依据、cad-core evidence 和下一步；两行继续保持 `notCollected`。
- `p5p6_blocker_queue.tsv`：保留 `P5P6-BLOCK-001` / `P5P6-BLOCK-002`，把它们作为可执行 FreeCAD oracle 队列，不关闭。
- `p5p6_non_goal_registry.tsv`：明确 Python facade、GUI/live editing 和完整 solver 的 nonGoal 路由与 reopen 条件。
- 步骤总览和主线入口只把 S3 标为已实现；S4-S6 仍待执行。

## 验收

本步骤只做文档 / TSV 复审：

```bash
rg -n "Defining|Frozen|Detached|Missing|Sync|rebuildExternalGeometry|ExternalGeometryExtension" src/Mod/Sketcher/App/SketchObjectExternal.cpp src/Mod/Sketcher/App/ExternalGeometryExtension.*
python3 - <<'PY'
import csv
from pathlib import Path
root = Path('docs/FreeCAD几何生态迁移工程-细分/P5P6-ExternalGeometry-TopoNaming联合主线/矩阵')
with (root / 'p5p6_scope_review_matrix.tsv').open(newline='') as f:
    scope = {row['scope_id']: row for row in csv.DictReader(f, delimiter='\t')}
with (root / 'p5p6_blocker_queue.tsv').open(newline='') as f:
    blockers = list(csv.DictReader(f, delimiter='\t'))
for sid in ['P5P6-SCOPE-007', 'P5P6-SCOPE-008']:
    assert sid in scope, sid
    if scope[sid]['current_status'] == 'notCollected':
        assert any(b['scope_id'] == sid and b['scope_status'] == 'notCollected' for b in blockers), sid
with (root / 'p5p6_non_goal_registry.tsv').open(newline='') as f:
    non_goals = list(csv.DictReader(f, delimiter='\t'))
for axis in ['python_facade_session_api', 'full_sketcher_solver']:
    rows = [row for row in non_goals if row['exclusion_axis'] == axis]
    assert rows and rows[0]['reopen_condition'], axis
PY
git diff --check
```

## 非目标

- 不采 FreeCAD oracle。
- 不写 C++。
- 不改 fixture expected。
- 不保存跨请求 external geometry cache。
- 不把 Python facade/session API、GUI/live editing 或完整 solver 写入 supported。
- 不把 Missing / deleted 绕过统一 resolver 和 `ReferenceShadow` evidence。
