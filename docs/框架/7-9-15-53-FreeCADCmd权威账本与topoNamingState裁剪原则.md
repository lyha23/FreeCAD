# FreeCADCmd expected 账本闭包验收原则

## 结论

`fixtures/<phase>/expected/*.freecad.json` 是 FreeCADCmd / native oracle 结果。本阶段先不重新设计一套更厚的 native oracle schema，而是验证现有 `.freecad.json` 里的 `topoNamingState` 账本投影是否闭合、自洽、可复现。

也就是说，验收重点不是要求 expected 新增 `oracleMetadata`、`topologyInventory`、`referenceLedger` 这类新块，而是检查现有结构里已经发布的这些账本是否不断链：

- `topoNamingState.objects[*].subshapes`
- `topoNamingState.objects[*].elementMap.entries`
- `topoNamingState.objects[*].childElementMaps`
- `topoNamingState.objects[*].mapperHistory`
- `elementReferenceUpdates[*].StableSubList`
- `diagnostics` 对 split / deleted / ambiguous 的解释

## 验收入口

新增独立脚本：

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
python3 tools/validate_freecad_expected_ledger.py --phase c4m6 --strict
```

也可以对全部 expected 做摸底审计：

```bash
python3 tools/validate_freecad_expected_ledger.py --all
```

`--all` 会读取所有 `*.freecad.json`，但只有带 `topoNamingState` 的文件参与账本闭包统计；普通几何 expected 不会因为没有 `topoNamingState` 被判失败。当前全库仍可能暴露其它 phase 的既有闭包缺口，不能直接当作本轮必须为绿的门禁。

如果要同时证明 checked-in expected 能由当前 FreeCADCmd collector 复现，再加：

```bash
python3 tools/validate_freecad_expected_ledger.py \
  --phase c4m6 \
  --strict \
  --freecadcmd-check
```

`--freecadcmd-check` 内部调用：

```bash
python3 tools/collect_freecad_expected.py --phase <phase> --check --skip-unsupported
```

## 这个脚本证明什么

它只读 `fixtures/<phase>/expected/*.freecad.json`，不依赖 `cad-core` runtime，也不读取 `cad-core` 内部 `named_shapes`。

它证明的是 checked-in `.freecad.json` 这个 expected artifact 自身闭合：

- 每个 `subshapes` key 都等于自己的 `subname`。
- 每个 subshape 都是 `FaceN` / `EdgeN` / `VertexN` 这种 indexed topo name。
- `rawFreecadMappedName` 能重新 canonicalize 到 `canonicalFreecadMappedName`。
- 每个 `elementMap.entries` key 等于 `mappedName.canonical`。
- 每个 `elementMap` target 都能回到某个 `topoNamingState.objects[*].subshapes`。
- 每个 `elementMap` source 非空。
- `evidence.mapperHistoryIds` 引用的 event 必须存在。
- `childElementMaps` 的 owner / child / target / source / evidence 能互相对上。
- `mapperHistory` 的 id 唯一，relation/source/target 合法。
- split / deleted / ambiguous 这类 terminal event 由 diagnostics 或 `diagnostic_status` 解释。
- `elementReferenceUpdates` 里的 `StableSubList` 能在目标 object state 中解析。

`--strict` 是 phase-level 覆盖要求，适合 `c4m6` 这种专门的 topoNamingState phase。它要求 corpus 至少覆盖：

- `elementMap.entries`
- `childElementMaps`
- `mapperHistory`
- `generated` / `modified` / `split` / `deleted` / `merge` / `ambiguous`
- `split_stable_subname` / `deleted_stable_subname` / `stable_identity_ambiguous`

## 分层关系

现在保留三个清楚的层次：

- `collect_freecad_expected.py --check`：证明 expected 能由 FreeCADCmd / native collector 复现。
- `validate_freecad_expected_ledger.py`：证明 checked-in `.freecad.json` 的 topoNamingState 账本投影闭合。
- 其它 coverage 测试：证明 expected corpus 覆盖了哪些业务形态。

不要把这三件事混成一个庞大的 native oracle schema。当前最小有效目标是：先让现有 `.freecad.json` 的 public topoNamingState 账本投影可验证、可门禁。
