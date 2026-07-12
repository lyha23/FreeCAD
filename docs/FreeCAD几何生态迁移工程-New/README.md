# FreeCAD 几何生态迁移工程 New

本工程包只在 public/ledger 行为无法对齐且公开输出不足以解释原因时，才把差异拆成 producer 过程问题。public/ledger 一致性是先行且最终的产品裁决；N1/N2/N3 trace 链只是按需定位工具，不是普通一致性评价的前置阶段或第三个权威。

## 当前基线

- live baseline：2026-07-12，`/Users/li/Chili3DProject/FreeCAD` HEAD `05f14b6b0c`。
- 当前 worktree 已有大量用户改动，包括 `cad-core` 的 StringHasher、ElementMap、TopoShape、Sketch、PartDesign 与 C4M6 产物；本工程实施时必须先重新记录 live status，只复用和增量修改，不得回退或覆盖。
- FGM-N1 已完成 request-local recorder、默认 CAD Core trace sidecar、required slices 与共享闭包 validator；FGM-N2 comparator/CLI/report/parity 观察性集成已通过 focused 和 `/tmp` 代表验证。
- native corpus 当前有 480 对 public expected/ledger，它们构成一致性权威。另有 480 个 native `*.freecad.producer-trace.json` 诊断 sidecar；无论其跟踪、closure 或确定性状态如何，都不构成第三个一致性权威。
- N2 尚未标记 `【已实现】`：默认 fixture tree 没有 CAD Core actual trace；现有 480 个 native trace 虽都有 request/response binding，但没有可重算的 canonical snapshot hash，且 native mapper/child snapshot 尚未发布 raw M/G/D 与 parent inventory/nested linkage。8 个代表 family 已用 `/tmp` 新采的 canonical-bound native trace 验证；probe contract 与正式 artifact 更新需另行授权。

## 裁决与参考顺序

1. public expected 与 strict ledger 是一致性验收权威；当前 CAD Core live public output 与它们比较。
2. 只有 public/ledger red 且需要定位内部原因时，才参考 FreeCAD 源调用链、producer trace 和探针规格。
3. `docs/框架/7-12-00-46-FreeCADCmd-ElementMap生产者Trace驱动CADCore实现指南.md` 只规定如何使用参考证据，不新增一致性门禁。
4. 本工程包负责诊断和修复，不得用 trace verdict 改写 public/ledger verdict。

## 工程分期

| 阶段 | 目标 | 产物 | 进入条件 |
| --- | --- | --- | --- |
| `FGM-N1` | 为按需诊断提供 CAD Core ElementMap producer trace | `<case>.cad-core.producer-trace.json`、闭包 validator、focused tests | public/ledger red 需要内部定位，或明确 trace 审计 |
| `FGM-N2` | 比较 native 与 CAD Core trace，报告第一处分叉 | `compare_element_map_producer_trace.py`、机器/人类诊断报告 | 已触发 trace 诊断且 N1 publisher 闭合 |
| `FGM-N3` | 按首处分叉把缺失语义放回 CAD Core 同构模块 | producer-family 修复批次，再回到 public/ledger 验收 | N2 对目标 red case 能稳定定位 |

必读文件：

| 文件 | 用途 |
| --- | --- |
| `FGM-N1-CADCore-ElementMap生产者切片探针/7-12-01-11-FGM-N1-CADCore-ElementMap生产者切片探针方案.md` | 首批完整实施规格；必须先完成 |
| `FGM-N1-CADCore-ElementMap生产者切片探针/矩阵/producer_trace_slice_matrix.tsv` | 全部 slice 与 CAD Core 落点，防止只做 recorder 骨架 |
| `FGM-N1-CADCore-ElementMap生产者切片探针/矩阵/producer_trace_blocker_queue.tsv` | N1 有序关闭队列 |
| `FGM-N2-ProducerTrace首分叉比较器/7-12-01-11-FGM-N2-ProducerTrace首分叉比较器方案.md` | 结构对齐、canonical projection 与首分叉报告规格 |
| `FGM-N3-按首分叉推进几何生态迁移/7-12-01-11-FGM-N3-按首分叉推进几何生态迁移方案.md` | 比较器就绪后的 producer-family 迁移循环 |

## 数据方向

```mermaid
flowchart LR
    I["fixture graph + optional topoNamingState"] --> F["producer-enabled FreeCADCmd"]
    I --> C["CAD Core"]
    F --> FE["freecad.json"]
    F --> FL["freecad.ledger.json"]
    F --> FT["freecad.producer-trace.json"]
    C --> CR["cad-core.json"]
    C --> CT["cad-core.producer-trace.json"]
    FT --> D["first-divergence comparator"]
    CT --> D
    FE --> P["public parity/release gate"]
    FL --> P
    CR --> P
    D --> T["producer scope + checkpoint + field path"]
```

trace 永远是只读、按需参考的诊断 sidecar。它不得进入 fixture 输入、Shape/BREP、普通 response、`topoNamingState`、public expected comparator 或 CAD Core 的建模决策；public/ledger green 时不要求运行 N1/N2/N3。

## 与既有路线的关系

- C13-M3 已建立 `MappedNameProvenance` 等业务账本；N1 只观察并 snapshot 这些账本，不复制第二份业务状态。
- C13-M4/M5 继续负责 public expected/ledger 与 release parity；N2 是诊断工具，不改变其 verdict。
- C4N-S3 的 deterministic producer-tag 仍是业务语义任务。N1/N2 不宣称替代它，但涉及相同 `string_hasher/topo_shape` 文件时不得并行落代码；先由 N1 固化观察证据，再用 N2 的第一处分叉恢复 C4N-S3。
- `runtime` 和 adapter 只能调度/发布已存在的 trace artifact，不能在 response 阶段补造 producer event。

## 工程关闭规则

- 每阶段按各自方案的短跑、阶段回归和重型收口验收，不能以“文件已生成”代替语义闭包。
- 方案未完成前保持当前文件名；全部实现并验证后，保留时间前缀并重命名为 `【已实现】...`，同步更新矩阵状态和本 README。
- 不手改任何 `*.freecad.json`、`*.freecad.ledger.json`、`*.freecad.producer-trace.json` 追随 CAD Core；native artifact 只能由相应 collector 重生。
