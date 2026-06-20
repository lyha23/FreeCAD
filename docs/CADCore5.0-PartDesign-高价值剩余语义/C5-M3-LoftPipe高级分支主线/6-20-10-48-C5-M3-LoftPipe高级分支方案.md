# C5-M3 Loft / Pipe 高级分支方案

## 目标

把 C4 中 PartDesign Loft / Pipe 的 first slice 扩展到产品需要的高级分支。Loft 和 Pipe 共享 Body replay、AddSubShape、solidification、sewing history 和 AllowCompound 风险，但 FreeCAD 调用链不同，实施时必须按子族拆清。

## 范围

- Loft 源码依据：`src/Mod/PartDesign/App/FeatureLoft.cpp`、`FeatureSketchBased.cpp`、`Body.cpp`。
- Pipe 源码依据：`src/Mod/PartDesign/App/FeaturePipe.cpp`、`FeatureSketchBased.cpp`、`Body.cpp`。
- topo 依据：`src/Mod/Part/App/TopoShapeExpansion.cpp::makeElementLoft()`、`makeElementPipeShell()`、`MapperSewing`。
- cad-core 落点：`cad-core/src/part_design/feature_loft.*`、`feature_pipe.*`、`cad-core/src/part/topo_shape_expansion.cpp`、`cad-core/src/part/topo_shape.cpp`、`cad-core/src/adapters/c_api/c_api.cpp`。

## 最小完整语义批次

本包可以拆成 Loft 和 Pipe 两个 sequential worker，但不能只做单个 fixture：

- Loft：Closed multi-section、multi-wire ordering、explicit Sections subelement selection、sewing / solidification、AllowCompound diagnostics。
- Pipe：Sections、AuxiliarySpine、AuxiliaryCurvilinear、Binormal、Mode、Transformation、Transition、SpineTangent、PipeShell setupAlgorithm、sewing / solidification。
- 共享：Body Tip replacement、AddSubShape、MapperSewing / MapperThruSections source ownership、capability metadata。

## 阶段

| 步骤 | 内容 |
| --- | --- |
| S0 | source audit：Loft 和 Pipe advanced 分支拆分 |
| S1 | Loft native oracle / fixture / diagnostics |
| S2 | Pipe native oracle / fixture / diagnostics |
| S3 | cad-core implementation、topo history、capability metadata 和 focused tests |

## 非目标

- 不把 Part Workbench `Part::Loft` / `Part::Sweep` capability 算作 PartDesign support。
- 不把 Hole internal PipeShell 混入 Pipe feature support。
- 不迁移 GUI task panel。
- 不用输出修剪替代 sewing MapperHistory。

## 验收

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义 cad-core
```

阶段回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```
