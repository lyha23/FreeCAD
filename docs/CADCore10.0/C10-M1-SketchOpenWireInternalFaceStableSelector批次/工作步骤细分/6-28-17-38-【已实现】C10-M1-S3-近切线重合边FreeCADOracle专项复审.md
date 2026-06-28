# C10-M1-S3 近切线重合边 FreeCAD Oracle 专项复审

## 目标

用 FreeCAD oracle 固定 near-tangent / coincident-edge sketch internal behavior，并只形成 oracle / route evidence。S3 不做产品决策，不改 C++，不修改 `profile_resolver.cpp`，不运行 cad-core build 或 full test。

## live 基线

| 命令 | S3 记录 |
| --- | --- |
| `pwd` | `/home/user/Chili3DProject/FreeCAD` |
| `git rev-parse --short HEAD` | `918c09ef8e` |
| `git log -1 --oneline` | `918c09ef8e docs: 完成 C10-M1 S2 范围准入矩阵` |
| `git -c core.quotepath=false status --short -uall` | 无输出，工作区干净。 |
| C10-M1 queue | 下一项为 S3。 |

## FreeCAD 依据

- `/home/user/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::buildInternals()`：先 `FaceMakerBuildFace`，再 `WireJoiner::getOpenWires()`。
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp::FaceMakerBuildFace::splitSelfIntersecting()` / `splitAtIntersections()` / `Build_Essence()`：负责 near tangent、coincident / near-overlap bounded region 的 split 与 face 生成。
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::checkIntersection()` / `splitEdges()` / `WireJoiner::getOpenWires()`：负责 touching open cutter 的 open-wire export 与 non-original fragment 语义。

## native collector 结果

| 项 | 结果 |
| --- | --- |
| `cad-core/fixtures/c10m1` 起始状态 | 不存在；S3 新增最小完整语义批次。 |
| FreeCADCmd 探测 | `command -v "$FREECADCMD" freecadcmd FreeCADCmd freecadcmd-daily` 返回 `/home/user/.local/bin/freecadcmd` 与 `/home/user/.local/bin/FreeCADCmd`。 |
| collector smoke | `python3 cad-core/tools/collect_freecad_expected.py cad-core/fixtures/p5/sketch-internal-face-through-open-cutter.json --check --freecadcmd /home/user/.local/bin/freecadcmd` 通过。 |
| C10M1 采集 | `cd cad-core && python3 tools/collect_freecad_expected.py --phase c10m1 --freecadcmd /home/user/.local/bin/freecadcmd` -> `processed=4 skipped=0 failed=0`。 |
| C10M1 check | `cd cad-core && python3 tools/collect_freecad_expected.py --phase c10m1 --check --freecadcmd /home/user/.local/bin/freecadcmd` -> `processed=4 skipped=0 failed=0`。 |
| `freecad_version` | `1.2.0 revision 20260519`，collector banner 为 `FreeCAD 1.2.0, Libs: 1.2.0devR20260519 (Git shallow)`。 |
| `notCollected` | 未发生；本轮没有环境 blocker。 |

## C10M1 oracle 批次

