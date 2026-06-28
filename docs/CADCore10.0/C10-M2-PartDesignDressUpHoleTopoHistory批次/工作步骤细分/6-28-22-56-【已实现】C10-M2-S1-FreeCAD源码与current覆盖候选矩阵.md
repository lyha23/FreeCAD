# 【已实现】C10-M2-S1 FreeCAD 源码与 current 覆盖候选矩阵

## 目标

复核 C10-M2 的 FreeCAD authority 和 current cad-core coverage，把 source candidate seed 改成当前仓库可追溯路径。S1 不把候选升级为 supported，不创建 backend gap。

## FreeCAD 依据

| 轴 | 路径 | 必查符号 |
| --- | --- | --- |
| DressUp base / AddSubShape | `src/Mod/PartDesign/App/FeatureDressUp.cpp` | `DressUp::getAddSubShape()`、`DressUp::getContinuousEdges()`、`DressUp::getFaces()` |
| Fillet | `src/Mod/PartDesign/App/FeatureFillet.cpp` | `Fillet::execute()` |
| Chamfer | `src/Mod/PartDesign/App/FeatureChamfer.cpp` | `Chamfer::execute()` |
| Draft | `src/Mod/PartDesign/App/FeatureDraft.cpp` | `Draft::execute()` |
| Thickness | `src/Mod/PartDesign/App/FeatureThickness.cpp` | `Thickness::execute()` |
| Hole | `src/Mod/PartDesign/App/FeatureHole.cpp` | `Hole::execute()`、`Hole::findHoles()`、`Hole::makeThread()` |
| Topo / Mapper | `src/Mod/Part/App/TopoShape*.cpp`、`src/Mod/Part/App/PropertyTopoShape.cpp` | `makeShapeWithElementMap`、MapperHistory、ElementMap 更新路径 |

## current cad-core 扫描轴

- `cad-core/src/part_design/feature_dress_up_support.h`
- `cad-core/src/part_design/feature_dress_up.cpp`
- `cad-core/src/part_design/feature_fillet.cpp`
- `cad-core/src/part_design/feature_chamfer.cpp`
- `cad-core/src/part_design/feature_draft.cpp`
- `cad-core/src/part_design/feature_thickness.cpp`
- `cad-core/src/part_design/feature_hole.cpp`
- `cad-core/src/part_design/body.cpp`
- `cad-core/include/cad_core/part/topo_shape.h`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/src/app/element_map.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_adapters.py`

## 必须回写的矩阵行

- `C10M2-SRC-101` 到 `C10M2-SRC-204`：确认 source file / symbol / source evidence / cad-core landing 仍准确。
- `C10M2-BLOCKER-101`：S1 完成后改为 `closed_s1`。
- 若某个 source path 不存在，不能静默删除，必须在 blocker queue 写明 replacement path 或 reopen condition。

## S1 复核结果

- `C10M2-SRC-101` 到 `C10M2-SRC-107` 已改为 live FreeCAD path + line anchor，并保留 `DressUp::getAddSubShape()`、`getContinuousEdges()`、`getFaces()`、Fillet / Chamfer / Draft / Thickness / Hole 调用链证据。
- `C10M2-SRC-201` 到 `C10M2-SRC-204` 已改为 current cad-core / tests / capability path + line anchor，覆盖 DressUp shared support、Hole producer history、Topo / ElementMap 和 P7 focused tests。
- seed 中的 `feature_dress_up_support.*` 已收敛为实际存在的 `feature_dress_up_support.h` 声明与 `feature_dress_up.cpp` 实现；没有保留不存在的 `.cpp` path。
- `C10M2-BLOCKER-101` 已关闭为 `closed_s1`。S1 未采 native oracle，未改 C++，未把任何行升级为 `supported` 或 `backendGap`。

## 推荐命令

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n "DressUp::getAddSubShape|DressUp::getContinuousEdges|Fillet::execute|Chamfer::execute|Draft::execute|Thickness::execute|Hole::execute|Hole::findHoles|Hole::makeThread|makeShapeWithElementMap" src/Mod/PartDesign/App src/Mod/Part/App
rg -n "DressUp|Fillet|Chamfer|Draft|Thickness|Hole|MapperHistory|ElementMap|StableSubList|ReferenceShadow" cad-core/src/part_design cad-core/src/part cad-core/src/app cad-core/include/cad_core/part cad-core/tests/test_p7_features.py cad-core/tests/test_adapters.py
```

## 验收标准

- `c10m2_dressup_hole_topohistory_source_candidates.tsv` 每一行都有真实 FreeCAD path 和 current cad-core landing。
- `C10M2-BLOCKER-101` 关闭为 `closed_s1`。
- `scope_review_matrix` 仍不出现未经证明的 `supported` / `backendGap` 结论。
- TSV 字段数检查通过：

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M2-PartDesignDressUpHoleTopoHistory批次/矩阵/*.tsv
git diff --check
```

验收通过后，S1 文件可重命名为 `6-28-22-56-【已实现】C10-M2-S1-FreeCAD源码与current覆盖候选矩阵.md`。

## 非目标

- 不采 native oracle。
- 不改 C++。
- 不用测试输出反推 FreeCAD 语义。
