# 【已实现】C13-M3 S3 PartDesign focused producer 接线

## 目标

把 producer ledger 接入 p2 / c4m6 / p6 涉及的 PartDesign producer paths，使 required entries 有 source-backed raw mapped-name evidence。

## 必读文件

- S2 输出
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/src/part_design/feature_pad.cpp`
- `cad-core/src/part_design/feature_pocket.cpp`
- `cad-core/src/part_design/body.cpp`
- focused fixtures and expected

## 操作

1. 接入 maker history / preserved source / generated / modified 传播点。
2. 确认 p2 Body、c4m6 Body、p6 ProbePad 的 required entries 能获得 ledger evidence。
3. 不处理 p5/p8 fake raw；它们仍应保持 indexed-only boundary。

## 关闭条件

- `C13M3-BLOCKER-301` 关闭。
- p2/c4m6/p6 不再只依赖 stable token fallback。

## 关闭结果

- `cad-core/src/part/topo_shape.cpp` 在 shared `applyHistoryElementMap()`、`applyPreservedElementMap()`、`addRetagAlias()`、`namedShapeForMakerHistory()` 和 `namedShapeForPreservedSources()` 路径填充 `NamedShape::mappedNameProvenance`，通过 S2 `encodedMappedNameProvenance()` 生成 source-backed raw/canonical；缺 tag/op/type evidence 的条目仍保持 missing/blocker，不用 stable token 伪造 raw。
- PartDesign focused producer op 已接线：Pad/Pocket prism 生产传入 `XTR`，Body boolean 生产传入 `FUS` / `CUT` / `CMN`，preserved source 重放会继承上游 source-backed provenance。
- 新增 `test_c13m3_s3_partdesign_producer_evidence_exists_for_focused_paths`，用 `CAD_CORE_TEST_LEGACY_OUTPUT=1` 验证 p2 `rect-pad-pocket` Body、c4m6 `topo-state-body-tip-stable-recovery` Body、p6 `up-to-face-stable-body-history` 的 Body-side producer evidence 已有 source-backed/encoded entries；p6 `ProbePad` 当前仍停在既有 UpToFace runtime diagnostic，S4 再处理 runtime 消费/发布。
- 本步未修改 `runtime/topo_naming_state.cpp`，未移除 C13-M2 五个 `@unittest.expectedFailure`，未编辑 fixtures/expected/cad-core-res，未复制 expected raw mappedName 字符串。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_topo_naming_state_response
```

验收通过：focused unittest 仍为 `OK (expected failures=5)`，S4-S5 继续 pending。
