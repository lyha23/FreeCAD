# P8 DistanceTypeExtendedGeometry S2 范围准入与 blocker 矩阵

## 目标

按最小完整语义批次冻结 scope / blocker / backend / nonGoal 矩阵。S2 必须解释哪些 cases 同批实现，哪些 oracle-first，哪些 default / TODO 边界暂不发布。

## 必须完成

- `DTE-SCOPE-004..007` 作为显式 switch / radius / torus / point-curve 主批次，不得拆成单 fixture。
- `DTE-SCOPE-008..009` 作为 default / curve boundary 纳入同一包审计；若不实现，必须说明拆分原因和 reopen 条件。
- `DTE-BLOCK-001..008` 必须都有 close condition，不能只写 pending。
- backend gap 只能在已有 FreeCAD oracle 和 cad-core mismatch 后创建；当前预置多为 `notCollected` 或 `releaseGate`。

## 验收

```bash
rg -n 'DTE-SCOPE-00[1-9]|DTE-BLOCK-00[1-8]|DTE-BG-00[1-8]' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线/矩阵/*.tsv
```

## 非目标

- 不通过删 scope 缩小批次。
- 不把 oracle 缺失改写成 C++ backendGap。
