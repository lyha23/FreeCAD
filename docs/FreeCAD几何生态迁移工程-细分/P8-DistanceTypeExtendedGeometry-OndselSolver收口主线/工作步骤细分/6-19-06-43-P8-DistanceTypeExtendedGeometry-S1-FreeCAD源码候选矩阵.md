# P8 DistanceTypeExtendedGeometry S1 FreeCAD 源码候选矩阵

## 目标

复核并补全 source candidates：DistanceType enum、classification、radius helpers、ASMT switch、cad-core DTO / resolver / collector / tests / capability。S1 只建立 authority，不裁决 supported。

## 必须完成

- 对 `p8_distance_type_extended_geometry_source_candidates.tsv` 逐行复核。
- 确认 FreeCAD enum 中所有剩余 cases 都被 scope matrix 覆盖。
- 把 `/Users/li/...` 和 `/home/user/...` 路径差异统一理解为同一源码树的路径前缀差异；后续相对路径、类/函数和关键短句才是依据。
- 对 cad-core 当前只支持 basic classifier 的事实做 live 记录。

## 验收

```bash
rg -n 'DistanceType|getDistanceType|getEdgeRadius|getFaceRadius|makeMbdJointDistance' src/Mod/Assembly/App/AssemblyUtils.h src/Mod/Assembly/App/AssemblyUtils.cpp src/Mod/Assembly/App/AssemblyObject.cpp
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线/矩阵/*.tsv
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线
```

## 非目标

- 不写 C++。
- 不采 oracle。
- 不把 source candidate 直接改成 supported。
