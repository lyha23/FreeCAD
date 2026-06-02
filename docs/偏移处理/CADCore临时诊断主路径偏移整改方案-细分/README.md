# CADCore 临时诊断主路径偏移整改方案（收敛版）

生成日期：2026-06-02

本包是一整套新的可收敛方案，目标是替换原目录 `docs/偏移处理/CADCore临时诊断主路径偏移整改方案-细分/` 中围绕 M3 不断扩散的写法。它不修改 GitHub 仓库，只提供可下载的替代方案文档。

## 文件说明

- `00-总览-收敛原则与重排.md`：说明为什么原方案不收敛，以及新的总体架构和里程碑顺序。
- `01-M3-generated-result-wire-identity-替换方案.md`：可直接替换原 `M3-generated-result-wire-identity.md` 的新版内容。
- `02-有限状态机与blocker枚举.md`：核心数据结构、状态机和 blocker enum 设计。
- `03-实施切片与文件落点.md`：按 `wire_joiner.h` / `wire_joiner.cpp` / `test_p5_sketch.py` 拆分可执行任务。
- `04-验收矩阵与回归命令.md`：阶段验收、最终验收、重点 fixture 和测试命令。
- `05-旧计数器映射表.md`：把旧 helper/generated 诊断字段映射到有限状态和 blocker，避免继续新增无限细分字段。
- `CADCore临时诊断主路径偏移整改方案-收敛版-合并版.md`：以上内容合并后的单文件版本。

## 使用方式

1. 先阅读 `00-总览-收敛原则与重排.md`，确认里程碑重排。
2. 用 `01-M3-generated-result-wire-identity-替换方案.md` 替代原 M3 文档内容。
3. 执行 `03-实施切片与文件落点.md` 中的 P0 到 P7；不得跳过 M2S source-shape identity gate。
4. 用 `04-验收矩阵与回归命令.md` 作为唯一收敛判据。

## 新方案核心结论

原 M3 的不收敛根因不是某个计数器没有继续细分，而是“诊断字段清零”“M2 source-shape identity”“M3 result-wire producer”“M4 history 消费”被混成了同一个完成条件。新方案将输出来源改为 `ResultWireProducerIdentity`，把所有旧 helper/generated 余额压缩到有限状态机和单一 `ResultWireBlocker` 枚举中；新增诊断字段不再是推进方式，未知情况必须作为 `UnknownInvariant` 失败，而不是继续发明新字段。
