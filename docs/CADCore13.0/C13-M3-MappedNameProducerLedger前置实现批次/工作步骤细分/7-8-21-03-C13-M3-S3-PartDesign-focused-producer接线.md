# C13-M3 S3 PartDesign focused producer 接线

## 目标

把 producer ledger 接入 p2 / c4m6 / p6 涉及的 PartDesign producer paths，使 required entries 有 source-backed raw mapped-name evidence。

## 必读文件

- S2 输出
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/src/part_design/feature_pad.cpp`
- `cad-core/src/part_design/feature_pocket.cpp`
- `cad-core/src/part_design/body.cpp`
- focused fixtures and expected

## 操作

1. 接入 maker history / preserved source / generated / modified 传播点。
2. 确认 p2 Body、c4m6 Body、p6 ProbePad 的 required entries 能获得 ledger evidence。
3. 不处理 p5/p8 fake raw；它们仍应保持 indexed-only boundary。

## 关闭条件

- `C13M3-BLOCKER-301` 关闭。
- p2/c4m6/p6 不再只依赖 stable token fallback。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_topo_naming_state_response
```
