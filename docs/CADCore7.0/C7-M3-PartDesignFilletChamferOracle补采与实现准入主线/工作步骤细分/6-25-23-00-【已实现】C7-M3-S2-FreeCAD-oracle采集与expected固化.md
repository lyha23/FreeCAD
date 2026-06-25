# 【已实现】C7-M3 S2 FreeCAD oracle 采集与 expected 固化

## 目标

按 S1 设计新增 C7-M3 fixtures，并采集 FreeCAD expected。S2 的核心产物是可信 oracle 或明确 native oracle blocker，不是 cad-core parity。

## live baseline

- `pwd=/Users/li/Chili3DProject/FreeCAD`
- `HEAD=ad03c44cfe`（`ad03c44cfe 文档：完成 C7-M3 S1 oracle fixture 设计`）
- 开始时 `git -c core.quotepath=false status --short -uall` 无输出。
- S2 只新增 fixture/expected/blocker JSON 并更新 C7-M3 docs/矩阵；不改 feature executor、runtime、topo、adapter、capability 或 tests。

## 必读

- S1 完成后的本包 `README.md`、方案和 `矩阵/*.tsv`
- `cad-core/tools/collect_freecad_expected.py`
- S1 指定的 fixture payload 参考文件
- `AGENTS.md` 中 FreeCADCmd / oracle / OCCT 基线规则

## 动作

1. 已记录 live baseline 和队列状态；队列从 S2 开始。
2. 已新增 S1 指定的 6 个 fixture JSON。
3. 已使用本机 `FREECADCMD=/Users/li/.cargo/bin/freecadcmd` 采集 5 个 FreeCAD expected；这些 expected 均来自 FreeCADCmd，不来自当前 `cad-core` recompute 输出。
4. 已对 `dressup-reference-shadow-base-recovery` 运行 geometry-only 探测到 `/tmp/c7m3-dressup-reference-shadow-base-recovery.geometry-only.freecad.json`，returncode=0；但当前 collector 仍只把 `StableSubList` 喂给 FreeCAD `PropertyLinkSub`，无法证明 stale `SubList` 经 `ShadowSub` / `ReferenceShadow` 原生恢复。
5. 已把 ReferenceShadow recovery 写成 native oracle blocker：`cad-core/fixtures/c3m5/expected/dressup-reference-shadow-base-recovery.freecad.json`，`known_gap.kind=dressup_reference_shadow_base_recovery_native_oracle_blocked`。
6. 已更新 README、方案、总入口、oracle/backend/source/blocker/validation 矩阵。
7. 本文件标题标记为 `【已实现】`，文件名随提交改名，队列推进到 S3。

## 产物

### FreeCADCmd expected-backed rows

| row | fixture | expected |
| --- | --- | --- |
| `C7M3-SCOPE-101` | `cad-core/fixtures/p7/fillet-pad-multi-edge.json` | `cad-core/fixtures/p7/expected/fillet-pad-multi-edge.freecad.json` |
| `C7M3-SCOPE-101` | `cad-core/fixtures/p7/fillet-pad-use-all-edges.json` | `cad-core/fixtures/p7/expected/fillet-pad-use-all-edges.freecad.json` |
| `C7M3-SCOPE-102` | `cad-core/fixtures/p7/chamfer-pad-edge-flip-true.json` | `cad-core/fixtures/p7/expected/chamfer-pad-edge-flip-true.freecad.json` |
| `C7M3-SCOPE-102` | `cad-core/fixtures/c3m5/chamfer-two-distances-edge-flip-true.json` | `cad-core/fixtures/c3m5/expected/chamfer-two-distances-edge-flip-true.freecad.json` |
| `C7M3-SCOPE-102` | `cad-core/fixtures/c3m5/chamfer-distance-angle-edge-flip-true.json` | `cad-core/fixtures/c3m5/expected/chamfer-distance-angle-edge-flip-true.freecad.json` |

采集命令均为单 fixture command，形式如下：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
FREECADCMD=/Users/li/.cargo/bin/freecadcmd python3 tools/collect_freecad_expected.py <fixture> --out <expected>
```

这些 expected 的 `reference` 字段均包含 `FreeCADCmd oracle from ...`，`freecad_version=1.2.0 revision 20260519`。

### native oracle blocker row

| row | fixture | blocker expected |
| --- | --- | --- |
| `C7M3-SCOPE-103` | `cad-core/fixtures/c3m5/dressup-reference-shadow-base-recovery.json` | `cad-core/fixtures/c3m5/expected/dressup-reference-shadow-base-recovery.freecad.json` |

`dressup-reference-shadow-base-recovery` 的 geometry-only 探测可生成 `objects.Fillet/Chamfer/Body` shape summary，但该结果只证明 `StableSubList=["Edge1"]` 当前几何可被采集，不能证明旧 `SubList=["OldFilletEdge1"]` 通过 `ShadowSub` / `ReferenceShadow` 被 FreeCAD 原生恢复。因此 S3 必须将 `C7M3-SCOPE-103` 裁为 `oracle_blocked`，不能打开实现闸门或发布 supported。

## 非目标

- 不改 feature executor。
- 不跑 implementation parity 结论。
- 不修改 capability supported 口径。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/工作步骤细分 --format markdown
find cad-core/fixtures/p7 -maxdepth 2 -type f | rg 'fillet|chamfer|reference|shadow|use-all|flip'
rg -n 'FreeCADCmd oracle|known_gap|native_oracle_blocker|ReferenceShadow|FlipDirection|UseAllEdges' cad-core/fixtures/p7 docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线/矩阵/*.tsv
git diff --check
```

本轮实际补充了 c3m5 fixtures，因此最终验证使用更宽的 fixture 查找和 grep：

```bash
cd /Users/li/Chili3DProject/FreeCAD
find cad-core/fixtures/p7 cad-core/fixtures/c3m5 -maxdepth 2 -type f | rg 'fillet-pad-(multi-edge|use-all-edges)|chamfer-.*flip-true|reference-shadow|shadow'
rg -n 'FreeCADCmd oracle|known_gap|native_oracle_blocker|ReferenceShadow|FlipDirection|UseAllEdges|ShadowSub' cad-core/fixtures/p7 cad-core/fixtures/c3m5 docs/CADCore7.0/C7-M3-PartDesignFilletChamferOracle补采与实现准入主线
```

## 通过条件

- 每个 row 有 expected-backed oracle、native oracle blocker 或明确 diagnostic fixture。
- 没有从 cad-core 输出倒推 expected。
- 队列推进到 S3。
