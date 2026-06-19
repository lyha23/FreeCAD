# C4-M2 PartDesign Feature Family 总览入口

## 目标

C4-M2 只收仓库目标内、前端 CAD 运行时需要的 PartDesign feature family。总览包负责阶段边界，具体执行拆到 Revolution/Groove 与 Loft/Pipe/Boolean/Datum 两个专题包。

## 子包

| 专题包 | 入口 | 队列 |
| --- | --- | --- |
| Revolution / Groove 审计主线 | `docs/CADCore4.0/C4-M2-PartDesign-RevolutionGroove审计主线/6-20-00-23-C4-M2-RevolutionGroove主线总入口.md` | `docs/CADCore4.0/C4-M2-PartDesign-RevolutionGroove审计主线/工作步骤细分/` |
| Loft / Pipe / Boolean / Datum 主线 | `docs/CADCore4.0/C4-M2-PartDesign-LoftPipeBooleanDatum主线/6-20-00-24-C4-M2-LoftPipeBooleanDatum主线总入口.md` | `docs/CADCore4.0/C4-M2-PartDesign-LoftPipeBooleanDatum主线/工作步骤细分/` |

## 验收口径

具体实现由子包给出 FreeCAD 调用链、oracle fixture、Body Tip replacement、topo history 和 adapter/capability 验收；总览包只维护 owner 索引。
