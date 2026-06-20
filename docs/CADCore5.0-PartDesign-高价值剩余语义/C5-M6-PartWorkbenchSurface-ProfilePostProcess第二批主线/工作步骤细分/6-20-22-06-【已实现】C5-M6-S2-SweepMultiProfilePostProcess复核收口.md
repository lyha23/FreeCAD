# C5-M6-S2 Sweep MultiProfile / PostProcess 复核收口【已实现】

## 目标

复核并收口 `Part::Sweep` multi-profile `Sections` 与 `Linearize=true` 后处理。当前 live 代码已显示 expected-backed 支持；本步骤优先验证和补文档，只有证据缺失时才进入 oracle-first 实现。

## 必读

- `src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()`
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementPipeShell()`
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::linearize()`
- `cad-core/src/part/part_sweep.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/tests/test_p8_features.py`
- `cad-core/src/adapters/c_api/c_api.cpp`
- 本包矩阵。

## 工作内容

1. 验证 `part-sweep-multi-profile-linearize` fixture / expected / focused test。
2. 验证 `part-sweep-advanced-deferred` 仍输出 locatable diagnostics，不被误发布为 supported。
3. 若 expected-backed fixture 缺失，先采集 native FreeCAD expected，再补 cad-core。
4. 确认 capability 不再保留 `linearize_post_processing` 或 `multi_profile_sections_expected` 旧 gap。
5. 保留 auxiliary spine / support mode / binormal / location mode / tolerance contract 为后续 owner。

## S2 live 记录

基线命令输出：

```text
pwd
/Users/li/Chili3DProject/FreeCAD

git rev-parse --short HEAD
52add5fe8d

git log -1 --oneline
52add5fe8d docs: 收口 C5-M6 S1 Loft 复核

git -c core.quotepath=false status --short -uall
<clean>
```

复核结论：

- FreeCAD `src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` 读取 `Sections`、`Spine`、`Solid`、`Frenet`、`Transition`、`Linearize`；执行顺序是先把 spine 放入 `shapes`，再遍历 `Sections.getValues()` 加入全部 profile，调用 `result.makeElementPipeShell(...)`，最后在 `Linearize=true` 时调用 `result.linearize(LinearizeFace::linearizeFaces, LinearizeEdge::noEdges)`。
- FreeCAD `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementPipeShell()` 要求 `shapes.size() >= 2`，第一个输入转为 single wire spine，后续 profiles 逐个 `mkPipeShell.Add(...)`；`TopoShape::linearize()` 对 planar face 做 line/plane 后处理，当前 Sweep API 使用 `LinearizeEdge::noEdges`。
- cad-core `cad-core/src/part/part_sweep.cpp` 已按 `App::PropertyLinkList` 解析多 `Sections`，并把全部 sections 传给 `makeElementPipeShellFromSources(..., linearize)`；`cad-core/src/part/topo_shape_expansion.cpp` 对 profiles 循环 `pipeShell.Add(...)`，记录 `part_sweep:pipeshell_history`，并在 `Linearize=true` 时记录 `part_sweep:linearized_planar_faces` 或 `part_sweep:linearize_noop`。
- `cad-core/fixtures/c4m1/part-sweep-multi-profile-linearize.json` 使用 `LowerProfile` / `UpperProfile` 两个 Sections 且 `Linearize=true`；`cad-core/fixtures/c4m1/expected/part-sweep-multi-profile-linearize.freecad.json` 是 FreeCADCmd native expected；`cad-core/tests/test_p8_features.py` 对该 fixture 调用 `assert_object_matches_expected`。
- `cad-core/fixtures/c4m1/part-sweep-advanced-deferred.json` 只验证 `AuxiliarySpine` 与 `Tolerance` 输出 locatable `unsupported_property` diagnostics；它不是 supported shape，也不发布 advanced PipeShell wrapper contract。
- `cad-core/src/adapters/c_api/c_api.cpp` 的 `part_workbench.sweep.status` 为 `supported_multi_profile_linearize_expected_backed`；`cad-core/tests/test_adapters.py` 明确断言 `linearize_post_processing` 与 `multi_profile_sections_expected` 不在 remaining gaps。
- 本步未修改 C++ 或 fixture，关闭 `C5M6-BLK-003`；advanced PipeShell wrapper、auxiliary spine、support mode、binormal、location mode、tolerance contract 和 Hole internal PipeShell 继续留给后续 owner / non-goal。

## 非目标

- 不实现 advanced `BRepOffsetAPI_MakePipeShell` wrapper。
- 不把 Hole internal PipeShell 计入 Part::Sweep support。
- 不修改 Loft、Filling、GeomPlate。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters

cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M6-PartWorkbenchSurface-ProfilePostProcess第二批主线 docs/CADCore3.0 cad-core
```

完成后重命名为 `6-20-22-06-【已实现】C5-M6-S2-SweepMultiProfilePostProcess复核收口.md`。
