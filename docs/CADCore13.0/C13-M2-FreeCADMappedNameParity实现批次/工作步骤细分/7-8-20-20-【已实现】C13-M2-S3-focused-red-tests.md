# 【已实现】C13-M2 S3 focused red tests

## 目标

先用 focused tests 锁定 mappedName evidence parity，再实现 codec/helper。

## 必读文件

- S2 输出
- `cad-core/tests/test_topo_naming_state_response.py`
- `cad-core/tests/test_adapters.py`
- focused fixtures 和 expected
- `cad-core/src/runtime/topo_naming_state.cpp`

## 操作

1. 新增 focused tests：`mappedName.raw/canonical` 不应只是 stable token fallback。
2. 对 p2 / c4m6 / p6 锁定 raw/canonical mappedName schema parity。
3. 对 child key / mapper ids 写明确 redline 或 blocker test。
4. 若当前实现尚未满足，使用 guarded expectedFailure 并写清 S4 删除条件。

## 关闭条件

- implementation matrix 标明测试对应实现落点。
- blocker queue 中 S3 blocker 关闭后才能进入 codec 实现。

## 关闭结果

- live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=696a4d0f29`（`696a4d0f29 文档：关闭 C13-M2 S2 证据矩阵`），起点 `git status --short -uall` 无输出。
- `cad-core/tests/test_topo_naming_state_response.py` 新增 5 个 guarded `unittest.expectedFailure` focused red tests：
  - `p2/rect-pad-pocket` 的 `Body` 必须与 expected FreeCAD `mappedName.raw/canonical` 和 entry key 集合一致。
  - `c4m6/topo-state-body-tip-stable-recovery` 的 `Body` 必须与 expected FreeCAD `mappedName.raw/canonical` 和 entry key 集合一致。
  - `p6/up-to-face-stable-body-history` 的 `ProbePad` 必须与 expected FreeCAD `mappedName.raw/canonical` 和 entry key 集合一致。
  - `p5/sketch-internal-face` 的 `Sketch` 必须保持 expected `indexed_only/0 entries`，不得发布 stable/display token fake raw mapped names。
  - `p8/app-link-box-face` 的 `BoxLink` 必须保持 expected `indexed_only/0 entries`，不得发布 Link display-path fake raw mapped names。
- 当前实现仍会发布 `Pad.Edge1`、`ProbeSketch.Edge1`、`g1`、`Box.Box.Face1` 这类 stable-token fallback，所以这 5 个测试以 `expectedFailure` 守住红线；S4/S5 实现通过时应出现 unexpected success，届时删除对应 `expectedFailure` 装饰器。
- 同文件新增 `test_c13m2_focused_expected_has_no_non_empty_s5_key_id_evidence_yet`，只断言 focused expected 当前没有非空 `childElementMapKey` / `mapperHistoryIds` evidence；S5 若采集到非空 evidence，必须替换为显式 child key / mapper id parity 测试，不能把空证据标绿。
- `C13M2-IMPL-004` 已在 implementation matrix 标为 `red_test_locked_s3`，`C13M2-BLOCKER-401` 已关闭；后续队列从 S4 `mappedName codec 实现` 开始。
- 本步未实现 codec/helper，未改 `cad-core/src/**`、fixtures、expected、collector、adapter 或 `cad-core-res`，未关闭 S4-S6，未把 S5 key/id 空 evidence 写成 supported。

## 删除条件

- S4 runtime publisher 改为消费 FreeCAD source-backed mapped-name codec 后，p2/c4m6/p6 三个 raw/canonical tests 应从 expected failure 变为普通通过；删除这三处 `@unittest.expectedFailure`。
- p5/p8 indexed-only no-fake-raw 边界被 runtime 正确守住后，删除这两处 `@unittest.expectedFailure`。
- S5 若 focused expected 出现非空 `childElementMapKey` 或 `mapperHistoryIds`，删除空证据守卫，改为显式 parity tests 和 blocker/implementation 状态。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_topo_naming_state_response
cd ..
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M2-FreeCADMappedNameParity实现批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M2-FreeCADMappedNameParity实现批次/矩阵/*.tsv
git diff --check
```

## S3 验证记录

- `cd cad-core && cmake --build build` 通过。
- `cd cad-core && python3 -m unittest tests.test_topo_naming_state_response` 通过，结果为 `Ran 8 tests`，`OK (expected failures=5)`。
- 队列、TSV 字段数和 `git diff --check` 作为 S3 关闭验收记录写入 validation matrix。
