# C13-M3 MappedName Producer Ledger 前置实现批次

C13-M2 S4 在实现 `mappedName.raw/canonical` codec 时被真实 blocker 卡住：runtime 只能看到 `NamedShape.elementMap` 里的 stable token，例如 `Pad.Edge1`、`ProbeSketch.Edge1`、`Box.Face1`，但 FreeCAD expected 需要 producer 阶段生成的 raw mapped name，例如 `Pocket.#11:2;:H21,V`、`Pad.#10:1;:G;XTR;:H41f:7,E`、`#3d:1;:G;PSM;:H3c3:7,E`。

这些 raw names 不是显示名转换出来的，而是 FreeCAD 在 `TopoShapeExpansion.cpp` 中用 `TopoShape.Tag`、source tag、operation postfix 和 `ElementMap::encodeElementName()` 生成出来的 producer-side ledger。C13-M3 的目标是补这个前置账本，让 C13-M2 S4 可以回到正常 codec/publisher 实现，而不是从 expected 字符串或 fixture 名称反推。

## 当前结论

- C13-M2 S0-S3 已关闭，S3 已把 focused red tests 锁住。
- C13-M2 S4 当前不能安全关闭；缺少的是 `NamedShape` 生产阶段的 FreeCAD-equivalent tag / op / raw mapped-name provenance。
- runtime 仍可继续发布 C13-M1 `topoNamingState`，但不能把 stable token 当作 FreeCAD raw mapped name。
- C13-M3 不替代 C13-M2；它是 C13-M2 S4 的前置补账本批次。

## FreeCAD source authority

| 语义 | FreeCAD source | C13-M3 用法 |
| --- | --- | --- |
| raw name 两段结构与 tag 解析 | `src/App/MappedName.h`, `src/App/MappedName.cpp::MappedName::findTagInElementName()` | 定义 `data + postfix` 和 `;:H<tag>:<len>,<type>` 语义。 |
| name 编码与 hash/dehash | `src/App/ElementMap.cpp::encodeElementName()`, `hashElementName()`, `dehashElementName()` | producer ledger 生成 raw/canonical evidence 的依据。 |
| child key 编码 | `src/App/ElementNamingUtils.h`, `ElementMap::hashChildMaps()`, `addChildElements()` | 后续 S5 child key evidence 的前置字段。 |
| producer 调用链 | `src/Mod/Part/App/TopoShapeExpansion.cpp` | `ensureElementMap()->encodeElementName(... Tag, op, other.Tag)` 是必须迁移的生成点。 |
| mapper relation source | `src/Mod/Part/App/TopoShapeMapper.cpp` | generated / modified relation 仍是 raw name provenance 的 source。 |

## cad-core 落点

| 落点 | 角色 |
| --- | --- |
| `cad-core/include/cad_core/part/topo_shape.h` | 增加 request-local mapped-name provenance / ledger 类型，或扩展 `NamedShape` 承载 source-backed raw name evidence。 |
| `cad-core/src/part/topo_shape.cpp` 与 `topo_shape_expansion.cpp` | 在 maker history、preserved source、generated/modified、upper/lower、combo/child map 传播点记录 tag/op/source provenance。 |
| `cad-core/include/cad_core/topo/` 与 `cad-core/src/topo/` | 放 FreeCAD mapped-name codec/helper，负责 source-backed encode/canonicalize。 |
| `cad-core/src/runtime/topo_naming_state.cpp` | 只消费 producer ledger；无 producer evidence 时保持 indexed-only / no-fake-raw 边界。 |
| `cad-core/tests/test_topo_naming_state_response.py` | 继续作为 focused redline；C13-M3 关闭后应让 C13-M2 S4 可以删除 expectedFailure。 |

## 非目标

- 不从 `fixtures/<phase>/expected/*.freecad.json` 复制 raw mappedName。
- 不按 fixture、phase、object 名称或 expected 字符串做分支。
- 不把 stable token、display path、`fullSubname` 当作 durable raw mapped name。
- 不做全量 topo state expected parity。
- 不改前端 consumer。
- 不改变 CAD Core 无状态边界；producer ledger 仍是单次 recompute 产物，随 response state 输出，不保存 backend session cache。

## 工作步骤

- 入口：`工作步骤细分/7-8-20-59-C13-M3工作步骤总入口.md`
- S0：冻结 C13-M2 S4 blocker 与当前 redline。
- S1：设计 producer ledger 接口、tag/op/provenance 数据模型。
- S2：实现 FreeCAD-equivalent encode helper 与 request-local tag/op 账本。
- S3：接入 PartDesign focused producer 链路，覆盖 p2/c4m6/p6。
- S4：runtime publisher 消费 ledger，p5/p8 no-fake-raw 边界保持。
- S5：发布闸门，回流 C13-M2 S4 或更新 blocker。

## 验收入口

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M3-MappedNameProducerLedger前置实现批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M3-MappedNameProducerLedger前置实现批次/矩阵/*.tsv
git diff --check
```
