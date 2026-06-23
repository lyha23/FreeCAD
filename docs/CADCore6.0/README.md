# CADCore6.0

CADCore6.0 承接 C5.1 / C51X freeze 后仍有产品价值、但不能再写成 FreeCAD parity 的扩展项。当前首个主线是 PartDesign Pipe 的 transformation law 与 tangent expansion 产品扩展。

## 入口

- C6-M1 总入口：`C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线/6-23-19-42-C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线总入口.md`
- C6-M1 工作步骤：`C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线/工作步骤细分/`
- C6-M1 矩阵：`C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线/矩阵/`

## 当前状态

- C6-M1 为待执行方案包，尚未实现代码、fixture 或 capability 发布。
- 本阶段不恢复 C5 broad deferred，也不把 FreeCAD source-commented 分支写成 FreeCAD parity。
- Pipe `Transformation=Linear/S-shape/Interpolation` 与 `SpineTangent/AuxiliarySpineTangent` 若进入实现，必须按 CAD Core product extension 发布，并同步 request DTO、continuous-edge ledger、PipeShell history、focused tests 与 capability product-contract。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M1-PartDesignPipeProductExtension-TransformationLawAndTangentExpansion主线/工作步骤细分 --format markdown
```
