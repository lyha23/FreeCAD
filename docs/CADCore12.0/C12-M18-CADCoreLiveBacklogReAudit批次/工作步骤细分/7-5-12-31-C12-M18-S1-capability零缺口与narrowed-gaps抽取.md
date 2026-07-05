# C12-M18 S1 capability 零缺口与 narrowed gaps 抽取

从 current capability 中抽取所有可作为 backlog 输入的结构化事实。

## 必读

- `../README.md`
- `../矩阵/c12m18_live_backlog_source_candidates.tsv`
- `../矩阵/c12m18_live_backlog_backend_gap_classification.tsv`
- `../../README.md`
- `../../C12-M9-CADCoreImplementationCandidate再盘点批次/README.md`
- `../../C12-M17-SubtractivePipeProductPipeLawShapeParity实现批次/README.md`
- `../../../../cad-core/src/runtime/capability_contract.cpp`
- `../../../../cad-core/tests/test_adapters.py`

## 操作

1. 生成 `/tmp/c12m18-capabilities.json`。
2. 抽取非空 `remaining_gaps` 和 `known_gaps`；当前预期为空。
3. 抽取所有 `narrowed_gaps` path 和 keys，写入 source / backend 矩阵。
4. 记录 publication authority 与 focused adapter assertions。
5. 不做 current mismatch 判断；S1 只建立输入清单。
6. 验证后把本文件重命名为带 `【已实现】` 的同名文件。

## 非目标

- 不把 `narrowed_gaps` 直接判成实现项。
- 不改 capability source。
- 不运行 FreeCADCmd。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
cad-core/build/cad-core capabilities > /tmp/c12m18-capabilities.json
jq -c 'paths as $p | select(($p[-1]? == "remaining_gaps") and ((getpath($p)|type)=="array") and ((getpath($p)|length)>0)) | {path:($p|join(".")), value:getpath($p)}' /tmp/c12m18-capabilities.json
jq -c 'paths as $p | select(($p[-1]? == "known_gaps") and (((getpath($p)|type)=="array" and (getpath($p)|length)>0) or ((getpath($p)|type)=="object" and (getpath($p)|length)>0))) | {path:($p|join(".")), value:getpath($p)}' /tmp/c12m18-capabilities.json
jq -c 'paths as $p | select($p[-1]? == "narrowed_gaps") | {path:($p|join(".")), keys:(getpath($p)|keys)}' /tmp/c12m18-capabilities.json
git diff --check
```

