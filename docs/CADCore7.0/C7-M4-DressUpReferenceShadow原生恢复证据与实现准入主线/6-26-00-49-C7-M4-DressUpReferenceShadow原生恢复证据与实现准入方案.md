# C7-M4 DressUp ReferenceShadow 原生恢复证据与实现准入方案

## 背景

C7-M3 已关闭 Fillet / Chamfer oracle rows，但 stale `ReferenceShadow` / Base recovery 仍是 `oracle_blocked`。阻塞点不是 cad-core 现有 geometry 能否算出结果，而是 FreeCAD native 是否真的会用旧 `SubList`、`ShadowSub` 和 `ReferenceShadow` 恢复 DressUp `Base` 引用。

当前 C7-M3 geometry-only probe 不足以作为 supported 证据：`cad-core/tools/collect_freecad_expected.py::link_sub_value()` 会优先把 `StableSubList` 喂给 FreeCAD `PropertyLinkSub`，这等价于已经恢复后的输入，不证明 native restore / element reference update 过程。

## 目标

- 把 C7-M3 `C7M3-SCOPE-103` 复制成 C7-M4 的唯一 active blocker。
- 设计并执行 FreeCAD native probe，观察 `PropertyLinkSub` / `ShadowSub` / `ReferenceShadow` 的真实恢复行为。
- 若 FreeCAD native 可恢复，用 current `cad-core` 做 parity 并裁决是否需要实现。
- 若 FreeCAD native 不可证明，保留 `oracle_blocked` 或裁为 `diagnostic_non_goal`，不改 C++。
- 若打开 implementation gate，按正式 link/topo/history 路径实现，并用 focused tests 约束 `documentObjectUpdates` / `elementReferenceUpdates`。

## 最小完整语义批次

| 批次 | 代表项 | 判定方式 |
| --- | --- | --- |
| Native restore evidence | stale `SubList=[OldFilletEdge1]`、`ShadowSub=[{oldName=OldFilletEdge1,newName=Edge1}]`、`ReferenceShadow` 指向旧 subshape | FreeCAD native restore / recompute probe，不能直接喂 `StableSubList` |
| DressUp consumer | `Chamfer.Base` 指向前序 `Fillet`，`DressUp::getContinuousEdges()` 使用 `Base.getShadowSubs()` | probe 必须记录 Base shadow subs 与最终 geometry |
| cad-core parity | 当前 `c3m5/dressup-reference-shadow-base-recovery` 或 S2 新 fixture | FreeCAD oracle expected / blocker + `cad-core` focused test |
| writeback contract | `documentObjectUpdates` / `elementReferenceUpdates` | 只有恢复后需要前端持久化建议时才写，不作为 backend session state |

## 实施纪律

- S0/S1 不改 C++、fixtures、expected 或 tests；只冻结状态和设计 probe。
- S2 可以新增或修改 oracle probe / collector 工具，但不能修改 runtime C++ 主路径。
- S3 只做 parity 和准入裁决；只有 route=`backend_gap_requires_implementation` 才打开 S4 code edit gate。
- `ReferenceShadow.brep` 只能保存被引用单个 subshape 的旧几何快照，用于恢复证据；不得作为建模输入或完整对象 BREP。
- 不允许 adapter 输出端修正、fixture 名称分支、按 EdgeN 猜测、只看 geometry 成功就发布 supported。

## 步骤

### S0 live baseline 与 blocked row 冻结

冻结当前 live 起点、工作区、C7-M1/M2/M3 队列和 C7-M3 blocker。S0 输出应至少包含：

- `pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git -c core.quotepath=false status --short -uall`。
- C7-M1 / C7-M2 / C7-M3 `工作步骤细分` 队列检查。
- `cad-core/fixtures/c3m5/dressup-reference-shadow-base-recovery.json` 与 expected known_gap 摘要。
- 当前相关 tests：`test_c7m3_reference_shadow_recovery_oracle_remains_blocked`、P6/C51 reference update tests。

### S1 FreeCAD native probe 设计

复核 FreeCAD 和 collector 调用链，输出可执行 probe 方案。S1 的关键问题是：怎样让 FreeCAD 从 stale persisted link state 自己恢复，而不是让 collector 直接设置恢复后的 subname。

推荐 probe 形态：

