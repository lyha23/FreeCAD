# 【已实现】C13-M3 S1 producer ledger 接口设计

## 目标

设计 `NamedShape` / codec helper 可承载的 FreeCAD-equivalent tag、source tag、operation postfix、raw/canonical mapped-name provenance。

## 必读文件

- S0 输出
- `src/App/MappedName.h`
- `src/App/MappedName.cpp`
- `src/App/ElementMap.cpp`
- `src/App/ElementNamingUtils.h`
- `cad-core/include/cad_core/part/topo_shape.h`
- `cad-core/src/part/topo_shape.cpp`

## 操作

1. 明确 ledger 字段：current element、source element、element type、producer/master tag、source tag、operation postfix、raw/canonical、provenance status。
2. 决定字段落在 `NamedShape`、`NamedElement`、独立 map，还是专用 `MappedNameProvenance`。
3. 写 FreeCAD source 注释要求。
4. 更新 source / implementation matrix。

## 关闭条件

- `C13M3-BLOCKER-101` 关闭。
- 代码接口可以支持 S2 实现，不要求本步填满所有 producer。

## 关闭结果

- `cad-core/include/cad_core/part/topo_shape.h` 新增 request-local `MappedNameProvenanceStatus` 与 `MappedNameProvenance`，字段覆盖 entry key、current/source element、element type、producer/master/source tag、operation postfix、raw/canonical mapped name 和 status。
- `NamedShape` 新增 `mappedNameProvenance` map，按 entry/stable element name 作为 key；`namedShapeToJson()` 输出 `mapped_name_provenance`，供 S2 codec/helper 与 S4 publisher 检查 producer evidence。
- 公共语义类型旁已标注 FreeCAD source：`TopoShapeExpansion.cpp::mapSubElement(...)` 调用 `ensureElementMap()->encodeElementName(..., Tag, op, other.Tag)`，`ElementMap.cpp::encodeElementName(... masterTag ... tag ...)`，以及 `MappedName.cpp::findTagInElementName()` 解析 `;:H<tag>:<len>,<type>`。
- 本步不填充 producer ledger、不实现 byte codec、不改变 runtime publisher、不删除 C13-M2 expectedFailure、不复制 expected raw mappedName，也不触碰 fixture/expected/cad-core-res。S2-S5 继续 pending。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build

cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M3-MappedNameProducerLedger前置实现批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M3-MappedNameProducerLedger前置实现批次/矩阵/*.tsv
git diff --check
```

本步验收已通过；队列从 S2 开始。
