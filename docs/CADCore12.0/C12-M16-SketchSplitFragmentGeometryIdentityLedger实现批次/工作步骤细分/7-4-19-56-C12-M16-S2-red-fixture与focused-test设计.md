# C12-M16 S2 red fixture 与 focused test 设计

## 目标

先补能失败的 focused tests / fixtures，锁定 split fragment ledger 的 public response 和 reference resolution 期望。

## 必读文件

- `../README.md`
- S1 已实现后的 step 文档和矩阵更新
- `../矩阵/c12m16_split_fragment_identity_contract_matrix.tsv`
- `../矩阵/c12m16_split_fragment_identity_implementation_matrix.tsv`
- `cad-core/tests/test_p5_sketch.py`
- `cad-core/tests/test_adapters.py`
- `cad-core/fixtures/p5/`

## 操作

1. 选择最小 split 场景：source geometry id 被 split 成多个 current fragments。
2. 新增或扩展 focused tests，先表达失败期望：`g<ID>:splitN` 出现在 edgeSegments / subshapes / raw ledger / reference update。
3. 需要 fixture 时放入 `cad-core/fixtures/c12m16/`，不要改旧 expected 除非证明旧 expected 本身错误。
4. 记录 red test 失败输出和实现落点。
5. 更新 contract / implementation / blocker / validation 矩阵。
6. 将本步骤重命名为 `【已实现】`。

## 关闭条件

- red tests 能约束 `g<ID>:splitN` response 与 reference resolution。
- fixture/test 命名清楚，覆盖 source one-to-many、fragment missing / reselect 和 missing id fallback。
- 下一步可直接实现 C++。

## 非目标

- 不让测试按 mesh/bbox/order 猜 fragment。
- 不改 production C++。
- 不刷新无关 expected。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest
cd ..
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/矩阵/*.tsv
git diff --check
```
