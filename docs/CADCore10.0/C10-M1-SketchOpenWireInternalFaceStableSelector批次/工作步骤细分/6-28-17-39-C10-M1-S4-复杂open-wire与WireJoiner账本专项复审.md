# C10-M1-S4 复杂 open-wire 与 WireJoiner 账本专项复审

## 目标

复核复杂开放线网和 WireJoiner history ledger，判断哪些 open-wire / split / branch case 能产生唯一 ElementMap alias，哪些必须保持 stable diagnostic。S4 不靠输出修剪解决 ownership。

## FreeCAD 依据

- `src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::initWireInfo()`
- `src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::splitEdges()`
- `src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::findTightBound()`
- `src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::exhaustTightBound()`
- `src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::getOpenWires()`

## 范围

| 账本轴 | 必须观察 | S4 路由 |
| --- | --- | --- |
| branch open cutter | connected branch 是否进入 openWireCompound 或被 bounded face 消费 | S6 implementation 或 diagnostic retained。 |
| multi-result open wires | child-wire ledger 是否能唯一定位 current member | S6 implementation 或 stable diagnostic。 |
| one-source-to-many | source edge split 成多个 InternalEdge 时是否有唯一 selector | diagnostic retained，除非 evidence 唯一。 |
| deleted source | noOriginal filtered raw Edge / Vertex 是否写 terminal deleted history | release gate 或 implementation candidate。 |

## 必须回写的矩阵行

- `C10M1-SCOPE-103`
- `C10M1-SCOPE-104`
- `C10M1-BLOCKER-401`
- `C10M1-CAT-102`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'WireJoiner|openWireCompound|EdgeInfo|WireInfo|aHistory|one-to-many|multiplicity|terminal deleted|InternalEdge|InternalVertex' cad-core/src/part cad-core/src/topo cad-core/tests cad-core/fixtures/p5 docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次/矩阵/*.tsv
git diff --check
```

## 验收标准

- 每个复杂 open-wire route 都说明是否具备唯一 ElementMap / mapper history evidence。
- 一对多或多解 case 必须保持 stable diagnostic，不得选择任意 target。
- 若 S4 打开 S6 implementation gate，必须列出具体 fixture、expected、C++ 落点和 focused test。

## 非目标

- 不新增 source index / split order / geometry sorting fallback。
- 不在 adapter 或 output JSON 末端补业务逻辑。
