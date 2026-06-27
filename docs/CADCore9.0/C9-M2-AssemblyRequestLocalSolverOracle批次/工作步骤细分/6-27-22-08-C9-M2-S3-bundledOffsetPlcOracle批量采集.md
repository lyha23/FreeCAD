# C9-M2 S3 bundledOffsetPlc Oracle 批量采集

## 目标

批量采集 fixed-joint bundle 产生 non-identity `objectPartMap.offsetPlc` 的 native oracle，覆盖 object marker、subshape marker 和 solver writeback 三类场景。

## FreeCAD 依据

- `src/Mod/Assembly/App/AssemblyObject.h::AssemblyObject::MbDPartData::offsetPlc`
- `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::getMbDData()`
- `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::handleOneSideOfJoint()`
- `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::validateNewPlacements()`
- `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::setNewPlacements()`

## 必须覆盖的 oracle cases

| case | fixture 目标 | 必须证明 |
| --- | --- | --- |
| object marker | `assembly-bundled-offset-object-marker-real-solver` | `objectPartMap.offsetPlc` 非 identity，marker 使用 `data.offsetPlc * plc`。 |
| subshape marker | `assembly-bundled-offset-subshape-marker-real-solver` | subshape JCS 经过 object-global 到 part-local，再应用 bundled offset。 |
| writeback | `assembly-bundled-offset-placement-writeback-real-solver` | `documentObjectUpdates` 反映 `getMbdPlacement(mbdPart) * offsetPlc`。 |

## 必须回写的矩阵行

- `C9M2-SCOPE-101`
- `C9M2-SCOPE-102`
- `C9M2-SCOPE-103`
- `C9M2-BG-101`
- `C9M2-BG-102`
- `C9M2-BG-103`
- `C9M2-BLOCKER-301`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'offsetPlc|objectPartMap|getMbDData|handleOneSideOfJoint|setNewPlacements|validateNewPlacements' src/Mod/Assembly/App cad-core/fixtures/c3m6 cad-core/tests/test_p8_features.py
test -f cad-core/fixtures/c3m6/expected/assembly-bundled-offset-object-marker-real-solver.freecad.json
test -f cad-core/fixtures/c3m6/expected/assembly-bundled-offset-subshape-marker-real-solver.freecad.json
test -f cad-core/fixtures/c3m6/expected/assembly-bundled-offset-placement-writeback-real-solver.freecad.json
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次/矩阵/*.tsv
git diff --check
```

S3 关闭时，三个 bundled `offsetPlc` cases 必须被路由为 expected-backed current match、backend_gap_candidate 或明确 native oracle blocker；不得只采一个代表 fixture 后关闭整步。

## 非目标

- 不用 current cad-core 输出刷新 expected。
- 不靠 fixture 名称、bbox、角度或输出排序猜测 offset。
- 不实现 C++，除非 S6 已确认 native oracle + current mismatch。
