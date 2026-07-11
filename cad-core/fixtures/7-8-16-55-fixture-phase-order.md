# cad-core fixtures 用例顺序说明

基线刷新：2026-07-12（按工作树根输入、`fixture_roles.v1.json` 和现有 sidecar 实测）。

本文说明 `cad-core/fixtures/` 的 phase 阅读顺序、根输入边界，以及 native oracle 的三侧产物契约。它不是发布状态快照：请以当前文件系统和角色 manifest 为准。

当前共有 51 个 phase、775 个根输入 fixture。角色 manifest 覆盖全部根输入，其中 480 个 `native`、14 个 `protocol_only`、281 个 `unsupported`。现有 `expected/` 中有 480 个 public expected、480 个 ledger 和 480 个通过闭包校验的 producer trace，全部 `native` 用例均已完成三侧产物收集。

## 核心结论

`fixtures` 不是单一线性队列：先按顶层 phase 分组，再在该 phase 内处理根输入 JSON。

- 语义阅读 / 排查顺序：先看 `p2`、`p3a`、`p3b`、`p4` 至 `p8`，再看后续 `cXmY` / `c51mY` 里程碑目录。
- 历史 `mvp` 已删除，`c7m1` 也不在当前目录集合；不要将它们当作空 phase 槽位。
- 根输入只能是 `fixtures/<phase>/*.json`。`expected/`、`cad-core-res/` 和 `cad-rs-res/` 都不是输入遍历的一部分。
- `cad-core/tools/freecad_expected_parity/fixture_roles.v1.json` 是输入角色的唯一目录：禁止按是否已有 expected 文件猜测 `native`、`protocol_only` 或 `unsupported`。
- `topoNamingState` 是请求随附的客户端状态，不是 FreeCAD/CAD Core 的跨请求缓存，也不是几何建模输入。

## native oracle 三侧产物

对角色为 `native` 的用例，`collect_freecad_expected.py` 以原生 FreeCADCmd 采集同一份 fixture 的三个同名 sidecar：

| 文件 | 定位 | CAD Core 是否可直接作为协议 oracle 使用 |
| --- | --- | --- |
| `expected/<case>.freecad.json` | 对外 public result / topo-state 合同 | 是 |
| `expected/<case>.freecad.ledger.json` | provenance、对象/元素解析等辅助证据 | 仅作辅助诊断 |
| `expected/<case>.freecad.producer-trace.json` | FreeCAD ElementMap 生产过程、切片、checkpoint、scope 和 snapshot 闭包证据 | 否；它是驱动实现和定位差异的只读诊断 oracle |

trace 不得被写入 `topoNamingState`、普通 CAD Core response 或 fixture 输入。它的作用是回答“哪个原生生产切片造成了这一元素映射/稳定名”，而不是替代 public result。

收集器默认使用本轮 producer-enabled 原生二进制：

```text
/Users/li/Chili3DProject/FreeCAD2/build/relwithdebinfo/bin/FreeCADCmd
```

可以用 `--freecadcmd <path>` 显式替换；不要再依赖 `FREECADCMD` 环境变量。收集失败、无法 drain trace、trace schema/sequence/snapshot/scope 闭包不合法，都会 hard fail，不能悄悄缺少第三个 sidecar。

当前 request-level preflight rejection 在创建 FreeCAD Document 前结束，没有可 drain trace。write-mode 会 hard fail，不能伪造空 trace。trace 全量迁移前，不要用 phase write-mode 批量覆盖正式 expected；它可能先写成功 case，再在 rejection/缺口处失败。

`--check` 会比较重新生成的 public expected 和 ledger，并校验已存在 producer trace 的结构与闭包。trace 仍是诊断证据，当前不以文本 diff 作为 release comparator；需要判断生产路径差异时，直接按 event 的 `slice`、checkpoint、`beforeSnapshot` / `afterSnapshot` 与 scope 链路定位。

## 推荐 phase 顺序与角色覆盖

表中的角色列来自 `fixture_roles.v1.json`；它们的和等于该 phase 的根输入数。`protocol_only` 有协议合同但不应调用 native 收集；`unsupported` 必须先补齐原生能力或得到明确的协议决策，不能用手写 expected 掩盖。

