# C12-M16 Sketch split fragment geometry identity ledger 实现批次

C12-M16 承接 C12-M15 的唯一代码缺口：`C12M15-CONTRACT-009 split_fragment_boundary`。C12-M15 已把 `geometryId -> g<ID> -> current EdgeN/InternalEdgeN` 账本发布为产品契约，但把 one source edge split into many current fragments 关闭为 `design_only`。用户已明确要求继续写代码实现该缺口，因此 C12-M16 是 implementation 批次，不是 no-code 设计批次。

本包目标是把 split fragment 从“只能 reselect / diagnostic”推进到 request-local fragment ledger：当同一个 source geometry edge 被 FaceMaker / WireJoiner / internal shape history 拆成多个 current fragments 时，后端应能发布明确的 fragment identity，例如 `g<ID>:splitN`，并让 `mesh.edgeSegments[]`、`subshapes[]`、`rawSketchEdgeIdentity`、`elementReferenceUpdates` 和 `StableSubList` 解析使用同一账本。

## 当前基线

- 创建基线：`pwd=/Users/li/Chili3DProject/FreeCAD`。
- 创建基线：`HEAD=decfc267a2`（`decfc267a2 docs: 关闭 C12-M15 S4 设计发布闸门`）。
- 创建前 `git -c core.quotepath=false status --short -uall` 无输出。
- S0 live 冻结：`HEAD=a4375f45a5`（`a4375f45a5 文档：关闭 C12-M16 工作步骤总入口`），`pwd=/Users/li/Chili3DProject/FreeCAD`，起点 `git -c core.quotepath=false status --short -uall` 无输出。
- C12-M15 队列已关闭且当前只输出 markdown 表头，final status 为 `design_published_no_code_current_sufficient`；C12-M16 显式重开其中 `CONTRACT-009` 的 implementation lane。
- S0 已冻结非目标：不重开普通 `g<ID>` raw edge identity，不处理 my-chili3d frontend sync，不处理 C12-M11 open wire mesh contract，不引入 persistent backend sketch session；下一步进入 S1。
- S4 接入验证：`HEAD=7c5ce46eca`（`7c5ce46eca 实现 C12-M16 S3 split fragment ledger`），起点 worktree clean；`raw_edge_identity`、`mesh.edgeSegments[]`、`subshapes[]` 和 `elementReferenceUpdates` 已复核共享 `g701:split1..3`，`StableSubList=["g701:split1"]` 解析到当前 `InternalEdge3`；capability / adapter 公开口径发布 request-local split fragment ledger support，并明确不声称 persistent FreeCAD session parity；下一步进入 S5 发布闸门。

## 问题定义

C12-M15 已证明普通 raw edge 的 stable identity 能以 `g<ID>` 发布和解析；但 split 场景仍只是边界规则：没有 fragment ledger / ElementMap 证据时不得把任意 fragment 自动声明为 source edge 的稳定延续。

C12-M16 要补的是这层证据：

1. 读取 source geometry id、raw source edge、current fragment edge 和 split history。
2. 为一个 source edge 的多个 current fragments 分配 request-local deterministic fragment token。
3. 把 token 发布到 response 与 reference update，而不是让 reference resolution 只能报 `subname_split_requires_reselect`。
4. 对无法唯一确定 fragment ownership 的场景继续输出 diagnostic，不靠 bbox、mesh 顺序或 source order 猜。

## FreeCAD source authority

| 语义 | FreeCAD source | C12-M16 用法 |
| --- | --- | --- |
| self-intersection pre-split history | `src/Mod/Part/App/FaceMakerBuildFace.cpp::FaceMakerBuildFace::splitSelfIntersecting()` | `myPreSplitHistory->AddModified(edge, fragment)` 是 source edge -> fragment 账本依据。 |
| FaceMaker split history chaining | `src/Mod/Part/App/FaceMaker.cpp::postBuild()` | `MapperHistory(myPreSplitHistory)` 与 `MapperMaker(mySplitter)` 说明 split history 应进入 ElementMap / mapped name，而不是输出端猜测。 |
| internal shape element mapping | `src/Mod/Sketcher/App/SketchObject.cpp::buildInternals()` / `getInternalElementMap()` | `InternalEdgeN/InternalFaceN` 需要通过 internal map 和 raw identity 追溯到 source-backed fragment。 |
| WireJoiner split bookkeeping | `src/Mod/Part/App/WireJoiner.cpp` | open wire / split edge ownership 需要 EdgeInfo / MapperHistory 等账本，不在 sketch executor 里补猜。 |
| mapped name propagation | `src/Mod/Part/App/TopoShapeExpansion.cpp::MapperHistory` | split fragment token 必须跟随 topo/history mapper，而不是只写 response 后处理。 |

## cad-core 落点

| 落点 | 角色 |
| --- | --- |
| `cad-core/include/cad_core/sketcher/sketch_edge_identity.h` | 扩展 `RawSketchEdgeIdentityLedger` 或新增 fragment ledger view。 |
| `cad-core/src/sketcher/sketch_edge_identity.cpp` | 生成 `g<ID>:splitN`，维护 source edge -> fragment edge 的 deterministic mapping。 |
| `cad-core/src/sketcher/sketch_internal_result.cpp` | 把 fragment ledger 放进 object fields，并发布到 mesh/subshapes。 |
| `cad-core/src/runtime/recompute.cpp` | response `edgeSegments[]` / `subshapes[]` 透传 fragment identity 字段。 |
| `cad-core/src/runtime/reference_resolution.cpp` | 让 `StableSubList` / `ReferenceShadow` 能解析 `g<ID>:splitN` 到当前 fragment，失败时保留 diagnostic。 |
| `cad-core/tests/test_p5_sketch.py` 或新增 `cad-core/tests/test_c12m16_sketch_split_fragment_identity.py` | focused red/green tests。 |
| `cad-core/fixtures/c12m16/` | 最小 split fragment implementation fixtures。 |

## 实现目标

- 定义 `SplitFragmentIdentityLedger` 或等价子结构，作为 `SketchGeometryIdentityLedger` 的 fragment 分支。
- 支持 source one-to-many fragment identity：`g<ID>:split1`、`g<ID>:split2` 等 token 稳定、可解释、request-local deterministic。
- `mesh.edgeSegments[]`、`subshapes[]`、`rawSketchEdgeIdentity.byStableSubname`、`rawSketchEdgeIdentity.byIndexed` 和 `elementReferenceUpdates` 共享同一 fragment ledger。
- 对 ambiguous split、fragment count drift、kind drift、missing fragment、old split token deleted 保留 explicit diagnostic / needs-reselect。
- 不保存 backend sketch session、TopoDS、NamedShape、ElementMap、BREP 或 mesh 跨请求状态。

## 非目标

- 不重开 C12-M15 已 current-supported 的普通 `g<ID>` raw edge identity。
- 不用 mesh polyline / bbox / output order 猜 fragment ownership。
- 不实现 my-chili3d frontend consumer sync。
- 不裁决 C12-M11 open wire raw edge mesh 产品契约。
- 不扩展到完整 Sketcher solver constraint identity。
- 不引入 persistent backend document/session/cache。

## 入口

- 总入口：`7-4-19-52-C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次总入口.md`
- 方案：`7-4-19-52-C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次 docs/CADCore12.0/README.md
git diff --check
```

实现 focused：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_returns_split_internal_face_mesh_ids
```

阶段收口候选：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests/test_mvp.py
```