| probe 轴 | input fixture | expected | FreeCAD expected counts | current comparison route | S3 结论 |
| --- | --- | --- | --- | --- | --- |
| near tangent arc/line | `cad-core/fixtures/c10m1/sketch-near-tangent-arc-line.json` | `cad-core/fixtures/c10m1/expected/sketch-near-tangent-arc-line.freecad.json` | `InternalFace=2`、`InternalEdge=7`、`InternalVertex=8` | `./cad-core recompute fixtures/c10m1/sketch-near-tangent-arc-line.json --output /tmp/c10m1-sketch-near-tangent-arc-line.current.json`；统计 `results[0].subshapes` 后 current counts 同为 `2/7/8` | `native_expected_collected`，count-level `no_gap`。 |
| coincident shared edge | `cad-core/fixtures/c10m1/sketch-coincident-shared-edge.json` | `cad-core/fixtures/c10m1/expected/sketch-coincident-shared-edge.freecad.json` | `InternalFace=2`、`InternalEdge=7`、`InternalVertex=6` | `./cad-core recompute fixtures/c10m1/sketch-coincident-shared-edge.json --output /tmp/c10m1-sketch-coincident-shared-edge.current.json`；current counts 同为 `2/7/6` | `native_expected_collected`，count-level `no_gap`。 |
| touching open cutter | `cad-core/fixtures/c10m1/sketch-touching-open-cutter.json` | `cad-core/fixtures/c10m1/expected/sketch-touching-open-cutter.freecad.json` | `InternalFace=2`、`InternalEdge=8`、`InternalVertex=8` | `./cad-core recompute fixtures/c10m1/sketch-touching-open-cutter.json --output /tmp/c10m1-sketch-touching-open-cutter.current.json`；current counts 同为 `2/8/8` | expected-backed `no_gap`；没有 `current_mismatch_candidate`。 |
| near-overlap bounded region | `cad-core/fixtures/c10m1/sketch-near-overlap-rectangles.json` | `cad-core/fixtures/c10m1/expected/sketch-near-overlap-rectangles.freecad.json` | `InternalFace=3`、`InternalEdge=12`、`InternalVertex=12` | `./cad-core recompute fixtures/c10m1/sketch-near-overlap-rectangles.json --output /tmp/c10m1-sketch-near-overlap-rectangles.current.json`；current counts 同为 `3/12/12` | `native_expected_collected`，count-level `no_gap`。 |

current comparison 只在 native expected 固定后执行；S3 没有从 current 输出倒推 FreeCAD expected。当前比较只证明公开 `InternalFaceN` / `InternalEdgeN` / `InternalVertexN` count 匹配，不证明更细的 producer-history 语义已经无需 S6 release gate。

## P5 参考覆盖

- `cad-core/fixtures/p5/sketch-internal-face-line-arc-same-endpoints.json` / `expected/sketch-internal-face-line-arc-same-endpoints.freecad.json`：已有 line / arc same-endpoint oracle。
- `cad-core/fixtures/p5/sketch-internal-face-adjacent-rectangles.json` / `expected/sketch-internal-face-adjacent-rectangles.freecad.json`：已有 coincident shared boundary oracle。
- `cad-core/fixtures/p5/sketch-coincident-profile.json` / `expected/sketch-coincident-profile.freecad.json`：已有 coincident-constrained profile Pad oracle。
- `cad-core/fixtures/p5/sketch-internal-face-through-open-cutter.json` / `expected/sketch-internal-face-through-open-cutter.freecad.json`：已有 through open cutter oracle。
- `cad-core/fixtures/p5/sketch-internal-face-branch-open-cutter.json` / `expected/sketch-internal-face-branch-open-cutter.freecad.json`：已有 branch open cutter oracle。
- `cad-core/fixtures/p5/sketch-internal-face-arc-lens.json`、`sketch-internal-face-overlap-rectangles.json` 及 expected：已有 overlap / bounded region 参考。

S3 因 `cad-core/fixtures/c10m1` 起始不存在且 native FreeCADCmd 可用，仍新增了专项 C10M1 input / expected，而不是只把 P5 标为 `already_covered`。

## 矩阵回写结果

- `C10M1-SCOPE-101`：由 `native_oracle_required` 改为 `native_expected_collected`，记录四个 C10M1 expected、`freecad_version` 和 current count-match route。
- `C10M1-SCOPE-102`：由 S2 的 `backend_gap_candidate` 收窄为 S3 count-level `no_gap`。S3 未采到 producer-history current mismatch，S6 不得从该行直接打开 C++ gate。
- `C10M1-BLOCKER-301`：关闭为 `closed_s3`，原因是 native expected 已采集且 current public counts 全匹配；没有保留 `notCollected`。
- `C10M1-CAT-101`：改为 `native_expected_collected_no_gap`；S6 只做 no-code release gate 或未来新增 oracle 的 reopen，不做本轮 FaceMaker / WireJoiner 实现。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'near|tangent|coincident|touching|InternalFace|FaceMakerBuildFace|WireJoiner|freecad_version|notCollected' cad-core/fixtures/p5 cad-core/fixtures/c10m1 docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0 cad-core/fixtures/c10m1
git diff --check
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次/工作步骤细分 --format markdown
```

## 非目标

- 不实现 FaceMaker / WireJoiner C++。
- 不修改 `profile_resolver.cpp`。
- 不把 near-tangent / coincident 或 FaceMaker producer history 写成 supported / implementation-ready。
