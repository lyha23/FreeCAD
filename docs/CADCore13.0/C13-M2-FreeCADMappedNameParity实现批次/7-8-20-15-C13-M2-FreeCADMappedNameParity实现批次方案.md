# C13-M2 FreeCAD MappedName Parity 实现批次方案

## 背景

C13-M1 已经完成 `topoNamingState` 发布闭环，但 S4 focused expected 对齐仍留下字节级 evidence gap：FreeCAD raw mapped-name、child map key、mapper history id。C13-M2 只补这层证据编码，不重新设计无状态 CAD Core 边界，也不改变 response subshape 语义。

## 问题定义

当前 runtime `topoNamingState` 的 `elementMap.entries` 来自 `NamedShape.elementMap`，但 `mappedName.raw/canonical` 仍主要使用 cad-core stable token。这样已经能支持 request-local round-trip，却不能和 FreeCAD expected 的 raw mapped-name evidence 完全对齐。

典型差异：

- expected `mappedName.raw` 是 FreeCAD `getElementMappedName()` 产生的 `#...:H...` / `#...:G...` 字符串。
- expected `mappedName.canonical` 经 collector canonicalizer 归一化 hash/delete 片段。
- expected `evidence.childElementMapKey` 和 `mapperHistoryIds` 是 FreeCAD / collector evidence 形态；runtime 目前只发布 child map projection 与 `mapperHistoryIndexes`。

## 设计原则

1. FreeCAD 源码优先：以 `MappedName.*`、`ElementMap.*`、`TopoShapeExpansion.cpp`、`TopoShapeMapper.cpp` 为语义来源。
2. Codec 与 publisher 分层：mapped-name 编码放到 `topo/` 或 `part/` 专用 helper；`runtime/topo_naming_state.cpp` 只消费结果。
3. expected 只做 oracle 对照：不得从 fixture expected 字符串反推出实现。
4. 最小完整语义批次：同一批覆盖 Body/Tip、Sketch internal、UpToFace、App::Link，不长期停留在单 fixture。
5. gap 可分类但不能伪支持：做不到 FreeCAD 字节级 parity 的字段必须留在矩阵中，而不是被写成 supported。

## 实施边界

第一轮做到：

- source audit 冻结 FreeCAD raw mapped-name 调用链和 collector comparator。
- focused red tests 锁定 `mappedName.raw/canonical`、`childElementMapKey`、`mapperHistoryIds` 的预期或明确 gap。
- runtime state publisher 使用 codec 结果，而不是 stable token fallback 当作 FreeCAD raw mappedName。
- 五个 focused fixtures 的 C13-M2 evidence 差异收敛或明确归类。

后续再做：

- 全量 expected topo state parity。
- 前端消费同步。
- 旧 adapter baseline 6F/8E 独立清理。

## 关闭条件

- `c13m2_mapped_name_blocker_queue.tsv` 中 C13-M2 必须项关闭。
- focused tests 证明 raw/canonical mappedName 不再由 fixture 字符串拼接。
- child map key 和 mapper history id 要么 focused 对齐，要么有明确 FreeCAD/source blocker。
- 队列全部关闭，矩阵 TSV 校验通过。