| 顺序 | phase | 根输入 | native | protocol only | unsupported | 说明 |
| ---: | --- | ---: | ---: | ---: | ---: | --- |
| 1 | `p2` | 4 | 3 | 0 | 1 | 早期 PartDesign / Body 基础扩展。 |
| 2 | `p3a` | 7 | 4 | 0 | 3 | P3 前半段扩展。 |
| 3 | `p3b` | 29 | 17 | 0 | 12 | P3 后半段扩展。 |
| 4 | `p4` | 8 | 4 | 0 | 4 | typed property、link/sublist、placement 等基础合同。 |
| 5 | `p5` | 117 | 58 | 0 | 59 | Sketch / internal shape / solver-facing 状态等大批量 fixture。 |
| 6 | `p6` | 15 | 7 | 0 | 8 | stable subname、Body history、UpToFace 等拓扑引用 fixture。 |
| 7 | `p7` | 109 | 56 | 0 | 53 | PartDesign Pad/Pocket/Hole/Pattern/DressUp 等综合 fixture。 |
| 8 | `p8` | 80 | 68 | 0 | 12 | Part workbench、Link、Assembly 早期能力等 fixture。 |
| 9 | `c3m1` | 12 | 3 | 0 | 9 | C3-M1 里程碑。 |
| 10 | `c3m2` | 12 | 1 | 11 | 0 | C3-M2 里程碑。 |
| 11 | `c3m3` | 22 | 1 | 0 | 21 | C3-M3 里程碑。 |
| 12 | `c3m4` | 30 | 17 | 0 | 13 | C3-M4 里程碑。 |
| 13 | `c3m5` | 26 | 15 | 0 | 11 | C3-M5 里程碑。 |
| 14 | `c3m6` | 73 | 71 | 0 | 2 | C3-M6 里程碑。 |
| 15 | `c3m7` | 1 | 1 | 0 | 0 | C3-M7 里程碑。 |
| 16 | `c4m1` | 20 | 19 | 0 | 1 | C4-M1 里程碑。 |
| 17 | `c4m2` | 14 | 10 | 0 | 4 | C4-M2 里程碑。 |
| 18 | `c4m3` | 18 | 8 | 0 | 10 | C4-M3 里程碑。 |
| 19 | `c4m4` | 9 | 2 | 0 | 7 | C4-M4 里程碑。 |
| 20 | `c4m5` | 3 | 3 | 0 | 0 | C4-M5 里程碑。 |
| 21 | `c4m6` | 10 | 9 | 1 | 0 | C4-M6 里程碑。 |
| 22 | `c5m1` | 10 | 6 | 0 | 4 | C5-M1 里程碑。 |
| 23 | `c5m2` | 4 | 2 | 0 | 2 | C5-M2 里程碑。 |
| 24 | `c5m3` | 6 | 4 | 0 | 2 | C5-M3 里程碑。 |
| 25 | `c5m4` | 1 | 0 | 0 | 1 | C5-M4 里程碑。 |
| 26 | `c5m7` | 9 | 9 | 0 | 0 | C5-M7 里程碑。 |
| 27 | `c5m8` | 12 | 11 | 1 | 0 | C5-M8 里程碑。 |
| 28 | `c5m9` | 5 | 4 | 0 | 1 | C5-M9 里程碑。 |
| 29 | `c5m10` | 6 | 6 | 0 | 0 | C5-M10 里程碑。 |
| 30 | `c5m12` | 5 | 4 | 0 | 1 | C5-M12 里程碑。 |
| 31 | `c5m13` | 5 | 5 | 0 | 0 | C5-M13 里程碑。 |
| 32 | `c51m1` | 12 | 7 | 0 | 5 | C5.1-M1；语义上按 C5.1 处理。 |
| 33 | `c51m2` | 5 | 0 | 0 | 5 | C5.1-M2。 |
| 34 | `c51m3` | 3 | 2 | 0 | 1 | C5.1-M3。 |
| 35 | `c51m4` | 4 | 3 | 0 | 1 | C5.1-M4。 |
| 36 | `c51m5` | 16 | 15 | 0 | 1 | C5.1-M5。 |
| 37 | `c6m1` | 6 | 1 | 0 | 5 | C6-M1 里程碑。 |
| 38 | `c6m3` | 3 | 0 | 0 | 3 | C6-M3 里程碑。 |
| 39 | `c6m4` | 4 | 4 | 0 | 0 | C6-M4 里程碑。 |
| 40 | `c6m5` | 7 | 6 | 1 | 0 | C6-M5 里程碑。 |
| 41 | `c6m6` | 2 | 2 | 0 | 0 | C6-M6 里程碑。 |
| 42 | `c6m7` | 2 | 0 | 0 | 2 | C6-M7 里程碑。 |
| 43 | `c8m1` | 12 | 0 | 0 | 12 | C8-M1 里程碑。 |
| 44 | `c8m2` | 1 | 0 | 0 | 1 | C8-M2 里程碑。 |
| 45 | `c8m4` | 1 | 1 | 0 | 0 | C8-M4 里程碑。 |
| 46 | `c9m5` | 1 | 0 | 0 | 1 | C9-M5 里程碑。 |
| 47 | `c10m1` | 4 | 4 | 0 | 0 | C10-M1 里程碑。 |
| 48 | `c12m12` | 1 | 1 | 0 | 0 | C12-M12 里程碑。 |
| 49 | `c12m13` | 6 | 5 | 0 | 1 | C12-M13 里程碑。 |
| 50 | `c12m14` | 1 | 0 | 0 | 1 | C12-M14 里程碑。 |
| 51 | `c12m16` | 2 | 1 | 0 | 1 | C12-M16 里程碑。 |
| **合计** | **51 phases** | **775** | **480** | **14** | **281** | **以 role manifest 为准。** |

