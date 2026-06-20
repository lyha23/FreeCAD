# C5-M6 Part Workbench Surface Profile / PostProcess 第二批方案

## 当前基线

- S0 live 复核基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=7217840df4`，`git log -1 --oneline=7217840df4 feat: 发布ProjectOnSurface能力收口`。
- S0 起始工作区不是干净树：dirty/untracked 集合集中在本 C5-M6 包草案与 CADCore5 根 README/矩阵行；未发现需要本步处理的 C++ dirty 文件。
- `cad-core/src/adapters/c_api/c_api.cpp` 已发布：
  - `part_workbench.loft.status=supported_profile_linearize_expected_backed`
  - `part_workbench.sweep.status=supported_multi_profile_linearize_expected_backed`
- `cad-core/tests/test_p8_features.py` 已有 focused tests：
  - `part-loft-linearize-profile-face`
  - `part-loft-linearize-profile-vertex`
  - `part-sweep-multi-profile-linearize`
  - `part-sweep-advanced-deferred`
- `cad-core/fixtures/c4m1` 已有上述 Loft / Sweep fixtures 和 native FreeCAD expected。

## S0 scope 冻结结论

- Loft supported / expected-backed 仅限 `c4m1/part-loft-linearize-profile-face` 与 `c4m1/part-loft-linearize-profile-vertex`：fixture、FreeCAD expected、`tests.test_p8_features` focused tests 和 adapter capability 断言均存在。
- Sweep supported / expected-backed 仅限 `c4m1/part-sweep-multi-profile-linearize`：fixture、FreeCAD expected、focused test 和 adapter capability 断言均存在。
- `c4m1/part-sweep-advanced-deferred` 只证明 advanced wrapper 属性会输出 locatable `unsupported_property` diagnostics；它不产生 supported shape，也不把 AuxiliarySpine / SupportMode / BiNormal / LocationMode / Tolerance contract 写成 supported。
- `complex_profile_family` 仍是 Loft remaining gap / non-goal；advanced PipeShell wrapper contract 仍是 Sweep future owner，不属于 C5-M6 supported scope。

## 为什么不是单独 Linearize 小包

`Linearize=true` 不是独立 feature，它在 FreeCAD `PartFeatures.cpp::Loft::execute()` 和 `Sweep::execute()` 中都位于主 shape 构造之后：

- Loft：`result.makeElementLoft(...)` 后调用 `result.linearize(LinearizeFace::linearizeFaces, LinearizeEdge::noEdges)`。
- Sweep：`result.makeElementPipeShell(...)` 后调用同一类 `result.linearize(...)`。

因此本包按同一 FreeCAD 调用链、同一 source-backed DocumentObject API、同一 native expected 类型，把 Loft face / vertex profile、Loft Linearize、Sweep multi-profile 和 Sweep Linearize 放在同一批复核和发布边界里。

## 范围

### Loft

- 源码依据：`src/Mod/Part/App/PartFeatures.cpp::Loft::execute()`、`src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementLoft()`、`TopoShape::linearize()`。
- cad-core 落点：`cad-core/src/part/part_loft.cpp`、`cad-core/src/part/topo_shape_expansion.cpp`、`cad-core/src/adapters/c_api/c_api.cpp`。
- S1 复核基线：`HEAD=07a0b3903d`，起始工作区干净；未发现需要修改 C++ 或 fixture 的证据缺口。
- 当前 expected-backed 证据：
  - `c4m1/part-loft-linearize-profile-face`
  - `c4m1/part-loft-linearize-profile-vertex`
- 发布边界：只发布 face / vertex profile 与 `Linearize=true` 对应的 `linearize_faces_no_edges_post_processing`；`complex_profile_family` 仍保留 gap / non-goal。

### Sweep

- 源码依据：`src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()`、`src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementPipeShell()`、`TopoShape::linearize()`。
- cad-core 落点：`cad-core/src/part/part_sweep.cpp`、`cad-core/src/part/topo_shape_expansion.cpp`、`cad-core/src/adapters/c_api/c_api.cpp`。
- 当前 expected-backed 证据：
  - `c4m1/part-sweep-multi-profile-linearize`
- 当前 diagnostic 证据：
  - `c4m1/part-sweep-advanced-deferred`
- 发布边界：multi-profile `Sections` 与 Linearize 后处理已可发布；AuxiliarySpine、SupportMode、Binormal、LocationMode、Tolerance 等 advanced wrapper contract 继续作为后续 owner，不混入本包。

## 最小完整语义批次

本包的最小完整批次不是一个 fixture，而是以下闭环：

1. Loft face profile + vertex profile + Linearize 后处理。
2. Sweep multi-profile sections + Linearize 后处理。
3. 两者共用的 `LinearizeFace::linearizeFaces / LinearizeEdge::noEdges` 后处理发布口径。
4. capability、fixtures、focused tests、CADCore3.0 docs 和 remaining gaps 同步。
5. 真正剩余的 complex / advanced branches 分流到下一包，不继续扩大本包。

## 非目标

- 不处理 PartDesign `FeatureLoft` / `FeaturePipe` 的 C5-M3 known-gap rows。
- 不处理 Filling / GeomPlate advanced constraints。
- 不实现 advanced `BRepOffsetAPI_MakePipeShell` wrapper 的辅助脊、support mode、trihedron / binormal、located profile 或 Hole internal PipeShell。
- 不用 bbox、输出顺序或 fixture 名称修正 topo naming。
- 不把 `complex_profile_family` 或 advanced PipeShell contract 写成 supported。

## 工作步骤

| step | 目标 | 产物 |
| --- | --- | --- |
| S0 | live 基线与 scope 冻结 | 当前 code/docs/capability 事实、矩阵状态 |
| S1 | Loft profile / Linearize 复核收口 | Loft fixture / expected / tests / docs 同步 |
| S2 | Sweep multi-profile / Linearize 复核收口 | Sweep fixture / expected / tests / docs 同步 |
| S3 | 剩余复杂分支分流 | complex_profile_family 与 advanced PipeShell contract owner |
| S4 | 发布与队列收口 | capability / CADCore3.0 / README / 队列空 |

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线/工作步骤细分 --format markdown
```

focused 回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

涉及 C++ 补实现时追加：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
```
