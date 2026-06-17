# P5P6-S5 FaceMaker / WireJoiner history producer 专项复审

## 目标

P5P6-S5 单独裁决 FaceMaker / WireJoiner 的 history producer 主路径。它要把当前 summary-only、geometry-match 和 fixture-shaped fallback 收束到正式 producer / mapper / resolver 链条。

## FreeCAD 依据

- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/FaceMaker.cpp::FaceMaker::postBuild()`
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp`
- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoiner::getOpenWires()`

关键源码账本：

- `FaceMakerBuildFace` 的 `myPreSplitHistory`。
- `FaceMaker` 的 `mySplitter`、`myShapesToReturn` 和 `postBuild()`。
- `WireJoinerP::EdgeInfo`、`WireInfo`、`wireInfo2`、`iteration` / `iteration2`。
- `superEdge`、`openWireCompound`、`BRepTools_History aHistory`。

## 复审范围

| 范围 | P5P6 状态草案 | 收口说明 |
| --- | --- | --- |
| FaceMaker pre-split history producer | `backendGap` | self-intersection / pre-split 必须变成 history event。 |
| FaceMaker splitter history producer | `backendGap` | splitter generated/modified/deleted 进入 topo，而不是 sketcher 猜。 |
| WireJoiner noOriginal filtering | `backendGap` 或复核 | 原始 open edge 过滤、split fragment 保留要有 producer evidence。 |
| WireJoiner EdgeInfo / WireInfo ownership | `backendGap` | owner slot、openWireCompound、split/deleted 需要正式 history。 |
| Sketch InternalShape consumer switch | `backendGap` | consumer 只能消费 producer events 和 resolver 结果。 |
| geometry-match internal map fallback | `releaseGate` | 主路径切换后必须删除或标明临时边界。 |

## 必须回写的矩阵行

- `P5P6-SCOPE-003`：FaceMakerBuildFace producer。
- `P5P6-SCOPE-004`：WireJoiner producer。
- `P5P6-SCOPE-011`：Sketch InternalShape main path。
- `P5P6-SCOPE-012`：fallback deletion。
- 对应 blocker：`P5P6-BLOCK-003`、`004`、`011`、`012`。

## 实现纪律

实施顺序固定：

```text
FaceMaker / WireJoiner producer evidence
  -> MapperHistory event
  -> ElementMap / diagnostics consumption
  -> Sketch InternalShape consumer switch
  -> fallback deletion
```

不得直接在 `cad-core/src/sketcher/sketch_object.cpp` 中按 fixture 名、几何类型、source index、split 顺序或输出排序补规则。

## 验收场景

- FaceMaker bounded / self-intersection。
- open wire `noOriginal=true`。
- dangling open line terminal deleted。
- through open cutter split fragments。
- one-source-to-many InternalEdge split。
- InternalFace outer-boundary generated history。
- ambiguous split diagnostic。

## 非目标

- 不扩大为完整 Sketcher solver。
- 不把 InternalFace 命名顺序差异当硬失败。
- 不在 adapter 或导出层做 output pruning。
- 不让 FaceMaker / WireJoiner summary 永久替代正式 producer。
