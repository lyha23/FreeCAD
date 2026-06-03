# CAD Core 2.0 方案入口

本目录定义 `cad-core` 从当前 P0-P8 基础子集继续推进到 CAD Core 2.0 的实施方案。2.0 的目标不是继续堆 feature 数量，而是把当前最深的 FreeCAD 语义缺口收敛成可维护主路径：完整引用更新、完整 MapperHistory、ExternalGeometry 状态机、FaceMaker / WireJoiner history 消费，以及 Link / Assembly / adapter 的产品化边界。

## 当前判断

当前 `cad-core` 已具备独立 C++17 / CMake Core、CLI adapter、薄 C ABI adapter、FreeCAD 风格 `DocumentObject graph` 输入、单次 recompute 输出、mesh / subshape / `NamedShape` / diagnostics、P0-P8 基础 feature 覆盖和 300+ 测试回归。它已经不是 MVP，但还不是完整 FreeCAD 几何内核抽取版。

按可用工程能力估算，当前约完成 65%-70%；按接近完整 FreeCAD 几何 / 拓扑语义 parity 估算，约完成 45%-50%。差距主要集中在深层引用和 history 生命周期，而不是单个几何 API。

## C2-M0 / C2-M1 当前基线

当前 C2-M0 基线已锁定：进入任务时 `git status --short` 无输出；`cad-core` 构建通过；指定回归命令覆盖 314 个测试，结果 `OK (skipped=9)`。capabilities 仍显式暴露 2.0 剩余缺口，包括 FaceMaker / WireJoiner 完整 history producer、taper 完整 history、transformed / pattern 完整 history、导入 shape ElementMap 和 assembly solver。

C2-M1 已在 `cad-core/topo` 建立统一 `MapperHistory` core：新增统一 event schema，表达 source / target endpoint、shape kind、relation、maker stage、evidence、recoverability 和 diagnostic status；`NamedShape` JSON 保留旧 `history` / `element_history_status`，同时新增 `mapper_history` 字段。现有 maker history、preserved ElementMap alias、terminal split / deleted、merge、Link retag、transformed copy 和 Sketch InternalShape 的 FaceMaker / WireJoiner summary 子集已通过统一入口序列化或转换。

当前仍非目标：不展开 C2-M2 的 FaceMaker / WireJoiner 完整 producer 迁移，不实现 C2-M3 ExternalGeometry 完整状态机，不收敛 C2-M5+ PartDesign 全量 history。

## 文档索引

| 文档 | 用途 |
| --- | --- |
| `00-总览.md` | 2.0 目标、边界、阶段拆分和推进顺序 |
| `01-P5P6-ExternalGeometry-TopoNaming主线.md` | ExternalGeometry、MapperHistory、FaceMaker / WireJoiner、旧引用恢复主线 |
| `02-P6P7-History-PartDesign收敛.md` | ShapeFix / Refine / taper / transformed / DressUp / PartDesign ownership 收敛 |
| `03-P8-Link-Assembly-Adapter产品化.md` | Link 账本、ShowElement 写回生命周期、Assembly solver、Worker / WASM / Web adapter |
| `04-验收矩阵与交付规则.md` | fixture / oracle / diagnostics / 回归命令 / 完成判定 |

## 执行原则

- 本地 FreeCAD `src/` 是语义来源；不从 fixture 输出倒推业务逻辑。
- `DocumentObject graph` 是唯一持久源数据；shape、mesh、`NamedShape`、`ElementMap` 是单次 recompute 产物。
- adapter 只做协议转换，不承载建模语义或引用恢复。
- P5/P6 的 MapperHistory 与引用恢复是 2.0 的前置主线；在它完成前，不继续扩大高层 executor 的特判。
- 每个新增能力必须有 FreeCAD 调用链、cad-core 落点、fixture / oracle 或明确 diagnostics。
