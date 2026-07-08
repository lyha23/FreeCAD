# 【已实现】C13-M2 S1 FreeCAD MappedName 源码调用链复核

## 目标

明确 FreeCAD raw `MappedName`、`ElementMap`、child map key 和 mapper history 的真实调用链。

## 必读文件

- S0 输出
- `src/App/MappedName.cpp`
- `src/App/MappedName.h`
- `src/App/ElementNamingUtils.h`
- `src/App/ElementMap.cpp`
- `src/App/ElementMap.h`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `src/Mod/Part/App/TopoShapeMapper.cpp`
- `src/App/PropertyLinks.cpp`

## 操作

1. 记录 `MappedName::fromRawData()`、postfix、hash/delete encoding 的关键短句。
2. 记录 `ElementMap::encodeElementName()`、`hashElementName()`、`hashChildMaps()`、`getElementHistory()` 的调用关系。
3. 记录 `TopoShapeExpansion.cpp` 中 `ensureElementMap()->encodeElementName()` 的生成/修改/child map 调用点。
4. 更新 source / contract matrix，明确哪些是 C13-M2 必须复刻，哪些仍是后续。

## 关闭条件

- source matrix 对 raw mappedName、child key、mapper history source authority 标为 `authority_frozen`。
- 不再存在“从 expected 字符串复制 mappedName”的路线。

## 源码结论

- `src/App/MappedName.h` 明确 `MappedName` 是两段结构：`data` 为创建后不可变的主数据，`postfix` 可被后续操作追加；`fromRawData()` 共享原始字节，不做 deep copy。C13-M2 raw/canonical helper 必须保持这个 data + postfix 合并视图，不能只按普通字符串重建。
- `src/App/MappedName.cpp::MappedName::findTagInElementName()` 解析 `POSTFIX_TAG == ";:H"`，格式为 `;:H<tag>:<len>,<type>` 或省略 tag 的 `;:H:<len>,<type>`；`tag` 和 `len` 以十六进制读取，`len` 指向 tag 前的 op-code 区间，`type` 是 `F/E/V` 等元素类型。
- `src/App/ElementNamingUtils.h` 是 element-name 字节常量 authority：`;:R` child map、`;:H` tag、`;:G` generated、`;:M` modified、`;:MG` modified+generated、`;:U` upper、`;:L` lower，以及 `;:C` child disambiguation 和 `;:I` index。
- `src/App/ElementMap.cpp::encodeElementName()` 先追加 op/postfix，再按需要保留或压缩 source tag，必要时调用 `hashElementName()`，最后追加 `;:H...` tag segment；`hashElementName()`/`dehashElementName()` 通过 `StringHasher` 和 `ElementIDRefs` 把长 mapped name 与 canonical hash 互转。
- `ElementMap::hashChildMaps()` 使用 child postfix 内已有 tag，先 hash postfix，再构造 `;:R<postfix>` 并强制 `encodeElementName()`；`addChildElements()` 负责 child map 展开、threshold、offset、duplicate `;:C` disambiguation 和 fallback 到逐元素 `setElementName()`。`getElementHistory()` 通过 `findTagInElementName()`、`;:R` child prefix 和 `dehashElementName()` 回溯原始/history names。
- `src/Mod/Part/App/TopoShapeExpansion.cpp` 中名称传播主路径是 `ensureElementMap()->encodeElementName()` 后 `elementMap()->setElementName()`：包括同拓扑映射、compound child map、generated/modified mapper、upper/lower 元素派生和 combo name。这里是 mapped name 生成/修改/上下层/组合名称传播 source authority。
- `src/Mod/Part/App/TopoShapeMapper.cpp` 的 `ShapeMapper::insert()`/`populate()` 是 generated/modified relation 的基础来源，并显式避免同一个目标 shape 同时进入 generated 和 modified；`TopoShapeExpansion.cpp::MapperMaker` 与 `MapperHistory` 从 OCCT maker/history 读取 `Modified()` / `Generated()` 列表后交给同一传播流程。
- `src/App/PropertyLinks.cpp` 负责 link subname 的 shadow、import/export 和 element reference 更新；它不是 raw mapped-name codec authority，但说明 mapped/new-style subname 会在属性保存、恢复和引用更新时被保留或替换。

## 关闭结果

- live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=10dd70aba8`（`10dd70aba8 docs: 关闭 C13-M2 S0 基线冻结`），起点 `git status --short -uall` 无输出。
- `C13M2-SRC-001..007` 已在 source matrix 标为 `authority_frozen`；`C13M2-SRC-008` 只保留为 S2 collector schema pointer。
- contract/scope matrix 已标清 raw/canonical/entry-key/child-key/mapper-id 的 FreeCAD source authority，但未标成 implemented、supported 或 focused parity green。
- `C13M2-BLOCKER-201` 和 `C13M2-IMPL-002` 已关闭；后续队列从 S2 `collector comparator 与 expected 证据矩阵` 开始。
- 本步未实现 codec/helper，未改 `cad-core` runtime/tests/fixtures/expected，未采集 oracle，未关闭 S2-S6，未从 expected 字符串复制 raw mappedName。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n "encodeElementName|hashElementName|hashChildMaps|getElementHistory|MappedName::fromRawData" src/App src/Mod/Part/App
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M2-FreeCADMappedNameParity实现批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M2-FreeCADMappedNameParity实现批次/矩阵/*.tsv
git diff --check
```
