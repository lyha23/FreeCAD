# C4-M3 ExternalGeometry / InternalShape 压力方案

## 目标

补 ExternalGeometry、InternalShape、FaceMaker / WireJoiner 和 topo naming 交叉压力包，保护 open profile、internal face、reference recovery 和 stable subname 语义。

## 范围

- FreeCAD 源码依据：`src/Mod/Sketcher/App/SketchObjectExternal.cpp`、`SketchObject.cpp`、`src/Mod/Part/App/FaceMaker*.cpp`、`WireJoiner.cpp`、`TopoShape*.cpp`。
- cad-core 落点：`cad-core/src/sketcher`、`cad-core/src/part/face_maker.cpp`、`cad-core/src/part/wire_joiner.cpp`、`cad-core/src/part/topo_shape*`。
- 验收：`tests.test_p5_sketch`、`tests.test_p6_topology`、`tests.test_adapters`。

## 阶段

| 步骤 | 内容 |
| --- | --- |
| S0 | ExternalGeometry lifecycle 与 InternalShape source audit |
| S1 | pressure fixture / expected / diagnostics |
| S2 | history propagation 或 explicit deferred 收口 |

## 非目标

- 不在 `sketch_object.cpp` 里猜测 split history。
- 不把 open profile 强行混回 FreeCAD 风格空 `InternalShape` 之外的语义。
- 不用输出端修剪替代 FaceMaker / WireJoiner / ElementMap 账本。
