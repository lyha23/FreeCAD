# C12-M18 CAD Core live backlog re-audit 批次总入口

按顺序执行 C12-M18 S0-S5。每一步都以 live repo 为准，不能从旧 C12-M9 或 C12-M17 结论直接推导实现范围。

## 目标

在 C12-M17 关闭后重新判断 FreeCAD/cad-core 是否存在下一轮可实现 backend gap。若没有，发布 no-code backlog gate 或把收益明确分流到 oracle/product-contract / my-chili3d frontend sync。

## 队列

| step | file | status |
| --- | --- | --- |
| 入口 | `工作步骤细分/7-5-12-29-【已实现】C12-M18工作步骤总入口.md` | implemented |
| S0 | `7-5-12-30-【已实现】C12-M18-S0-live基线与C12关闭口径冻结.md` | implemented |
| S1 | `7-5-12-31-【已实现】C12-M18-S1-capability零缺口与narrowed-gaps抽取.md` | implemented |
| S2 | `7-5-12-32-【已实现】C12-M18-S2-历史narrowed-gap三闸门复审.md` | implemented |
| S3 | `7-5-12-33-C12-M18-S3-产品扩展与frontend-sync分流裁决.md` | pending |
| S4 | `7-5-12-34-C12-M18-S4-next-package-authorization裁决.md` | pending |
| S5 | `7-5-12-35-C12-M18-S5-发布闸门与后续分流.md` | pending |

## 必读文件

- `../README.md`
- `../7-5-12-29-C12-M18-CADCoreLiveBacklogReAudit批次方案.md`
- `../../README.md`
- `../../C12-M17-SubtractivePipeProductPipeLawShapeParity实现批次/README.md`
- `../../../capability/7-5-00-14-cad-web-background非FreeCAD原生语义边界.md`
- `../../../../cad-core/src/runtime/capability_contract.cpp`
- `../../../../cad-core/tests/test_adapters.py`

## 输出规则

- 每一步必须更新对应矩阵和本包 README。
- 不要把 `narrowed_gaps` 直接写成 implementation gap。
- 不要重开 C12-M17，除非有新的 focused regression 或 expected/current mismatch。
- 不要把 PartDesign BSpline / 非 Line axis product extension 当作待修 bug。
- 若要建议 my-chili3d frontend sync，只在本包内记录分流，不改前端代码。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M18-CADCoreLiveBacklogReAudit批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M18-CADCoreLiveBacklogReAudit批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M18-CADCoreLiveBacklogReAudit批次 docs/CADCore12.0/README.md
git diff --check
```