## 单个 phase 内的文件边界

根输入 fixture 是 `fixtures/<phase>/*.json`。例如 `p2` 当前包含：

```text
body-basefeature-pad.json
pocket-open-sketch.json
pocket-without-base.json
rect-pad-pocket.json
```

`collect_freecad_expected.py --phase <phase>` 根据角色 catalog 枚举根输入，不会递归进入 `expected/`、`cad-core-res/` 或 `cad-rs-res/`。`--skip-unsupported` 只在 phase 模式中跳过 manifest 标为 `unsupported` 的用例；它不会把 `protocol_only` 伪装成 native。

`tests/test_expected_fixtures.py` 扫描 `fixtures/*/expected/*.freecad.json`。其中 `known_gap` 的 expected 被跳过，只说明该 public oracle 当前不参与该测试，不等于根输入不存在。

## 常用检查与收集命令

查看 live phase / 角色统计（不依赖平台特定 `find` 选项）：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 - <<'PY'
import json
from collections import Counter, defaultdict
from pathlib import Path

root = Path("cad-core/fixtures")
roles = json.loads(
    Path("cad-core/tools/freecad_expected_parity/fixture_roles.v1.json").read_text()
)["roles"]
by_phase = defaultdict(Counter)
for item in roles:
    by_phase[item["phase"]][item["role"]] += 1
for phase in sorted(p.name for p in root.iterdir() if p.is_dir()):
    inputs = len(list((root / phase).glob("*.json")))
    row = by_phase[phase]
    print(phase, inputs, row["native"], row["protocol_only"], row["unsupported"])
PY
```

安全地在 `/tmp` 收集单个 native fixture 的三侧产物：

```bash
cd /Users/li/Chili3DProject/FreeCAD
mkdir -p /tmp/freecad-expected
python3 cad-core/tools/collect_freecad_expected.py \
  cad-core/fixtures/c4m6/topo-state-body-tip-stable-recovery.json \
  --out /tmp/freecad-expected/topo-state-body-tip-stable-recovery.freecad.json \
  --validate-ledger
```

上例会同时写出：

```text
/tmp/freecad-expected/topo-state-body-tip-stable-recovery.freecad.json
/tmp/freecad-expected/topo-state-body-tip-stable-recovery.freecad.ledger.json
/tmp/freecad-expected/topo-state-body-tip-stable-recovery.freecad.producer-trace.json
```

对已完成 trace 迁移的 phase 检查 public / ledger 与 trace 闭包：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 cad-core/tools/collect_freecad_expected.py \
  --phase <phase> \
  --check \
  --skip-unsupported \
  --validate-ledger
```

当前只有 Body/Tip case 已入库 trace，因此 `--phase c4m6 --check` 会对其余已处理 native case 的缺 trace fail closed。这是 migration 未完成的真实结果，不应通过跳过或空文件绕过。

收集产生的 expected、ledger、trace 都是 native 工具的输出；不要手改 fixture 以“补齐”任一 sidecar。先用 `/tmp` 验证，只有明确要刷新该 native oracle 时才写回 `expected/`。

运行通用 public expected fixture 回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_expected_fixtures
```
