# c3m4 native public parity 修复记录

## 结论

2026-07-10 完成 c3m4 已有 FreeCAD native expected 的 17 个 case 公共语义收口：

- strict ledger/preflight：17/17 有效；
- snapshot：`semanticStatus=green`，`unaccepted=0`；
- live：`semanticStatus=green`，`releaseGatePassed=true`；
- release gate：`releaseGatePassed=true`，返回码为 0；
- `geometry.numeric=0`，没有未接受的几何、拓扑、结果发布、stable identity、`topoNamingState` 或 diagnostics 差异。

`exactStatus=red` 仍然保留：741 项 exact diff 全部是已分类、可审计的表示差异（690）或 CAD Core 产品扩展（51），不是未接受的公共语义差异。对 diff 逐项审计后，741 项全是 CAD Core 额外发布的 `kind=extra`，没有 `value` 或 `missing` 差异；因此没有用表示层规则掩盖 expected 公共字段的改值或缺失。

## 范围

本轮只纳入同时存在 `*.freecad.json` 和 `*.freecad.ledger.json` 的 17 个 case：

- Filling：`part-filling-boundary-edges-default`、`part-filling-closed-wire-default`、`part-filling-invalid-inputs`；
- GeomPlate：`part-geomplate-curve-point-default`、`part-geomplate-invalid-inputs`；
- Loft：`part-loft-closed`、`part-loft-ruled`、`part-loft-solid`、`part-loft-two-section-surface`；
- Section：`part-section-stable-history`；
- Sweep：`part-sweep-frenet-off`、`part-sweep-open-profile-surface`、`part-sweep-right-corner-surface`、`part-sweep-solid`、`part-sweep-spine-subedges`、`part-sweep-transition-round-corner`、`part-sweep-transition-transformed`。

c3m4 其余 13 个无 native expected 的 fixture 未进入 parity。本轮未修改 `FreeCAD/src/`、native expected、ledger、collector 或 comparator。

## 根因与 FreeCAD 调用链

| 根因 | FreeCAD 公共语义依据 | CAD Core 落点 |
| --- | --- | --- |
| Loft/Sweep/Filling/GeomPlate 的生产者结果已在内部 metadata 中，但 official response 没有生产者拥有的 `object_fields`、`shape_summary` 和 native error envelope | `PartFeatures.cpp::Loft::execute()` / `Sweep::execute()` 经 `PropertyPartShape::setValue()` 发布 Shape 及 feature property；`AppPartPy.cpp::makeFilledFace()` 和 `GeomPlate/BuildPlateSurfacePyImp.cpp` 返回 transient helper 结果 | `ComputeContext::publicResultFields`、`part_feature_support.cpp::publishPartShape()`、各 Part producer 显式选择 native 公共子集，`runtime/recompute.cpp` 只负责合并 |
| Filling loose-edge 边界的连接策略和 wire fix 与 FreeCAD helper 不同 | `TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()` 先用 `makeElementWires(...requireSharedVertex)` / `ShapeAnalysis_FreeBounds::ConnectEdgesToWires()` 分割 wire，再经 `TopoShape::fix()` / `ShapeFix_Shape` 交给 `BRepOffsetAPI_MakeFilling` | `topo_shape_expansion.cpp` 复用相同连接、fix 和 maker 边界，不改写 OCCT 异常 |
| 同样报告 7.8.1 的 OCCT 二进制发行物对断开 Filling 输入返回了不同的底层异常帧 | FreeCADCmd 的 `Part.so` 实际加载 FreeCAD app bundled `libTKOffset.7.8.1.dylib`；Homebrew 同版本 dylib 的 SHA-256 不同，最小 C++ 探针可稳定复现两种原生异常 | `CMakeLists.txt` 增加 `CAD_CORE_OCCT_LIBRARY_ROOT`，允许 CAD Core 使用与 native oracle 相同的 OCCT runtime；没有 exception 字符串归一化 |
| helper 无效输入被 graph 或 parser 提前拦截，丢失 native collector 的 code/source/message 和 result error envelope | `AppPartPy.cpp::makeFilledFace()`、`GeomPlate/CurveConstraintPyImp.cpp`、`PointConstraintPyImp.cpp`、`PlateSurfacePyImp.cpp` 的入参与 wrapper lifecycle | `FeatureRegistry` 以通用 `MissingReferenceAdmissionPolicy::ProducerValidated` 仅将 missing-target admission 留给 transient helper，`ReferenceShadow` 恢复仍经 runtime 通用验证；Filling/GeomPlate producer 校验 property/DTO 并通过 carrier 发布 native error，runtime 只投影 carrier |
| GeomPlate helper 的未引用 source object 没有进入文档拓扑快照 | `Document.cpp::Document::recompute()` 重算 dependency-sorted 的 pending object，然后 transient helper 从 source document 构建结果 | `graph/recompute_plan.cpp` 仅对 Filling/GeomPlate transient helper target 补充未引用的非 helper source，不扩大普通 request 的 target 边界 |
| Section 的 producer-local `SEC` ElementMap 被 owner-qualified alias 覆盖，Edge3/Edge4 重排丢失了可审计映射，初次重算又产生了虚假 deleted history | `FeaturePartSection.cpp::Section::opCode()` 返回 `SEC`，`FeaturePartBoolean.cpp::Boolean::execute()` 将它交给 `TopoShape::makeElementShape()`，再由 `PropertyPartShape::setValue()` retag ElementMap；`ElementMap.cpp::encodeElementName()` 接受 producer-local IndexedName | `MakerHistoryOptions` 由 Section producer 明确声明 local alias 与“不把未映射 source 当作首次重算 deletion”；runtime 只通用识别无限定、maker-backed 的 local provenance，不删除 mapper history event |

