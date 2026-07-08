# cad-core fixtures 用例顺序说明

基线时间：2026-07-08 16:55。

本文说明 `cad-core/fixtures/` 的 phase 阅读顺序，以及每个 phase 内哪些文件算输入用例。当前工作树下共有 809 个根输入 fixture、501 个 `expected/*.freecad.json` oracle 文件、809 个 `cad-core-res/*.cad-core.json` 对照输出文件。

## 核心结论

`fixtures` 不是一个单独线性队列，而是先按顶层 phase 目录分组，再在每个 phase 内处理根输入 JSON。

- 语义阅读 / 排查顺序：先看 `p2`、`p3a`、`p3b`、`p4` 到 `p8`，再看后续 `cXmY` / `c51mY` 里程碑目录。
- 历史 `mvp` phase 已删除；当前根输入序列从 `p2` 开始。
- `fixtures/<phase>/*.json` 是根输入 fixture。
- `fixtures/<phase>/expected/*.freecad.json` 是 FreeCADCmd / native oracle 结果。
- `fixtures/<phase>/cad-core-res/*.cad-core.json` 是当前 `cad-core` 对照输出。
- `expected/` 和 `cad-core-res/` 不参与 `--phase` 的根输入遍历。

## 推荐 phase 顺序

这个顺序用于人工阅读、排查、写方案和解释阶段关系。

| 顺序 | phase | 当前根输入数 | 说明 |
| ---: | --- | ---: | --- |
| 1 | `p2` | 4 | 早期 PartDesign / Body 基础扩展。 |
| 2 | `p3a` | 7 | P3 前半段扩展。 |
| 3 | `p3b` | 29 | P3 后半段扩展。 |
| 4 | `p4` | 8 | typed property、link/sublist、placement 等基础合同。 |
| 5 | `p5` | 118 | Sketch / internal shape / solver-facing 状态等大批量 fixture。 |
| 6 | `p6` | 29 | stable subname、Body history、UpToFace 等拓扑引用 fixture。 |
| 7 | `p7` | 109 | PartDesign Pad/Pocket/Hole/Pattern/DressUp 等综合 fixture。 |
| 8 | `p8` | 91 | Part workbench、Link、Assembly 早期能力等 fixture。 |
| 9 | `c3m1` | 12 | C3-M1 里程碑。 |
| 10 | `c3m2` | 16 | C3-M2 里程碑。 |
| 11 | `c3m3` | 22 | C3-M3 里程碑。 |
| 12 | `c3m4` | 30 | C3-M4 里程碑。 |
| 13 | `c3m5` | 27 | C3-M5 里程碑。 |
| 14 | `c3m6` | 73 | C3-M6 里程碑。 |
| 15 | `c3m7` | 1 | C3-M7 里程碑。 |
| 16 | `c4m1` | 20 | C4-M1 里程碑。 |
| 17 | `c4m2` | 14 | C4-M2 里程碑。 |
| 18 | `c4m3` | 19 | C4-M3 里程碑。 |
| 19 | `c4m4` | 12 | C4-M4 里程碑。 |
| 20 | `c4m5` | 3 | C4-M5 里程碑。 |
| 21 | `c4m6` | 9 | C4-M6 里程碑。 |
| 22 | `c5m1` | 10 | C5-M1 里程碑。 |
| 23 | `c5m2` | 4 | C5-M2 里程碑。 |
| 24 | `c5m3` | 6 | C5-M3 里程碑。 |
| 25 | `c5m4` | 1 | C5-M4 里程碑。 |
| 26 | `c5m7` | 9 | C5-M7 里程碑。 |
| 27 | `c5m8` | 12 | C5-M8 里程碑。 |
| 28 | `c5m9` | 5 | C5-M9 里程碑。 |
| 29 | `c5m10` | 6 | C5-M10 里程碑。 |
| 30 | `c5m12` | 5 | C5-M12 里程碑。 |
| 31 | `c5m13` | 5 | C5-M13 里程碑。 |
| 32 | `c51m1` | 12 | C5.1-M1 里程碑；语义上按 C5.1 处理。 |
| 33 | `c51m2` | 5 | C5.1-M2 里程碑。 |
| 34 | `c51m3` | 3 | C5.1-M3 里程碑。 |
| 35 | `c51m4` | 4 | C5.1-M4 里程碑。 |
| 36 | `c51m5` | 16 | C5.1-M5 里程碑。 |
| 37 | `c6m1` | 6 | C6-M1 里程碑。 |
| 38 | `c6m3` | 3 | C6-M3 里程碑。 |
| 39 | `c6m4` | 4 | C6-M4 里程碑。 |
| 40 | `c6m5` | 7 | C6-M5 里程碑。 |
| 41 | `c6m6` | 2 | C6-M6 里程碑。 |
| 42 | `c6m7` | 2 | C6-M7 里程碑。 |
| 43 | `c7m1` | 0 | 当前为空目录；保留 phase 槽位。 |
| 44 | `c8m1` | 12 | C8-M1 里程碑。 |
| 45 | `c8m2` | 1 | C8-M2 里程碑。 |
| 46 | `c8m4` | 1 | C8-M4 里程碑。 |
| 47 | `c9m5` | 1 | C9-M5 里程碑。 |
| 48 | `c10m1` | 4 | C10-M1 里程碑。 |
| 49 | `c12m12` | 1 | C12-M12 里程碑。 |
| 50 | `c12m13` | 6 | C12-M13 里程碑。 |
| 51 | `c12m14` | 1 | C12-M14 里程碑。 |
| 52 | `c12m16` | 2 | C12-M16 里程碑。 |

## 单个 phase 内的文件边界

单个 phase 内，根输入 fixture 指 `fixtures/<phase>/*.json`。例如 `p2` 当前包含：

```text
body-basefeature-pad.json
pocket-open-sketch.json
pocket-without-base.json
rect-pad-pocket.json
```

`collect_freecad_expected.py --phase <phase>` 只收集 `fixtures/<phase>/*.json`，不会递归进入 `expected/` 或 `cad-core-res/`。

`tests/test_expected_fixtures.py` 扫描 `fixtures/*/expected/*.freecad.json`。遇到 `known_gap` 的 expected 会被跳过，不代表该 fixture 不存在。

## 常用检查命令

查看每个 phase 当前根输入数量：

```bash
cd /Users/li/Chili3DProject/FreeCAD
find cad-core/fixtures -maxdepth 2 -type f -name '*.json' \
  -not -path '*/expected/*' \
  -not -path '*/cad-core-res/*' \
  | awk -F/ '{count[$3]++} END {for (d in count) print d, count[d]}' \
  | sort
```

采集某个 phase 的 native expected：

```bash
cd /Users/li/Chili3DProject/FreeCAD
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd \
  python3 cad-core/tools/collect_freecad_expected.py \
  --phase <phase> \
  --check \
  --skip-unsupported
```

运行通用 expected fixture 回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_expected_fixtures
```
