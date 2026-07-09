# C13-M3 MappedName Producer Ledger 前置实现批次

C13-M2 S4 在实现 `mappedName.raw/canonical` codec 时被真实 blocker 卡住：runtime 只能看到 `NamedShape.elementMap` 里的 stable token，例如 `Pad.Edge1`、`ProbeSketch.Edge1`、`Box.Face1`，但 FreeCAD expected 需要 producer 阶段生成的 raw mapped name，例如 `Pocket.#11:2;:H21,V`、`Pad.#10:1;:G;XTR;:H41f:7,E`、`#3d:1;:G;PSM;:H3c3:7,E`。

这些 raw names 不是显示名转换出来的，而是 FreeCAD 在 `TopoShapeExpansion.cpp` 中用 `TopoShape.Tag`、source tag、operation postfix 和 `ElementMap::encodeElementName()` 生成出来的 producer-side ledger。C13-M3 已补齐这个前置账本，让 C13-M2 S4 可以回到正常 codec/publisher 收口，而不是从 expected 字符串或 fixture 名称反推。

## 当前结论

- C13-M3 S0 已关闭：`pwd=/Users/li/Chili3DProject/FreeCAD`，起点 `HEAD=095777f8ab`（`095777f8ab 文档：关闭 C13-M3 工作步骤入口`），起点 worktree clean。
- C13-M2 S0-S3 已关闭，S3 已把 focused red tests 锁住。
- C13-M2 S4 的前置 producer-ledger blocker 已解除；C13-M2 S4 本身仍保持 open，由 C13-M2 队列继续执行和关闭。
- C13-M3 S1 已关闭：`NamedShape` 现在有 request-local `mapped_name_provenance` JSON 出口，按 entry/stable element name 承载 tag/sourceTag/op/raw/canonical/status 接口；runtime 发布仍在 S4。
- C13-M3 S2 已关闭：`cad_core::topo` 提供 FreeCAD mapped-name codec/helper，可从完整 `MappedNameProvenance` source/tag/op/type evidence 编码 raw/canonical，并在缺 evidence 时返回 missing/blocker status；runtime 发布仍在 S4。
- C13-M3 S3 已关闭：shared Part/PartDesign maker-history 和 preserved-source alias 写入 source-backed producer evidence；focused debug recompute 已覆盖 p2 `Body`、c4m6 `Body`、p6 Body-side UpToFace source path。p6 `ProbePad` 的 runtime publication 仍留给 S4。
- C13-M3 S4 已关闭：runtime 只消费 source-backed producer ledger；无 ledger evidence 不发布 fake raw；p2 `rect-pad-pocket` / `Body`、c4m6 `topo-state-body-tip-stable-recovery` / `Body` 的 `mappedName.raw/canonical` parity 普通通过；p5 `sketch-internal-face` / `Sketch` 与当前 p8 `app-link-box` / `BoxLink` 保持 indexed-only no-fake-raw 普通通过。S4 提交 `f385c2b5e5 fix: 关闭 C13-M3 S4 runtime 消费`，`tests.test_topo_naming_state_response` 为 `Ran 15 OK` 且无 expectedFailure；adapter channel 单测为 `Ran 1 OK`。
- S4 修复了 producer tag 的 adapter 通道漂移：`requestLocalProducerTagForShape()` 不再使用 `std::hash<TopoDS_Shape>` 这类进程局部 identity，改用当前 producer shape 的稳定 topology / geometry fingerprint 作为 request-local tag evidence。
- 旧 focused fixture `p6/up-to-face-stable-body-history` 与 `p8/app-link-box-face` 已在 `718267783c chore: 刷新 FreeCAD expected 账本基线` 中作为已迁移或废弃 fixture 删除；S4 不手工恢复或手改 expected。若后续重新纳入 p6 `ProbePad`，必须重新采集 native expected 后另行排期。
- runtime 仍可继续发布 C13-M1 `topoNamingState`，且不再把 stable token 当作 FreeCAD raw mapped name。
- C13-M3 S5 已关闭 `C13M3-BLOCKER-501`：C13-M3 队列清空；C13-M2 S4 可恢复执行，S5/S6 仍按 C13-M2 队列处理。C13-M3 不替代 C13-M2，也不把 `childElementMapKey` / `mapperHistoryIds` 标成支持。

## S0 关闭结果

- C13-M2 S3 已有 5 个 guarded `unittest.expectedFailure` redline：p2 `Body`、c4m6 `Body`、p6 `ProbePad` 的 raw/canonical parity，以及 p5 `Sketch`、p8 `BoxLink` 的 indexed-only no-fake-raw 边界。
- C13-M2 S4 的历史 blocker 是缺少 `TopoShape.Tag` / `ElementMap::encodeElementName()` 等 producer evidence；该前置 blocker 已由 C13-M3 S1-S4 解除。C13-M2 S4 仍保持 open，后续应按 C13-M2 步骤验证并正式关闭。
- S0 只冻结 blocker、scope 和验证入口；不改 `cad-core` 代码、tests、fixtures、expected，也不关闭 S1-S5。

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
| `cad-core/tests/test_topo_naming_state_response.py` | 作为 focused green regression evidence；C13-M2 S4 后续应据此正式收口或记录 narrowed blocker。 |

## 非目标

- 不从 `fixtures/<phase>/expected/*.freecad.json` 复制 raw mappedName。
- 不按 fixture、phase、object 名称或 expected 字符串做分支。
- 不把 stable token、display path、`fullSubname` 当作 durable raw mapped name。
- 不做全量 topo state expected parity。
- 不改前端 consumer。
- 不改变 CAD Core 无状态边界；producer ledger 仍是单次 recompute 产物，随 response state 输出，不保存 backend session cache。

## 工作步骤

- 入口：`工作步骤细分/7-8-20-59-【已实现】C13-M3工作步骤总入口.md`
- S0：冻结 C13-M2 S4 blocker 与当前 redline。
- S1：已关闭，落地 producer ledger 接口、tag/op/provenance 数据模型。
- S2：已关闭，落地 FreeCAD-equivalent encode helper 与 request-local tag/op evidence 编码边界。
- S3：已关闭，接入 PartDesign focused producer 链路，覆盖 p2/c4m6/p6 producer evidence。
- S4：已关闭，runtime publisher 消费 source-backed ledger，p2/c4m6 parity 与 p5/当前 p8 no-fake-raw 边界普通通过，C13-M1 adapter channel 不回退。
- S5：已关闭，发布闸门确认 C13-M2 S4 resume-ready，S5/S6 仍留在 C13-M2 队列。

## 验收入口

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M3-MappedNameProducerLedger前置实现批次/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M2-FreeCADMappedNameParity实现批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M3-MappedNameProducerLedger前置实现批次/矩阵/*.tsv
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M2-FreeCADMappedNameParity实现批次/矩阵/*.tsv
git diff --check
```