- 构造 `SketchPad -> Pad -> Fillet -> Chamfer -> Body`，让 `Chamfer.Base` 保存 stale old subname。
- 通过 FreeCAD native property restore / XML restore / document restore 路径注入 `SubList` 与 `ShadowSub`，而不是普通 Python `Base = (Fillet, ["Edge1"])` 赋值。
- 触发 recompute 后记录 `Chamfer.Base.getSubValues(false)`、`Chamfer.Base.getShadowSubs()`、`Chamfer.Shape` / `Body.Shape` summary。
- 如需 `ReferenceShadow.brep`，只记录旧 edge snapshot 与 hash，不把它作为生成 shape 的输入。

### S2 native oracle probe 与 expected / blocker

按 S1 设计执行 native probe。S2 有三种合法结果：

| route | 条件 | 输出 |
| --- | --- | --- |
| `native_oracle_collected` | FreeCAD 从 stale state 恢复，且能输出 shape summary 与 Base shadow evidence | 新 expected / evidence JSON，记录 FreeCAD version、恢复前后 SubList / ShadowSub / ReferenceShadow |
| `native_oracle_blocked` | FreeCADCmd、restore hook 或 collector 能力不足，无法证明 native 行为 | known_gap JSON，保留删除条件 |
| `native_not_supported` | FreeCAD native 明确不能恢复该 stale state | diagnostic expected，S3 不打开 implementation gate |

S2 不允许把 `/tmp` geometry-only 输出或 StableSubList-fed expected 当成 native oracle。

### S3 cad-core parity 与 implementation gate

如果 S2 得到 native oracle，则用当前 `cad-core` 对同一 fixture 做 parity：

- 若 current `cad-core` 已匹配 FreeCAD oracle，route=`already_closed_expected_backed`。
- 若 FreeCAD 可恢复但 current `cad-core` 失败，route=`backend_gap_requires_implementation`，S4 打开 code edit gate。
- 若 S2 是 blocker，route=`oracle_blocked`，S4 只做 no-code 发布。
- 若 FreeCAD native 明确不支持，route=`diagnostic_non_goal` 或 `native_not_supported`。

S3 必须把实现落点限定到 `cad-core/src/app`、`cad-core/src/part`、`cad-core/src/part_design` 和 focused tests，不得把 full MapperHistory 或 full DressUp universe 带进本包。

### S4 实现或 no-code 发布

若 S3 打开 code gate，S4 实现顺序固定：

1. 补 `PropertyLinkSub` / link DTO 解析和 shadow/reference evidence validation。
2. 补 `ReferenceShadow` / `brep` 证据读取与目标 subshape 匹配，失败给 diagnostics。
3. 在 `feature_dress_up.cpp` 只消费正式恢复后的 Base / shadow subs，不猜 EdgeN。
4. 写 focused tests，断言 geometry、diagnostics、`documentObjectUpdates` / `elementReferenceUpdates`。
5. 删除或保留 blocker expected 时必须同步矩阵和 README。

若 S3 没打开 code gate，S4 只做 publication closure，不改 C++。

### S5 release gate

收口队列和发布口径：

- 运行本包 queue、TSV、trailing whitespace 和 `git diff --check`。
- 若 S4 改了 C++ / collector / tests，运行对应 focused unittest；若改 C++，再运行 `cmake --build build`。
- 更新 root README、本包 README/总入口/方案、矩阵和必要的 P7 细化口径。
- 标记完成文件为 `【已实现】`，队列为空后提交。

## 验收分层

### 本轮短跑

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线 docs/CADCore7.0/README.md
git diff --check
```

### Oracle / collector 短跑

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
FREECADCMD=/Users/li/.cargo/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c3m5/dressup-reference-shadow-base-recovery.json --out /tmp/c7m4-dressup-reference-shadow-native-probe.json
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c7m3_reference_shadow_recovery_oracle_remains_blocked
```

S1/S2 可以替换第一条命令为新增 native probe 命令；替换时必须在矩阵记录准确命令。

### 实现短跑

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest
python3 -m unittest tests.test_p6_topology.CadCoreP6TopologyTest
```

只有 S4 改 C++、link recovery 或 topology recovery 时，这组才是必须执行项。

## 收口标准

- C7-M4 对 `dressup-reference-shadow-base-recovery` 给出 native oracle、native blocker 或 native not-supported 结论。
- 没有 native oracle 时不发布 supported，不打开 implementation gate。
- 有 native oracle 且存在 backend gap 时，代码实现不依赖 output-side guessing。
- 文档、矩阵、focused tests 和 capability / publication 口径一致。
