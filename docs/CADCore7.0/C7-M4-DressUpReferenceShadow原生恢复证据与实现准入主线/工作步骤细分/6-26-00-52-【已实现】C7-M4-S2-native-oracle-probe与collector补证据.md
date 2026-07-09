# 【已实现】C7-M4 S2 native oracle probe 与 collector 补证据

## 目标

按 S1 设计执行 FreeCAD native probe 或补 collector 证据，产出 native oracle、native blocker 或 native not-supported 结论。S2 可以修改 oracle 工具、fixture 或 expected/blocker；不改 runtime C++ 主路径。

## 必读文件

- S1 完成后的本包 README、方案和矩阵。
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/fixtures/c3m5/dressup-reference-shadow-base-recovery.json`
- `cad-core/fixtures/c3m5/expected/dressup-reference-shadow-base-recovery.freecad.json`
- `cad-core/tests/test_p7_features.py`
- `src/App/PropertyLinks.cpp`
- `src/Mod/PartDesign/App/FeatureDressUp.cpp`

## 执行要点

1. 记录 live baseline 和 C7-M4 queue。
2. 按 S1 命令执行 native probe；如需新增 probe script，放在 `cad-core/tools/` 并保持只服务 oracle 采集。
3. 如果采到 native oracle，写入 expected/evidence JSON，必须记录 FreeCAD version、恢复前后 link state、shape summary 和 delete condition。
4. 如果无法证明 native restore，更新 known_gap JSON，保留 `oracle_blocked` 并写清 blocker 是 FreeCADCmd / Python API / collector 生命周期哪一层。
5. 更新 S2 相关矩阵和方案。
6. 标记本文件标题和文件名为 `【已实现】`，队列推进到 S3。

## 合法产物

- 可以新增或更新 `cad-core/tools/*reference*shadow*probe*.py`。
- 可以新增或更新 `cad-core/fixtures/c3m5/*reference-shadow*.json`。
- 可以新增或更新 `cad-core/fixtures/c3m5/expected/*reference-shadow*.freecad.json`。
- 可以新增 focused oracle tests；但不要把 current cad-core 输出写成 expected。
- 不允许改 `cad-core/src/part_design/feature_dress_up.cpp`、`cad-core/src/app` 或 `cad-core/src/part` 主路径。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
FREECADCMD=/Users/li/.cargo/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c3m5/dressup-reference-shadow-base-recovery.json --out /tmp/c7m4-dressup-reference-shadow-native-probe.json
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c7m3_reference_shadow_recovery_oracle_remains_blocked
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M4-DressUpReferenceShadow原生恢复证据与实现准入主线 docs/CADCore7.0/README.md
git diff --check
```

若 S1 替换了 probe 命令，以 S1/S2 矩阵记录的命令为准。

## 完成标准

- `dressup-reference-shadow-base-recovery` 有 native oracle、native blocker 或 native not-supported 结论。
- S2 不改 C++ runtime 主路径。
- 队列推进到 S3。

## S2 完成结论

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=dc041901a7`（`dc041901a7 文档：完成 C7-M4 S1 native probe 设计`），开始状态 `git status --short -uall` 无输出。
- route=`native_oracle_blocked`。`C7-M4 ReferenceShadow native probe（脚本已移除）` 已按 S1 设计构造 baseline FCStd、patch `Chamfer.Base` 的 `LinkSub/Sub value=OldFilletEdge1 shadow=Edge1`，并通过 FreeCADCmd reopen/recompute；命令 `returncode=0`，FreeCAD version=`1.2.0 revision 20260519`，`Chamfer` / `Body` shape summary 成功。
- 阻塞层：FreeCADCmd Python 可见 `Chamfer.Base=(Fillet, ["Edge1"])` 且 `dumpPropertyContent()` 可记录 reopen 后 property XML，但 Python property value 不暴露 `Base.getShadowSubs()`、`getSubValues(false)`、`getSubValues(true)`，因此不能证明 native ShadowSub lifecycle。
- evidence：`cad-core/fixtures/c3m5/dressup-reference-shadow-base-recovery.native-probe.evidence.json`；expected known_gap 和 `test_c7m3_reference_shadow_recovery_oracle_remains_blocked` 已更新。
- StableSubList-fed 负控命令已运行，`returncode=0`，输出 `/tmp/c7m4-dressup-reference-shadow-stablesublist-fed.freecad.json`；它只证明 Edge1-fed geometry，不能删除 native blocker。
- 本步骤未修改 `cad-core/src/app`、`cad-core/src/part`、`cad-core/src/part_design`、runtime/topo/adapter 等 C++ 主路径。
