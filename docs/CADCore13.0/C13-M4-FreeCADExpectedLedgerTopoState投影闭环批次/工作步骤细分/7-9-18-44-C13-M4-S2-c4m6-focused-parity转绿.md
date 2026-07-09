# 【已实现】C13-M4 S2 c4m6 focused parity 转绿

## 目标

把 S1 的 child path projection 改动纳入 focused parity gate，确认 `c4m6` 的 public topoNamingState 输出与 `.freecad.json` 对齐，同时 ledger sidecar gate 仍通过。

## 必读文件

- S1 输出
- `cad-core/tests/test_topo_naming_state_response.py`
- `cad-core/tools/validate_freecad_expected_ledger.py`
- `cad-core/fixtures/c4m6/expected/*.freecad.json`
- `cad-core/fixtures/c4m6/expected/*.freecad.ledger.json`

## 操作

1. 跑完整 `tests.test_topo_naming_state_response`，不要只跑单 case。
2. 若仍失败，先分类：projection gap、hash/version hard fail regression、ReferenceShadow regression、mapped-name producer blocker。
3. 只修 projection gap；如果失败属于 C13-M3 raw-key producer blocker，记录并回流，不在 C13-M4 内 fake。
4. 确认 hard fail fixtures 不发布 `topoNamingState`。
5. 确认 `ReferenceShadow.brep` 仍只作为 recovery evidence，不成为建模输入。

## 关闭条件

- `C13M4-FIX-001` 关闭。
- `C13M4-VAL-101` 与 `C13M4-VAL-102` 通过。
- `python3 -m unittest tests.test_topo_naming_state_response` 普通通过。
- `python3 tools/validate_freecad_expected_ledger.py --phase c4m6 --strict` 仍通过。

## S2 实现结果

- C13M4-BLOCKER-201、C13M4-IMPL-201、C13M4-VAL-101、C13M4-VAL-102 已关闭；c4m6 focused runtime parity 正式转绿。
- `python3 tools/validate_freecad_expected_ledger.py --phase c4m6 --strict` 验证 9 个 `.freecad.json` 与同名 `.freecad.ledger.json`，结果 9/9 green。
- `python3 -m unittest tests.test_topo_naming_state_response` 完整运行 14 个测试并通过，没有只跑单 case。
- hard fail 覆盖点仍在 focused test 中：schema、producer、documentHash、objectHash、object element-map encoding、child-map encoding 失败都断言不发布 `topoNamingState`。
- `topo-state-reference-shadow-brep` 的 ReferenceShadow 边界未变：StableSubList、ShadowSub、ReferenceShadow stableSubname 仍对齐 expected 合同；`ReferenceShadow.brep` 只作为旧 subshape recovery evidence，不作为建模输入。
- 本步未修改 expected/ledger JSON，未重采 oracle，未推进 S3 索引或最终包收口；队列下一步从 S3 开始。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 tools/validate_freecad_expected_ledger.py --phase c4m6 --strict
python3 -m unittest tests.test_topo_naming_state_response
cd ..
git diff --check
```
