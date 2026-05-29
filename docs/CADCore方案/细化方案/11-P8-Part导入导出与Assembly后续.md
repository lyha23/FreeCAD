# P8：Part、导入导出与 Assembly 后续

P8 的目标是在 PartDesign 主体稳定后，扩展更宽的 CAD Core 能力：Part primitives、Boolean、文件导入导出、Assembly Link / Joint，以及 Worker / WASM / Web adapter 产品化。

## 前置条件

- Document / Property / Link / Placement 稳定。
- Topo Naming 主路径稳定。
- PartDesign 常用生态已有 fixture 和 oracle。
- adapter 边界仍保持薄转换，不承载业务语义。

## Part 能力

FreeCAD 参考：

- `src/Mod/Part/App/PartFeature.cpp`
- `src/Mod/Part/App/BodyBase.cpp`
- `src/Mod/Part/App/TopoShape*.cpp`
- `src/Mod/Part/App/FaceMaker*.cpp`
- `src/Mod/Part/App/WireJoiner.cpp`

目标：

- 支持常用 Part primitives。
- 支持 Part Boolean。
- 支持 Shape import/export adapter。
- 支持 Part::Feature 作为 DocumentObject graph 节点。

fixtures：

```text
fixtures/p8/
  part-box.json
  part-cylinder.json
  part-boolean-cut.json
  part-import-shape.json
```

## 文件导入导出

规则：

- 文件导入导出是 adapter / geometry service 能力，不改变持久源数据是 `DocumentObject graph` 的边界。
- STEP / IGES / BREP / STL 可作为输入输出 artifact，但不能变成跨请求隐藏状态。
- import 后若要参与参数化 recompute，必须落成明确 `DocumentObject` 和属性。

候选命令：

```bash
cad-core import-step model.step --output document.json
cad-core export-step document.json --object Body --output Body.step
cad-core export-stl document.json --object Body --output Body.stl
```

## Assembly 后续

FreeCAD 参考：

- `src/Mod/Assembly/App`
- `src/App/PropertyLinks.cpp`
- `src/App/Link*.cpp`

目标：

- 支持 Assembly object graph。
- 支持 Link / Joint 的基础数据模型。
- 支持装配依赖和 recompute 顺序。
- 支持装配约束求解输出到 placement。

暂缓边界：

- Assembly 不应早于 Document / Link / Placement / Topo Naming 稳定。
- 不把 Assembly solver 状态塞进 PartDesign executor。

## Worker / WASM / Web adapter

目标：

- CLI 继续作为 fixture / CI 主入口。
- C ABI 继续作为多语言桥接。
- Worker 用于隔离耗时 recompute。
- WASM / Web adapter 只做协议和运行环境适配。

规则：

- adapter 不解析 FreeCAD feature 语义。
- adapter 不修补 topo naming。
- adapter 不保存跨请求裸 shape。
- 同一 input document 在 CLI / C ABI / Worker / Web 下核心输出一致。

## 完成定义

P8 完成需要同时满足：

- Part / import-export / Assembly 能力都不破坏 CAD Core 无状态边界。
- 文件 artifact 不进入持久 DocumentObject graph，除非显式建成导入对象。
- Assembly 的 Link / Joint / placement 语义和 Document graph 对齐。
- Worker / WASM / Web adapter 通过一致性 fixture，而不是自带业务分支。
