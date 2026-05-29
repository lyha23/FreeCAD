# P8：Part、导入导出与 Assembly 后续

P8 已启动 Part primitive 基础子集。当前先把 FreeCAD `Part::Primitive` 中最基础、对前端展示和后续 Boolean 有价值的 shape 接入 `cad-core`，导入导出、Assembly 和产品化 adapter 仍保持后置。

## 当前基线

- `Part::Box` 已注册 executor，按 `Length` / `Width` / `Height` 构造 OCCT solid，默认值和过小尺寸校验对齐 FreeCAD `Box::execute()`。
- `Part::Cylinder` 已注册 executor，按 `Radius` / `Height` / `Angle` 构造圆柱底面，再按 `PrismExtension` 的 `FirstAngle` / `SecondAngle` 拉伸成 solid。
- Part primitive 输出 mesh、subshape map、bbox、volume、kernel metadata 和 indexed `NamedShape`。
- `fixtures/p8` 已覆盖 `part-box`、`part-cylinder` 和 `part-cylinder-angled-prism`。

## 目标范围

- Part primitives：Box、Cylinder 已接入；Sphere、Cone、Torus 等仍待补。
- Part Boolean：Fuse、Cut、Common、Section、Fragments 等独立 Part 操作。
- 文件导入导出：STEP / BREP / STL 等 adapter 能力。
- Assembly：Link、Joint、约束求解和装配 recompute。
- 产品化 adapter：Worker、WASM、Web service bridge。

## 边界

- 文件导入导出可以处理 BREP，但 BREP 不进入持久 `DocumentObject graph` 的默认状态模型。
- Web / Worker / WASM 只做 adapter，不改变 CAD Core 无状态边界。
- Assembly 不应绕过 topo naming；Link / Joint 的 subname 和 placement 仍需要稳定引用模型。
- 当前 Part primitive 只使用 indexed `NamedShape`；完整 primitive maker history、Boolean history 和导入 shape 的 ElementMap 仍属于 P6/P8 后续工作。

## 前置条件

- P6 MapperHistory、split / merge 旧引用恢复和 ShapeFix / Refine history 足够稳定。
- P5 Sketcher external geometry 和 internal element map 能支撑常用引用。
- P7 Body 生态不再依赖高层 fixture 特判。
- CLI / C ABI 对同一 fixture 的核心结果一致。

## 规划落点

| 能力 | cad-core 落点 |
| --- | --- |
| Part primitives | 当前落在 `features/part.cpp`，后续复杂 primitive 可拆到 `geometry/primitives.*` |
| Part Boolean | `features/part_boolean.*` + `topo/named_shape.*` |
| Import / Export | `adapters/` 和可选 `geometry/io.*` |
| Assembly Link / Joint | `features/assembly_*`、`document/` link 扩展、`graph/` |
| Worker / WASM / Web | adapter 层 |

## 剩余缺口

- Sphere、Cone、Torus、Plane、Line、Vertex 等 Part primitive 尚未迁移。
- Part Boolean 尚未注册 executor，不能替代 Body fuse/cut 主链。
- STEP / BREP / STL 导入导出还没有 adapter 能力。
- Assembly Link / Joint、placement chain 和装配求解未迁移。
- Worker / WASM / Web service bridge 未产品化。

## 验收

- 每个 Part / Assembly `TypeId` 有明确 executor 或 diagnostics。
- 文件导入导出不污染无状态核心边界。
- Link / Joint placement 和 stable subname 不靠前端猜测。
- Worker / WASM / Web adapter 与 CLI / C ABI 复用同一 core recompute。
