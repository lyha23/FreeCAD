# 【已实现】C13-M5 S0 inventory 与比较边界冻结

## 目标

冻结 `fixtures/<phase>/expected/*.freecad.json` 的发现规则、比较边界和首批 strict lane，避免后续实现时把 protocol contract、cad-core-res 额外文件或 sidecar ledger 混进 release parity。

## 必做

1. 枚举所有含 `.freecad.json` 的 phase，记录 expected count、当前 cad-core-res count、input path 是否齐备。
2. 定义 comparator 字段策略：
   - strict：object set、diagnostic code、subshape path、stableSubname、canonical elementMap key。
   - canonicalized：`mappedName.raw` 中的随机 `:H...` token。
   - tolerant：明确为数值输出的 bbox / placement / volume 等浮点字段。
   - ignored-with-evidence：只在 sidecar ledger 或 collector metadata 中存在、不是 public runtime output 的字段。
3. 首批 lane 固定为 `c4m6`，因为它覆盖 topoNamingState release publication 的代表性边界。
4. 记录无关 dirty 文件，不纳入本批次。

## 实现结果

- Live discovery 命令固定为 `find cad-core/fixtures -path '*/expected/*.freecad.json' -type f | sort`。
- 当前发现 42 个 phase、475 个 checked-in `expected/*.freecad.json`。
- 所有 expected 都有同名 input `fixtures/<phase>/<case>.json`。
- 所有 expected 都有同名当前输出 `fixtures/<phase>/cad-core-res/<case>.cad-core.json`。
- `cad-core-res` 额外文件只在矩阵记录 `cad_core_res_extra_count`，不参与 expected discovery，也不自动成为 release parity 缺口。
- 首批 lane 固定为 `c4m6`。
- 无关 dirty 文件 `DESIGN.md`、`docs/框架/7-9-15-53-FreeCADCmd权威账本与topoNamingState裁剪原则.md` 不纳入本批次。

## 字段策略

- strict：public object set、diagnostic code、results key、subshape path/type/count、stableSubname、mappedName.canonical、canonical elementMap key、childElementMaps key、mapperHistory public event identity、ReferenceShadow 边界。
- canonicalized：`mappedName.raw` 等 raw FreeCAD token 中的随机 `:H...` 片段，只在 comparator 内规整，不改 expected 和 runtime。
- tolerant：明确为数值输出的 bbox、placement、matrix、volume、area、length 等浮点字段，只允许小容差，不允许掩盖拓扑数量或诊断差异。
- ignored-with-evidence：只存在于 `.freecad.ledger.json` sidecar、collector coverage/projection/roundTrip/inputReferences 等 provenance 字段，或 cad-core response 中非 native public expected 的 transport/adapter metadata；忽略时必须能指向证据来源。

## 非目标

- 不写 C++。
- 不重采 FreeCAD expected。
- 不重生成 cad-core-res。
- 不把全量 diff 当作一个实现任务。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
find cad-core/fixtures -path '*/expected/*.freecad.json' -type f | sort
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次/矩阵/*.tsv
git diff --check -- docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次 docs/CADCore13.0/README.md
```
