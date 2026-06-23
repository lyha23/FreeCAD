# 【已实现】C6-M1 S4 ContinuousEdgeLedger 专项复审

## 完成结论

- S4 已裁决 `C6M1-SCOPE-301`、`C6M1-SCOPE-302`：`SpineTangent` 与 `AuxiliarySpineTangent` 进入 CAD Core product contract，状态从 `productContractRequired` 推进为 S6 可实现的 `backendGap`。
- FreeCAD 依据只限注释意图：`FeaturePipe.h::getContinuousEdges()` 的 “get the given edges and all their tangent ones”、`FeaturePipe.cpp::getContinuousEdges()` 的整体注释伪代码、以及 `buildPipePath()` 中被注释的 `getContinuousEdges(shape, subedge)` 调用。S4 不声明 FreeCAD parity。
- request-local `continuous_edge_ledger` 字段定稿为：`source_object`、`requested_subnames`、`expanded_subnames`、`continuity_rule`、`adjacency_evidence`、`rejection_reason`；账本只在单次请求内产生，不保存跨请求邻接缓存。
- 已定义 `non-edge`、`disconnected`、`branch`、`closed-loop ambiguity` 诊断边界；branch junction 不自动选择路径，closed loop 必须有可判定的起止 / 方向证据。
- 已禁止 compound all-edge collection、输出几何猜测、按 fixture 名称补路径、以及把 tangent expansion 应用到 profile / sections。
- 已回写 `C6M1-BLK-301`、`C6M1-IN-301`、`C6M1-ORC-301/302` 与 `C6M1-CAT-301`；S4 只定合同和矩阵，不改 C++、fixture、capability。

## 目标

为 `SpineTangent` / `AuxiliarySpineTangent` 定义 request-local continuous-edge expansion 账本，避免靠输出几何猜测、compound 全边收集或 fixture 名称补路径。

## FreeCAD 依据

| 源码 | 证据 | C6 处理 |
| --- | --- | --- |
| `FeaturePipe.h::getContinuousEdges()` | 注释为 “get the given edges and all their tangent ones” | 只作为产品意图来源，不代表 active native path。 |
| `FeaturePipe.cpp::getContinuousEdges()` | 主体整体注释，包含 edge / adjacency / continuity 伪代码 | 只能支撑 request-local ledger 字段和诊断维度；不能声明 FreeCAD parity。 |
| `FeaturePipe.cpp::buildPipePath()` | `if (SpineTangent.getValue()) getContinuousEdges(shape, subedge);` 被注释 | C6 必须定义自己的 request-local expansion 规则，S6 才能替换 source-blocked diagnostic。 |

## Ledger 合同

| 字段 | 含义 |
| --- | --- |
| `source_object` | `Spine` 或 `AuxiliarySpine` 的 link owner；expanded edge 必须来自同一个 source object。 |
| `requested_subnames` | request 中显式选择的 `EdgeN` 列表；空 sublist、whole Edge / Wire / Compound 只走 baseline path build，不触发 tangent expansion。 |
| `expanded_subnames` | 从 `requested_subnames` 出发按 continuity ledger 得到的有序 `EdgeN`，去重且保持可复现顺序。 |
| `continuity_rule` | C6-M1 固定为 `G1Include_C0BoundaryStops`：只纳入可证明 tangent-continuous 的相邻边，遇到 C0 边界或证据不足即停止。 |
| `adjacency_evidence` | 每次纳入 / 拒绝相邻边都记录共享顶点、候选方向、continuity 分类、source EdgeN 与 decision。 |
| `rejection_reason` | `non_edge_subname`、`disconnected_path`、`ambiguous_branch_junction`、`closed_loop_ambiguity` 或 `insufficient_adjacency_evidence`。 |

## 诊断边界

| 场景 | 裁决 |
| --- | --- |
| non-edge subname | 报 locatable diagnostic；不得把 Vertex / Face / 空 subname 当成 EdgeN 扩张。 |
| disconnected path | 报 `disconnected_path`；不得用输出 wire 结果或 compound 顺序补桥。 |
| branch junction | 报 `ambiguous_branch_junction`，除非未来产品 spec 明确选择策略；S4 不自动选分支。 |
| closed-loop ambiguity | 报 `closed_loop_ambiguity`，除非 requested edge 和 adjacency evidence 能确定唯一方向与完整环路。 |
| whole compound | baseline build 可收集 compound 内 edge / wire，但 tangent ledger 禁止 compound all-edge expansion。 |

## 范围

| 项 | 当前裁决 |
| --- | --- |
| selected `Spine` EdgeN expansion | C6 product contract approved；S6 backendGap。 |
| selected `AuxiliarySpine` EdgeN expansion | C6 product contract approved；S6 backendGap，账本必须标注 auxiliary source。 |
| whole wire / compound without selected EdgeN | baseline supported，不做 tangent expansion。 |
| profile / sections | 非目标；不应用 continuous-edge expansion。 |
| branch junction | diagnostic，除非产品定义选择策略。 |
| self-intersecting path | diagnostic，不能靠 shape result 后修。 |

## 已回写的矩阵行

- `C6M1-SCOPE-301`、`C6M1-SCOPE-302`：从 `productContractRequired` 推进为 `backendGap`。
- `C6M1-BLK-301`：close condition 补齐 ledger 字段、branch / disconnected / non-edge / closed-loop diagnostics 和禁止路径。
- `C6M1-IN-301`：定稿 request / response 字段和 prohibited fields。
- `C6M1-ORC-301`、`C6M1-ORC-302`：升级为 `contractApprovedPendingImplementation`，S6 再新增 fixture / tests。
- `C6M1-CAT-301`：continuous-edge ledger 分类改为 backend gap。

## 验收标准

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'getContinuousEdges|buildPipePath|SpineTangent|AuxiliarySpineTangent' src/Mod/PartDesign/App/FeaturePipe.cpp src/Mod/PartDesign/App/FeaturePipe.h cad-core/src/part_design/feature_pipe.cpp
rg -n 'continuous-edge|expanded_subnames|continuity_rule|C6M1-SCOPE-301|C6M1-BLK-301|branch|disconnected|closed-loop|compound all-edge' docs/CADCore6.0/C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线
for f in docs/CADCore6.0/C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
git diff --check -- docs/CADCore6.0
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线/工作步骤细分 --format markdown
```

通过条件：ledger 字段、diagnostic 条件和禁止路径明确；S6 不能再用 compound guessing 实现 tangent。S4 文件名和标题标记为 `【已实现】` 后，队列显示 S0-S4 已完成、S5-S6 待执行。

## 非目标

- 不写 C++。
- 不新增 fixture。
- 不更新 capability。
- 不实现完整 FreeCAD neighbor finder parity。
- 不把 tangent expansion 应用于 profile / sections。
- 不把 branch junction 自动选一条路径。
