# C12-M16 Sketch split fragment geometry identity ledger 实现批次方案

## 背景

C12-M15 已发布 `SketchGeometryIdentityLedger` 产品契约，并证明普通 source geometry id 的 `g<ID>` raw edge identity 当前可用。但 C12-M15 把 split fragment durable identity 判成 `design_only`：一个 source edge 被 split 成多个 current fragments 时，当前只能依赖 mapper / ReferenceShadow reselect diagnostic，不能稳定发布每个 fragment 的 identity。

用户明确要求继续写代码实现该缺口。C12-M16 因此只处理 split fragment ledger，不重开 C12-M15 的 no-code 结论，也不重写普通 raw edge identity。

## 核心设计

| 项 | 设计 |
| --- | --- |
| Module | `SplitFragmentIdentityLedger`，作为 `SketchGeometryIdentityLedger` 的 fragment 分支。 |
| Seam | FaceMaker / WireJoiner / internal shape split history 已产生后，response / reference update 发布前。 |
| Interface | 输入 source geometry identity、source raw edge、current fragment edges、split history / internal alias；输出 source id 到 fragment token 与 current indexed edge 的账本。 |
| Token | `g<ID>:splitN`，N 由 request-local deterministic fragment order 产生；不能由 mesh segment 或 response array order 猜。 |
| Consumers | `mesh.edgeSegments[]`、`subshapes[]`、`rawSketchEdgeIdentity`、`elementReferenceUpdates`、reference resolution。 |

## 实现规则

- 一个 source edge 未 split：继续使用 C12-M15 `g<ID>`。
- 一个 source edge split 成多个 current fragments：发布 `g<ID>:split1..N`，每个 fragment 关联 `sourceGeometryId`、`sourceStableSubname=g<ID>`、`fragmentStableSubname=g<ID>:splitN`、`identityStatus=stable_split_fragment`。
- old reference 为 `g<ID>:splitN` 时，reference resolution 必须解析到当前 matching fragment；找不到时输出 `deleted_stable_subname` / `split_fragment_missing`。
- fragment 数量或几何 kind 改变且不能唯一匹配时，输出 `split_requires_reselect`，不得重绑到随机 fragment。
- `InternalEdgeN` 只有在 internal alias 或 split history 能追溯到 fragment 时继承 fragment identity；否则 fallback / diagnostic。
- fragment ledger request-local，不保存 backend session 或完整 BREP cache。

## 最小完整语义批次

1. self-intersecting or cutter split line：一个 `geometryId` 对应多个 current fragments。
2. split fragment response：`edgeSegments[]` / `subshapes[]` / `rawSketchEdgeIdentity` 同步发布 `g<ID>:splitN`。
3. stable sublist recovery：旧 `StableSubList=["g<ID>:split1"]` 可解析到当前 fragment。
4. fragment deleted / count drift：不能唯一匹配时 diagnostic，不按顺序猜。
5. internal alias：`InternalEdgeN` / `InternalFaceN` 通过 source-backed fragment identity 继承。
6. missing id fallback：没有 geometry id 的 split fragment 不能发布 durable token。

## 工作步骤

- 入口：核对包结构、矩阵、步骤队列。
- S0：冻结 live 基线、C12-M15 继承边界和用户授权的 implementation scope。
- S1：复核 FreeCAD split history source 与 cad-core current split/reselect 行为，明确 red path。
- S2：补 red fixtures / focused tests，锁定 `g<ID>:splitN` response 与 reference update 期望。
- S3：实现 split fragment ledger 的 C++ API、materialization 与 diagnostics。
- S4：接入 response / reference resolution / adapter capability wording，并跑 focused validation。
- S5：发布实现闸门，更新 root README、矩阵和验收记录。

## 发布状态

- Final status：`implemented_current_supported`。
- S0-S4 均已 `【已实现】`：S3 实现 `g<ID>:splitN` request-local fragment ledger，S4 验证 response / reference resolution / adapter / capability public wording 共享同一 fragment token。
- S5 关闭 blocker queue 和 validation matrix；C12-M16 队列关闭后只输出 markdown 表头。
- 后续只在 focused regression、new checked-in expected/current mismatch、adapter/capability contract drift，或用户明确要求 persistent FreeCAD session parity 时重开。

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

## 非目标

- 不重做普通 `g<ID>` raw edge identity。
- 不用 mesh / bbox / output order 推断 split fragment ownership。
- 不处理 frontend consumer sync。
- 不处理 open wire mesh 产品契约。
- 不引入 persistent backend session/cache。