所有实质 FreeCAD 语义改动都在相邻 C++ 注释中记录了对应源文件、类/函数和关键调用依据。实现不包含 fixture 名称分支、输出修剪、宽泛 ignore 或 synthetic name。

## 验证

直接公共语义 focused test 不导入 comparator/projector，而是运行 official CLI，直接比较 17 个 native response 的生产者字段、native error、diagnostic code/source、Section ElementMap/subshape、mapper history 和 topo object 集：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake -S . -B build \
  -DCAD_CORE_OCCT_LIBRARY_ROOT=/Applications/FreeCAD.app/Contents/Resources
cmake --build build --target cad-core -j4
python3 -m unittest tests.test_c3m4_native_public_parity
# Ran 8 tests: OK
```

现有 17 个 native case 对应的 `CadCoreP8FeatureTest` focused methods 全部通过；跨 phase 的 topoNamingState response 回归也通过：

```text
CadCoreP8FeatureTest c3m4 native methods: Ran 17 tests, OK
tests.test_topo_naming_state_response: Ran 17 tests, OK
tests.test_topo_state_fixture_migration: Ran 5 tests, OK
```

最终 artifact 与阶段命令：

```bash
python3 tools/compare_freecad_expected.py --phase c3m4 --write-current --bin build/cad-core
python3 tools/compare_freecad_expected.py --phase c3m4 --strict --output out/c3m4-snapshot.json
python3 tools/compare_freecad_expected.py --phase c3m4 --strict --live --bin build/cad-core --output out/c3m4-live.json
python3 tools/compare_freecad_expected.py --phase c3m4 --release-gate --run-contract-tests --bin build/cad-core --output out/c3m4-release-gate.json
```

三份报告均为 `preflight.valid=true`、17 case、`semanticStatus=green`、`unaccepted=0`、`geometry.numeric=0`；两份 live 报告均为 `releaseGatePassed=true`。所有 17 个 current artifact 的 `artifactEvidence.currentFresh=true`。

## 剩余风险

- raw/exact response 仍有表示层差异，因此不能把本轮解读为 byte-for-byte parity；但目前没有未接受公共语义差异。
- 核心异常帧可能取决于 OCCT 的具体二进制发行物，不能只比较 `OCC_VERSION_COMPLETE`。执行 native parity 时必须将 `CAD_CORE_OCCT_LIBRARY_ROOT` 指向生成 expected 的 FreeCADCmd 所用 OCCT runtime；CMake 会校验全套 library 都解析到该 prefix、real filename 版本与 header 一致，并在存在 conda manifest 时复核 manifest version。该选项生成的绝对 build RPATH 只用于本机 oracle 验收，不是可分发的 install contract；生产打包必须重定位同一套完整 OCCT runtime。若未对齐，gate 应保持 red，不得用异常字符串映射消除差异。
- 如未来扩大 transient helper 家族，应继续以 FreeCAD `Document::recompute()` 与具体 helper 调用链为依据，不应将普通 request 改为无条件全文档重算。
- c3m4 无 native expected 的 13 个 fixture 仍然是 probe/product-contract 范围，不能借用本轮结论宣称 native parity。
