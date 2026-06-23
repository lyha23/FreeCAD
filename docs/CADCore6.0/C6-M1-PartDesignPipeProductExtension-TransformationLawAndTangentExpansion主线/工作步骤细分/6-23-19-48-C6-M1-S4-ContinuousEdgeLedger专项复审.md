# C6-M1 S4 ContinuousEdgeLedger 专项复审

## 目标

为 `SpineTangent` / `AuxiliarySpineTangent` 定义 request-local continuous-edge expansion 账本，避免靠输出几何猜测、compound 全边收集或 fixture 名称补路径。

## FreeCAD 依据

| 源码 | 证据 | C6 处理 |
| --- | --- | --- |
| `FeaturePipe.h::getContinuousEdges()` | 注释为 “get the given edges and all their tangent ones” | 作为产品意图来源。 |
| `FeaturePipe.cpp::getContinuousEdges()` | 主体整体注释，包含 edge / face continuity 伪代码 | 不能声明 FreeCAD parity；可作为 ledger 字段参考。 |
| `FeaturePipe.cpp::buildPipePath()` | `getContinuousEdges(shape, subedge)` 调用被注释 | C6 必须定义自己的 request-local expansion 规则。 |

## ledger 初稿

| 字段 | 含义 |
| --- | --- |
| `source_object` | spine 或 auxiliary spine owner。 |
| `requested_subnames` | request 中显式选择的 EdgeN。 |
| `expanded_subnames` | ledger 解析后的有序 EdgeN。 |
| `continuity_rule` | `C0BoundaryStops`、`G1Include` 或产品批准的命名。 |
| `adjacency_evidence` | 每条边的前后邻接、共享顶点、continuity 分类。 |
| `rejection_reason` | non-edge、ambiguous branch、disconnected path、closed-loop ambiguity。 |

## 范围

| 项 | 当前裁决 |
| --- | --- |
| selected `Spine` EdgeN expansion | C6 target。 |
| selected `AuxiliarySpine` EdgeN expansion | C6 target。 |
| whole wire / compound without selected EdgeN | baseline supported，不做 tangent expansion。 |
| branch junction | diagnostic，除非产品定义选择策略。 |
| self-intersecting path | diagnostic，不能靠 shape result 后修。 |

## 必须回写的矩阵行

- `C6M1-SCOPE-301`、`C6M1-SCOPE-302`。
- `C6M1-BLK-301`。
- `C6M1-IN-301`。
- `C6M1-ORC-301`、`C6M1-ORC-302`。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'getContinuousEdges|buildPipePath|SpineTangent|AuxiliarySpineTangent' src/Mod/PartDesign/App/FeaturePipe.cpp src/Mod/PartDesign/App/FeaturePipe.h cad-core/src/part_design/feature_pipe.cpp
rg -n 'continuous-edge|expanded_subnames|continuity_rule|C6M1-SCOPE-301|C6M1-BLK-301' docs/CADCore6.0/C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线
```

通过条件：ledger 字段、diagnostic 条件和禁止路径明确；S6 不能再用 compound guessing 实现 tangent。

## 非目标

- 不实现完整 FreeCAD topological neighbor finder parity。
- 不把 tangent expansion 应用于 profile / sections。
- 不把 branch junction 自动选一条路径。
