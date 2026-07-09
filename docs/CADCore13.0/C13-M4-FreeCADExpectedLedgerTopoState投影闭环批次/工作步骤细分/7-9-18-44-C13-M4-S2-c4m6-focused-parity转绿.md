# C13-M4 S2 c4m6 focused parity 转绿

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

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 tools/validate_freecad_expected_ledger.py --phase c4m6 --strict
python3 -m unittest tests.test_topo_naming_state_response
cd ..
git diff --check
```
