# C11-M1 S3 FreeCADCmd 原生 LocationOverload 复采集

## 目标

复采集 FreeCAD `BRepOffsetAPI_MakePipeShellPy::add(Profile, Location, WithContact, WithCorrection)` 与 advanced combined helper 的 native oracle。S3 的输出只能是 stable native expected / `notCollected` / probe blocked 三类之一；S3 不改 cad-core C++。

## live baseline

本轮 S3 执行基线：

```text
pwd=/home/user/Chili3DProject/FreeCAD
HEAD=ff07bd9b83
git log -1 --oneline=ff07bd9b83 docs: 完成 C11-M1 S2 范围准入路由
git -c core.quotepath=false status --short -uall=<clean>
```

S3 起点工作区干净；本步只允许更新 C11-M1 S3 文档、总入口 / 索引必要状态、S3 指定矩阵行和当前 probe 输出证据。

## FreeCAD 依据

- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::BRepOffsetAPI_MakePipeShellPy::add()`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::setAuxiliarySpine()`
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` 只作为 native DocumentObject baseline，不作为 advanced wrapper property source。

## 范围

| scope | 要采集的 native 行为 | 结果路由 |
| --- | --- | --- |
| `C11M1-SCOPE-102` | `add(Profile, Location, WithContact, WithCorrection)` located profile shape summary。 | stable -> S4 comparison；失败 -> retained `notCollected`。 |
| `C11M1-SCOPE-104` | `setAuxiliarySpine(); setTolerance(); add(Profile, Location, WithContact, WithCorrection)` combined shape summary。 | 依赖 `C11M1-SCOPE-102` stable；否则 retained dependency。 |

## 必须回写的矩阵行

- `C11M1-SCOPE-102`
- `C11M1-SCOPE-104`
- `C11M1-BLOCKER-301`
- `C11M1-BLOCKER-302`
- `C11M1-CAT-101`
- `C11M1-CAT-103`
- `C11M1-VAL-301..303`

## 本轮 FreeCADCmd 复采集

复用旧 C5-M13 S2 probe：

```bash
cd /home/user/Chili3DProject/FreeCAD
FreeCADCmd -c "exec(compile(open('docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/docs/temp/6-22-04-46-c5m13-s2-sweep-location-combined-probe.py', encoding='utf-8').read(), 'c11m1_s3_sweep_probe.py', 'exec'))" > docs/temp/6-29-10-15-c11m1-s3-sweep-location-combined-probe-output.json
```

版本元数据查询：

```bash
cd /home/user/Chili3DProject/FreeCAD
FreeCADCmd -c "exec(compile('<inline FreeCAD/Part version probe>', 'c11m1_version_probe.py', 'exec'))" > docs/temp/6-29-10-15-c11m1-s3-freecadcmd-version.txt
```

原始证据：

- `docs/temp/6-29-10-15-c11m1-s3-sweep-location-combined-probe-output.json`
- `docs/temp/6-29-10-15-c11m1-s3-freecadcmd-version.txt`

版本记录：

| 项 | 本轮结果 |
| --- | --- |
| FreeCAD | `1.2.0 revision 20260519`；`BuildRevision=20260519 (Git shallow)`；`BuildRevisionDate=2026/05/19 20:59:45` |
| OCCT | `Part.OCC_VERSION=7.8.1` / `FreeCAD.ConfigGet("OCC_VERSION")=7.8.1` |
| LibPack | 当前 AppImage runtime 未暴露 LibPack：`LibPack=""`、`LibPackVersion=""`、`FreeCADLibs=""`、`env_FREECAD_LIBPACK=None` |
| runtime path | `AppHomePath=/tmp/.mount_FreeCA*/usr/` |

## S3 复采集结论

| scope | 结果 | 证据 | 后续路由 |
| --- | --- | --- | --- |
| `C11M1-SCOPE-102` located profile | `notCollected` | `located_free_vertex`、`located_profile_owned_vertex`、`located_profile_coordinate_free_vertex`、`located_spine_owned_vertex`、`located_open_wire_profile` 均进入 `is_ready_before_build=true` / `status_before_build=0` 后在 `builder.build()` 失败：`OCCError: NCollection_Array1::Value`。`located_add_before_frenet`、`located_add_before_transition`、`located_no_frenet`、`located_tolerance_before_add` 也失败；`plain_control` 成功返回 Shell shape summary。 | 保留 c5m10 historical guard；不新增 `cad-core/fixtures/c11m1` native expected；不打开 C++ gate。 |
| `C11M1-SCOPE-104` advanced combined | `dependency-retained` / `notCollected` | `combined_aux_tolerance_add`、`combined_tolerance_aux_add`、`combined_add_aux_tolerance`、`combined_aux_add_tolerance` 均在 `builder.build()` 失败：`OCCError: NCollection_Array1::Value`。`combined_no_location_control` 成功返回 Shell shape summary，说明本轮 blocker 仍依赖 located overload。 | 保留 C6-M4 product contract non-parity 与 c5m10 advanced combined guard；不把 combined wrapper source 升级为 backend gap。 |

S3 没有稳定 `shape_summary` 可作为 native oracle，因此不创建 C11-M1 native expected / fixture，不改写 c5m10 或 C6-M4 expected，不改 capability，不改 cad-core C++ / tests。S4 只能消费 `notCollected` / dependency-retained 结论，不能基于本步打开 implementation row。

## S3 矩阵回写

- `c11m1_part_sweep_location_overload_scope_review_matrix.tsv`：`C11M1-SCOPE-102` 标为 `notCollected_s3_reconfirmed`，`C11M1-SCOPE-104` 标为 `dependency_retained_s3`。
- `c11m1_part_sweep_location_overload_blocker_queue.tsv`：`C11M1-BLOCKER-301` 与 `C11M1-BLOCKER-302` 关闭为 S3 retained diagnostic；notCollected 不升级为 backend gap。
- `c11m1_part_sweep_location_overload_backend_gap_classification.tsv`：`C11M1-CAT-101` / `C11M1-CAT-103` 保持 no-C++ gate。
- `c11m1_part_sweep_location_overload_validation_matrix.tsv`：补齐 `C11M1-VAL-301..303` 的 S3 historical guard、FreeCADCmd probe 和 notCollected publication grep。

## S3 验收结论

S3 已按本文验收命令通过：两条 `rg` 均命中 historical guard / notCollected evidence，TSV field-count 检查通过，`git diff --check` 通过。本步骤未运行 cad-core build。

## 验收标准

本步骤优先使用 repo 现有 FreeCAD expected / probe / collector 风格。若需要新增临时 probe，输出必须落到 `docs/temp` 或本包记录中，并写清 FreeCAD / LibPack / OCCT 版本。

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'part-sweep-located-profile-contract|part-sweep-advanced-combined-contract|NCollection_Array1::Value|request_metadata_only' cad-core/fixtures/c5m10 cad-core/tests/test_p8_features.py cad-core/tests/test_expected_fixtures.py docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线
rg -n 'add\\(Profile, Location, WithContact, WithCorrection\\)|freecadcmd_location_overload_status|notCollected' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py docs/CADCore11.0/C11-M1-PartSweepLocationOverloadNativeParity复开批次/矩阵
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore11.0/C11-M1-PartSweepLocationOverloadNativeParity复开批次/矩阵/*.tsv
git diff --check
```

如果运行 FreeCADCmd：

```bash
cd /home/user/Chili3DProject/FreeCAD
FREECADCMD=${FREECADCMD:-FreeCADCmd} "$FREECADCMD" <probe-script>
```

通过条件：

- S3 文档记录 FreeCAD 版本、LibPack / OCCT 版本、probe 命令和最终结果。
- located profile 与 advanced combined 均被明确标成 stable oracle、`notCollected` 或 blocked；不能留空。
- 如果 stable，必须给出 shape summary / expected 产物位置和 S4 comparison route。
- 如果失败，必须保留 c5m10 / C6-M4 historical guard，不打开 C++ gate。

## 非目标

- 不把普通 no-location control 当 located overload parity。
- 不把 c6m4 product fixture 改写成 native expected。
- 不在 S3 修改 `cad-core/src`。
