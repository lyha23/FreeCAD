# C7-M2 PartDesign Fillet Chamfer 复杂参数引用恢复收口主线

本目录是 CADCore7.0 的第二条主线。它不重开基础 Fillet / Chamfer 支持，而是专门收口 P7 文档里仍保留的 known gap：`Fillet / Chamfer 复杂参数组合、复杂引用变更后的完整稳定恢复`。

当前 P7 基线已经支持基础 Edge / Face Base、连续边过滤、OCCT fillet/chamfer maker、replacement solid、RefineModel、DressUp AddSubShape cache、slot 级 `NamedShape`、`SupportTransform=true` 和链式 DressUp 被 transformed family 消费。C7-M2 的价值是把复杂 Chamfer 参数、Fillet/Chamfer 多边选择、UseAllEdges、Body/DressUp 链式 Base 引用恢复和 expected/capability 发布口径放进同一批次裁决，避免在 executor、adapter 或输出层继续追加 fixture 特判。

## 入口

- 主线总入口：`6-25-16-20-C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线总入口.md`
- 方案：`6-25-16-20-C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- 方案创建基线：`HEAD=6ba500ea32`（`6ba500ea32 文档：完成 C7-M1 S5 发布闸门`）。
- 创建前 `git status --short -uall` 无输出；C7-M1 队列为空。
- S0-S5 尚未执行。下一步应先运行 S0，冻结当前 P7 capability、fixtures、tests 和 docs，而不是直接改 C++。
- 当前源码候选包括 `src/Mod/PartDesign/App/FeatureFillet.cpp`、`src/Mod/PartDesign/App/FeatureChamfer.cpp`、`src/Mod/PartDesign/App/FeatureDressUp.cpp`、`cad-core/src/part_design/feature_dress_up.cpp`、`cad-core/tests/test_p7_features.py` 和 `cad-core/fixtures/p7`。

## 收口边界

- Fillet：`Radius`、`UseAllEdges`、Base LinkSub、连续边扩展、multi-edge 选择和 replacement refine。
- Chamfer：`ChamferType` 的 `Equal distance`、`Two distances`、`Distance and Angle`，以及 `Size`、`Size2`、`Angle`、`FlipDirection`、`UseAllEdges`。
- DressUp 引用恢复：Body member Base、前序 Fillet/Chamfer Base、`StableSubList` / `FullSubList` 输入、`SupportTransform` AddSubShape cache、链式 DressUp source base。
- 发布口径：fixtures、expected、focused tests、capability/docs 必须一致。S2 之前只裁决，不实现。

## 非目标

- 不实现 GUI Fillet / Chamfer task panel 或交互选择器。
- 不扩展到 full DressUp universe，例如 Draft、Thickness 或全部 dress-up 类型。
- 不把任意拓扑命名和完整 MapperHistory 作为本包必须完成的目标。
- 不允许在 adapter、executor 输出端或 fixture 名称上做引用恢复猜测。
- 不覆盖 transformed family 全部复杂参数，只回归 `SupportTransform` / DressUp 链路中与 Fillet / Chamfer 相关的发布边界。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线
git diff --check
```
