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

## FreeCAD 调用链

- Loft：`FeatureLoft.cpp::Loft::execute()` 先通过 `getSectionShape("Profile")` 和 `Sections.getSubListValues()` 取 Profile / Sections；Sketch 非 `Vertex*` 子选择会回退整草图；每个 profile wire 调 `TopoShape::makeElementLoft(... IsSolid::notSolid, Ruled, Closed)`；`Closed` 只有 3 个及以上 profile/section 才生效；随后用 `BRepBuilderAPI_Sewing` 缝合 front/back face 和 shells，`MapperSewing(sewer)` 写入 element map，再 `makeElementSolid()`、`AddSubShape`、Body fuse/cut。
- Pipe：`FeaturePipe.cpp::Pipe::execute()` 取 `Profile`、`Spine`，`buildPipePath()` 接受选中 edges、Edge/Wire 或 edge/wire compound；`Transformation==Multisection` 时消费 `Sections`；`setupAlgorithm()` 映射 `Transition=Transformed/Right/Round`、`Mode=Fixed/Frenet/Auxiliary/Binormal`，Auxiliary 还读 `AuxiliarySpine` / `AuxiliaryCurvilinear`；shell 通过 `BRepBuilderAPI_Sewing` 补 front/back face 后 solidification，再写 `AddSubShape`。
- topo：`TopoShapeExpansion.cpp::makeElementLoft()` 使用 `BRepOffsetAPI_ThruSections`、`CheckCompatibility(Standard_True)` 和 `MapperThruSections`；`makeElementPipeShell()` 使用 `BRepOffsetAPI_MakePipeShell`；full front/back sewing 依赖 `MapperSewing::modified()`，不能用 adapter 输出修剪替代。

## 非目标

- 不把 Part Workbench `Part::Loft` / `Part::Sweep` capability 算作 PartDesign support。
- 不把 Hole internal PipeShell 混入 Pipe feature support。
- 不迁移 GUI task panel。
- 不用输出修剪替代 sewing MapperHistory。

## 实施结果

- Loft：C4 full-profile + one section first slice 保持 supported；本轮新增 Closed multi-section 和 multi-wire native oracle fixture，但 cad-core 在 closed / multi-wire bbox 上与 native FreeCAD 不等价，因此 expected 标记 known gap，运行时改为稳定 diagnostics。`Closed=true` 报 `unsupported_property` / `Closed`；multi-wire ordering 报 `unsupported_property` / `Sections`；`AllowCompound=false` multi-wire pressure 报 `multiple_solids_disallowed` / `AllowCompound`。
- Pipe：`Transformation=Multisection` + `Sections`、`Mode=Frenet` + `Transition=Right corner` 已 native expected-backed；`Mode=Auxiliary`、`AuxiliarySpine`、`AuxiliaryCurvilinear`、`Mode=Binormal`、`Binormal`、`Transformation=Linear/S-shape/Interpolation`、`SpineTangent` 保持稳定 `unsupported_property` diagnostics。`Transition=Round corner`、`Mode=Fixed` 和 full front/back `MapperSewing` 仍是后续 owner。
- capability：`cad_core_capabilities_json` 只发布 PartDesign Loft/Pipe 的上述边界，不把 Part Workbench `Part::Loft` / `Part::Sweep` 或 Hole internal PipeShell 计入支持范围。

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
