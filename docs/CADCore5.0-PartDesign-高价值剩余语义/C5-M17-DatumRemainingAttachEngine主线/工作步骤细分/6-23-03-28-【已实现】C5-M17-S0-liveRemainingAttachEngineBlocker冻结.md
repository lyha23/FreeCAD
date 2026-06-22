# 【已实现】C5-M17-S0 live Remaining AttachEngine Blocker 冻结

## 目标

冻结 C5-M17 的 live blocker：只处理 C5-M16 之后仍在 `datum_attach_engine_remaining_modes` 中的剩余项，并确认哪些可以进入 conic landmark 批次。

## 输入

- `cad-core/src/adapters/c_api/c_api.cpp`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/cadcore5_*`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M17-DatumRemainingAttachEngine主线/矩阵/*.tsv`

## 必做

1. 运行 `rg -n 'datum_attach_engine_remaining_modes|Folding|Directrix1|Asymptote1|TangentU|Focus1|IntersectionPoint' cad-core/src/adapters/c_api/c_api.cpp`。
2. 记录 live modes 是否仍为 `Folding`、`Directrix1/2`、`Asymptote1/2`、`TangentU/V`、`Focus1/2`、`IntersectionPoint`。
3. 更新 package-local source/scope/blocker/oracle/non-goal/validation 矩阵中的 S0 行。
4. 确认 C5-M14/C5-M15/C5-M16 已关闭 modes 不被重新加入 blocker 或实现范围。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'datum_attach_engine_remaining_modes|Folding|Directrix1|Asymptote1|TangentU|Focus1|IntersectionPoint' cad-core/src/adapters/c_api/c_api.cpp
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M17-DatumRemainingAttachEngine主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
```

## 非目标

- 不采集 FreeCAD expected。
- 不改 C++。
- 不移除 capability exact blocker。
