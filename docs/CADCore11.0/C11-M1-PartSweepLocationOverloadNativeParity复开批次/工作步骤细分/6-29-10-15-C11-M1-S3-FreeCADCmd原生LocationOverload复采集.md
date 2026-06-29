# C11-M1 S3 FreeCADCmd 原生 LocationOverload 复采集

## 目标

复采集 FreeCAD `BRepOffsetAPI_MakePipeShellPy::add(Profile, Location, WithContact, WithCorrection)` 与 advanced combined helper 的 native oracle。S3 的输出只能是 stable native expected / `notCollected` / probe blocked 三类之一；S3 不改 cad-core C++。

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
