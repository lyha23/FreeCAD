# C12-M11 Sketch Internal Edge Subshape / Mesh Contract 批次总入口

本文是草图边 response contract 的 C12-M11 主入口。它回答一个具体问题：如何让后端稳定返回草图边 `subshapes[]` 与 `mesh.edgeSegments[]`，使前端提交草图后不仅能保留面，也能保留边引用。

C12-M11 是用户单独指定的方案包；它不关闭 C12-M10，也不改写 C12-M10 的 pending queue 状态。

## 主线目标

- 把 FreeCAD `SketchObject::buildShape()` / `buildInternals()` / `getInternalElementMap()` 的边命名链条记录成 CAD Core source authority。
- 复核当前 `cad-core` 是否已经把 `InternalShape` 的 edgeSegments 和 subshapes 一起发布。
- 明确 response contract：`mesh.edgeSegments[].indexed` 必须能在 `subshapes[].indexed` 中找到同名项。
- 明确稳定性层级：request-local `InternalEdgeN -> EdgeN` 与 FreeCAD-grade geometry id mapped name 分开验收。
- 根据实际 current response 分流到 backend implementation、frontend consumer sync 或 geometry id follow-up。

## 证明链条

```text
FreeCAD SketchObject source authority
  -> raw Shape EdgeN / VertexN mapped names
  -> InternalShape InternalEdgeN / InternalVertexN publication
  -> internal element map InternalEdgeN <-> EdgeN
  -> cad-core mesh edgeSegments and subshapes same topology source
  -> recompute response object-qualified id and stableSubname
  -> frontend selection persistence consumes backend token without guessing
```

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| README | `README.md` | 当前定位、口径和入口。 |
| 方案 | `7-1-02-57-C12-M11-SketchInternalEdgeSubshapeMeshContract批次方案.md` | 批次规则、FreeCAD 调用链、CAD Core 落点和验收分层。 |
| 工作步骤总入口 | `工作步骤细分/7-1-02-58-【已实现】C12-M11工作步骤总入口.md` | goal 队列索引，已关闭。 |
| S0 | `工作步骤细分/7-1-02-59-C12-M11-S0-live基线与并行开包冻结.md` | 冻结 baseline 和 C12-M10 pending 关系。 |
| S1 | `工作步骤细分/7-1-03-00-C12-M11-S1-FreeCAD与cad-core-source复核.md` | 复核 FreeCAD / cad-core source authority。 |
| S2 | `工作步骤细分/7-1-03-01-C12-M11-S2-current-response-contract复核.md` | 验证 current edgeSegments/subshapes 对齐。 |
| S3 | `工作步骤细分/7-1-03-02-C12-M11-S3-contract-gap分流裁决.md` | 裁决 backend/frontend/stable-id 缺口。 |
| S4 | `工作步骤细分/7-1-03-03-C12-M11-S4-implementation最小语义批次.md` | 定义实现包或同步包的最小范围。 |
| S5 | `工作步骤细分/7-1-03-04-C12-M11-S5-发布闸门与后续分流.md` | 发布最终状态。 |
| 矩阵 | `矩阵/` | source、contract、gap、blocker、non-goal、validation。 |

## 当前状态

- 工作步骤总入口已关闭：已核对 C12-M11 包结构、入口 + S0-S5 队列顺序和 6 个 TSV 字段数。
- 后续队列从 S0 `工作步骤细分/7-1-02-59-C12-M11-S0-live基线与并行开包冻结.md` 开始。
- 本次关闭只处理入口索引，不执行 S0-S5 实质裁决，不改变 C12-M10 pending 队列。

## 执行规则

1. 每步开始前执行 live baseline：`pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git -c core.quotepath=false status --short -uall`。
2. 执行前刷新 C12-M11 队列；只处理当前第一条未完成 step。
3. C12-M10 仍 pending 时，不得把 C12-M11 写成自然下一包；只按用户单独授权主题推进。
4. S1 必须引用 FreeCAD source 和 `cad-core` current landing，不得从旧 memory 或 fixture 输出倒推语义。
5. S2 只验证 response contract；不因前端消费失败直接改 backend。
6. S3 之后才能决定是否打开 production code 修改；若当前 backend 已支持，后续应转为前端消费同步或 stable geometry id follow-up。
7. 每步完成后重命名为 `【已实现】` 并更新 README / 总入口 / 矩阵状态。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M11-SketchInternalEdgeSubshapeMeshContract批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M11-SketchInternalEdgeSubshapeMeshContract批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M11-SketchInternalEdgeSubshapeMeshContract批次 docs/CADCore12.0/README.md
git diff --check
```
