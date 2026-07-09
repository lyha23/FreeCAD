# 【已实现】C13-M4 S0 live 基线与 ledger 闭包冻结

## 目标

冻结 C13-M4 的 live 起点，确认当前问题是 runtime public projection 缺口，而不是 expected / ledger sidecar 缺口。

## 必读文件

- `docs/框架/7-9-15-53-FreeCADCmd权威账本与topoNamingState裁剪原则.md`
- `docs/CADCore13.0/C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次/README.md`
- `cad-core/fixtures/c4m6/expected/topo-state-link-compound-child-maps.freecad.json`
- `cad-core/fixtures/c4m6/expected/topo-state-link-compound-child-maps.freecad.ledger.json`
- `cad-core/tests/test_topo_naming_state_response.py`

## 操作

1. 记录 `git status --short`，确认 `DESIGN.md` 属于无关 dirty 文件，不纳入本批次。
2. 运行 c4m6 ledger validator，确认 sidecar 闭包为 green。
3. 运行 focused topoNamingState response test，记录首个 failure。
4. 用 actual/expected 对比确认缺失项是 `Compound.Child0.Face1` projection，而不是普通 compound child maps 全缺。
5. 更新矩阵中 S0 状态，只记录结论，不改 C++。

## 关闭条件

- `C13M4-BLOCKER-001` 关闭。
- `C13M4-FIX-001` 的当前 diff 被精确记录。
- 本步不修改 expected，不重采 oracle，不改 runtime。

## S0 冻结结果

- live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=718267783c`，`git log -1 --oneline=718267783c chore: 刷新 FreeCAD expected 账本基线`。
- dirty 边界：S0 开始前已有 `DESIGN.md`、`docs/CADCore13.0/README.md`、`docs/框架/7-9-15-53-FreeCADCmd权威账本与topoNamingState裁剪原则.md` 以及 C13-M4 包目录改动；`DESIGN.md` 不纳入本步。
- ledger gate：`cd cad-core && python3 tools/validate_freecad_expected_ledger.py --phase c4m6 --strict` 通过，9 个 c4m6 `.freecad.json` 均由同名 `.freecad.ledger.json` 闭合。
- focused runtime gate：`cd cad-core && python3 -m unittest tests.test_topo_naming_state_response` 按 S0 预期失败，首个 failure 是 `c4m6/topo-state-link-compound-child-maps: topoNamingState.objects.Compound.subshapes.Child0.Face1: missing from actual response`。
- actual/expected 对比：actual 已发布普通 child maps `Compound:ChildBoxA:Child0` 与 `Compound:ChildBoxB:Child1`；缺失的是 input-reference-driven projection map `Compound:ChildBoxA`、subshape `Child0.Face1`，以及顶层 owner-qualified entry `Compound/ChildBoxA.#f:1;BOX,F`。
- ledger sidecar 证据：`topo-state-link-compound-child-maps.freecad.ledger.json` 的 `coverage.uncoveredInputReferenceIds=[]`、`roundTrip.status=passed`，并有 `event:1` 将 `ChildBoxA.Face1` 解析到 `Compound.Child0.Face1`。
- 结论：当前缺口归属 runtime public projection，不是 expected / ledger sidecar；S1 才进入 C++ 发布逻辑。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/validate_freecad_expected_ledger.py --phase c4m6 --strict
python3 -m unittest tests.test_topo_naming_state_response
cd ..
git diff --check
```
