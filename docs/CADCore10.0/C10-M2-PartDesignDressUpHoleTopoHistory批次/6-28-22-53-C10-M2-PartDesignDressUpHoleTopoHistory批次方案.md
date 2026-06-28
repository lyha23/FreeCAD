# C10-M2 PartDesign DressUp / Hole TopoHistory 批次方案

## 背景

C10-M1 已关闭 Sketch open-wire / InternalFace stable selector 队列。下一轮不应回到 CopyOnChange retained known gap，也不应把已经 oracle-blocked 的 DressUp stale `ReferenceShadow` 恢复直接做成实现任务。current capability 显示 DressUp / Hole topo history 已进入 first slice：覆盖了 DressUp AddSubShape slot、multi-selection history、Draft / Thickness 参数变体、Hole `findHoles()` / ModelThread / head-cut / Body subtractive history，但这类 producer history 仍值得用 C10 队列做第二轮 source-backed 复核。

## 实施原则

- 先读 FreeCAD 源码，再写 cad-core 落点；不能从 fixture 输出倒推业务逻辑。
- `Shape`、`NamedShape`、`ElementMap`、mapper history 和 mesh 都是 request-local 产物；不新增跨请求缓存。
- `backendGap` 必须同时有 FreeCAD authority 和 current cad-core mismatch evidence。
- `notCollected` 只触发 oracle / evidence 任务，不直接触发 C++。
- 旧引用恢复必须走 `StableSubList -> ElementMap / MapperHistory / ReferenceShadow` 证据链；不靠 raw `FaceN`、bbox、面积、source index、输出顺序或 fixture 名称。

## S0-S6 拆分

| 步骤 | 目标 | 关键输出 |
| --- | --- | --- |
| S0 | 冻结 live baseline 和声明口径 | README、总入口、状态词典、forbidden claims、validation matrix 与 `C10M2-BLOCKER-000=closed_s0` 对齐。 |
| S1 | 复核 FreeCAD 源码候选和 current coverage | source candidate TSV 回写为真实源码 / cad-core 路径；不升级 supported 口径。 |
| S2 | 做范围准入与 blocker 路由 | scope / blocker / non-goal / backend-gap TSV 全部有 owner step 和 close condition。 |
| S3 | DressUp producer history 专项复审 | Fillet / Chamfer / Draft / Thickness 的 AddSubShape slot、selection、refine、Body / transformed consumption 路由清楚。 |
| S4 | Hole producer history 专项复审 | `findHoles()`、profile-source、ModelThread、head-cut、Body cut history 与 P7 expected / focused tests 对齐。 |
| S5 | 跨特征旧引用恢复与 diagnostic 边界复审 | DressUp / Hole 经 Body、transformed、Link retag 后的 split / deleted / old reference 只在有 evidence 时推进。 |
| S6 | Oracle 实现与发布闸门 | 对已证明 mismatch 的 row 落 C++ / tests；无 mismatch 时发布 no-code gate。 |

## 下一轮代码落点规则

S6 只有在 S3-S5 产生 `backend_gap_candidate` 或 `release_gate` 行时才改代码。允许的落点包括：

- DressUp：`cad-core/src/part_design/feature_dress_up_support.*`、`feature_fillet.cpp`、`feature_chamfer.cpp`、`feature_draft.cpp`、`feature_thickness.cpp`、`body.cpp`。
- Hole：`cad-core/src/part_design/feature_hole.cpp`、`body.cpp`。
- Topo / ElementMap：`cad-core/include/cad_core/part/topo_shape.h`、`cad-core/src/part/topo_shape.cpp`、`cad-core/src/app/element_map.cpp`。
- Capability / tests：`cad-core/src/runtime/capability_contract.cpp`、`cad-core/tests/test_p7_features.py`、`cad-core/tests/test_adapters.py`，必要时新增 `cad-core/fixtures/c10m2` expected。

禁止在 adapter、JSON parser、输出排序、fixture 名称分支或几何相似度匹配中补业务语义。

## 验收分层

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore10.0/C10-M2-PartDesignDressUpHoleTopoHistory批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M2-PartDesignDressUpHoleTopoHistory批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0/C10-M2-PartDesignDressUpHoleTopoHistory批次 docs/CADCore10.0/README.md
git diff --check
```

代码闸门触发后：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_adapters
```

重型收口只在 S6 实际修改 expected、collector、capability 或核心 C++ 后执行；docs-only S0-S2 不跑 cad-core build。
