# C13-M5 S5 release gate 收口

## 目标

把完成的 phase 变成可持续 release gate：expected、cad-core-res、diff report、focused tests 和 known-gap registry 都闭合。

## 必做

1. 更新 phase matrix，把每个 phase 标成 green / red / known_gap / unsupported / protocol_divergence。
2. 对 green phase 固定验收命令。
3. 对 known gap 写明：
   - FreeCAD source authority。
   - cad-core 缺口落点。
   - 为什么不是 expected 错误。
   - 删除条件。
4. 对 protocol divergence 写明：
   - native expected 行为。
   - cad-core protocol 选择。
   - 前端消费影响。
   - 是否需要另设 `.expeted.json` 合同。
5. 保证 `cad-core-res` 与当前 cad-core 版本一致。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次/矩阵/*.tsv
cd cad-core
python3 tools/compare_freecad_expected.py --phase <green-phase> --strict
```
