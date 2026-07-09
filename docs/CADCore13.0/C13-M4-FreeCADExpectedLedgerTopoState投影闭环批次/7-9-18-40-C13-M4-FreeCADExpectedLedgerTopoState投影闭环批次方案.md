# C13-M4 FreeCADExpectedLedger TopoState 投影闭环批次方案

## 背景

`docs/框架/7-9-15-53-FreeCADCmd权威账本与topoNamingState裁剪原则.md` 已经确定 expected 分层：

- `fixtures/<phase>/expected/*.freecad.json` 是 public protocol expected。
- `fixtures/<phase>/expected/*.freecad.ledger.json` 是 FreeCADCmd / native oracle 权威账本 sidecar。
- validator 只证明 checked-in expected 与 ledger sidecar 闭合，不调用 `cad-core` runtime。

C13-M4 的任务不是再扩展 expected schema，而是把 runtime output 对齐 public projection。当前最小闭环选 `c4m6`，因为它同时有 native expected、ledger sidecar、protocol failure cases 和 focused runtime tests。

## 当前失败

已验证：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/validate_freecad_expected_ledger.py --phase c4m6 --strict
```

结果：9 个 expected fixture 均 OK。

已验证：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_topo_naming_state_response
```

当前失败集中在 `c4m6/topo-state-link-compound-child-maps`：

```text
topoNamingState.objects.Compound.subshapes.Child0.Face1: missing from actual response
```

actual 已经发布普通 child map：

- `Compound:ChildBoxA:Child0`
- `Compound:ChildBoxB:Child1`

expected 还要求一个 input-reference-driven projection map：

- `key = Compound:ChildBoxA`
- `pathPrefix = Child0`
- `entry = ChildBoxA.#f:1;BOX,F`
- `source = ChildBoxA.Face1`
- `target = Compound.Child0.Face1`

并且顶层 `Compound.elementMap.entries` 需要包含 owner-qualified canonical key，例如 `Compound/ChildBoxA.#f:1;BOX,F`。

## 设计原则

1. `.freecad.ledger.json` 是验收证据，不是 runtime 输入。
2. `.freecad.json` 继续只发布 public `topoNamingState` 投影。
3. runtime 发布逻辑必须由当前 `NamedShape`、child maps、input references 和 mapper history 解释。
4. 对 canonical key collision 只能显式保留 ambiguity / mapperHistory 证据，不允许静默覆盖。
5. 对 child path projection 只补通用语义，不按 `topo-state-link-compound-child-maps` fixture 名称写分支。

## 实现框架

### S0 live baseline

冻结以下事实：

- `validate_freecad_expected_ledger.py --phase c4m6 --strict` 已过。
- `tests.test_topo_naming_state_response` 当前只剩 `Compound.Child0.Face1` projection 类失败。
- `DESIGN.md` 是无关 dirty 文件，后续不纳入本批次提交。

### S1 child path projection 发布

落点：`cad-core/src/runtime/topo_naming_state.cpp`。

实现方向：

1. 在 object topo state 发布阶段，基于 `NamedShape.childElementMaps` 和 response/input reference evidence 生成 child path subshape。
2. 对 `Compound` 这类非 Body owner，不只发布 `Face1`，也发布被 reference projection 需要的 `Child0.Face1`。
3. 生成 `Compound:ChildBoxA` 这类 projection child map，保留 source object / target owner / `pathPrefix` / mappedName evidence。
4. 合并 projection entry 到顶层 `elementMap.entries`，key 使用 owner-qualified canonical key，避免和普通 child-local canonical key 冲突。

### S2 c4m6 focused parity

目标：

- `python3 -m unittest tests.test_topo_naming_state_response` 普通通过。
- `topo-state-link-compound-child-maps` 中 `Compound.subshapes.Child0.Face1`、`Compound:ChildBoxA`、顶层 `Compound/ChildBoxA.#f:1;BOX,F` 均对齐 expected。
- hard fail cases 仍不发布 `topoNamingState`。
- `ReferenceShadow.brep` 仍只作为 evidence，不变成建模输入。

### S3 门禁收口

目标：

- 更新 README/矩阵状态。
- 若 S2 证明不依赖 C13-M3 raw-key blocker，则记录 C13-M4 与 C13-M2/C13-M3 的边界。
- 若 S2 暴露更深 producer ledger blocker，只记录为 C13-M3 回流项，不在 C13-M4 里复制 expected 字符串。

## 非目标

- 不手改 expected。
- 不重采 native expected，除非 collector 被证明错误。
- 不把 sidecar ledger 读入 runtime。
- 不扩大到全量 `fixtures/<phase>/expected/*.freecad.json`。
- 不修 p2/p6 raw-key producer ledger blocker。
- 不改前端。

## 验收命令

### 本轮短跑

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/validate_freecad_expected_ledger.py --phase c4m6 --strict
python3 -m unittest tests.test_topo_naming_state_response
cd ..
git diff --check
```

### 代码改动后

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 tools/validate_freecad_expected_ledger.py --phase c4m6 --strict
python3 -m unittest tests.test_topo_naming_state_response
```

### 阶段收口

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次/矩阵/*.tsv
git diff --check
```
