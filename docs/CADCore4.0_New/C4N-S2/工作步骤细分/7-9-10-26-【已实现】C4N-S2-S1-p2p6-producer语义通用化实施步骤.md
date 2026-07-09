# C4N-S2-S1 p2/p6 producer 语义通用化实施步骤

## 目标

关闭 `cad-core/tests/test_topo_naming_state_response.py` 中 p2 / p6 producer mapped-name expectedFailure，并保持 C4N-S1 c4m6 exact parity 不回退。

## 完成状态

- p2 / p6 producer mapped-name expectedFailure 已移除。
- `p2/rect-pad-pocket` Body 与 `p6/up-to-face-stable-body-history` ProbePad raw / canonical mapped name 已对齐 native expected。
- C4N-S1 c4m6 focused 回归仍在 `tests.test_topo_naming_state_response` 中通过。

## Step 1：基线复核

先跑当前 focused test，确认只有预期红线仍由 `expectedFailure` 隔离：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_topo_naming_state_response
```

若需要保留 cad-core 当前输出，只写到对应 phase 的 `cad-core-res/`：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
build/cad-core recompute fixtures/p2/rect-pad-pocket.json \
  --output fixtures/p2/cad-core-res/rect-pad-pocket.cad-core.json
build/cad-core recompute fixtures/p6/up-to-face-stable-body-history.json \
  --output fixtures/p6/cad-core-res/up-to-face-stable-body-history.cad-core.json
```

native expected 只做 check，不手改：

```bash
cd /Users/li/Chili3DProject/FreeCAD
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd \
  python3 cad-core/tools/collect_freecad_expected.py --phase p2 --check --skip-unsupported
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd \
  python3 cad-core/tools/collect_freecad_expected.py --phase p6 --check --skip-unsupported
```

## Step 2：建立 FreeCAD producer ledger

实现位置优先放在 `topo` / `part/topo`，不要放到 adapter 或测试输出修正层。

- 在 `cad-core/src/topo/freecad_mapped_name_codec.cpp` 补齐 FreeCAD `encodeElementName()` / `hashElementName()` 对应的编码边界。
- 在 `cad-core/src/part/topo_shape.cpp` 把现有矩形棱柱 seed 扩成 request-local producer ledger。
- ledger 至少表达 generated、modified、preserved、reverse lower pass、forward upper pass 的 entry 来源。
- `#id[:index]` 只表达本次 recompute 内的 StringID 关系，不能承诺跨请求稳定。

新增语义代码必须带 FreeCAD 来源注释，指向：

- `src/App/ElementMap.cpp::ElementMap::encodeElementName()`
- `src/App/ElementMap.cpp::ElementMap::setElementName()`
- `src/App/StringHasher.cpp::StringID::toString()`
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap()`

## Step 3：切 runtime publication

修改 `cad-core/src/runtime/topo_naming_state.cpp`：

- 只发布 producer ledger 中 source-backed 的 ElementMap entry。
- 保持 raw mapped name 与 FreeCAD expected 同风格；测试 canonical 化只用于比较。
- split、deleted、ambiguous 或 indexed-only 的对象不伪造 raw mapped name。
- Body / Tip / childElementMaps 保持 C4N-S1 已完成的发布边界。

## Step 4：移除 p2/p6 expectedFailure

当 p2/p6 raw / canonical 对齐 native expected 后，移除：

- `test_c13m2_p2_body_mapped_name_raw_canonical_matches_freecad_expected` 上的 `@unittest.expectedFailure`
- `test_c13m2_p6_probe_pad_mapped_name_raw_canonical_matches_freecad_expected` 上的 `@unittest.expectedFailure`

保留 indexed-only 边界：

- `p5/sketch-internal-face`
- `p8/app-link-box-face`

## Step 5：验收与文档收口

本轮必须通过：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_topo_naming_state_response
```

补丁检查：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore4.0_New cad-core
```

阶段收口才跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_expected_fixtures tests.test_adapters
```

完成后更新 `docs/CADCore4.0_New/README.md` 与 `C4N-S2/矩阵/c4n_s2_fixture_matrix.tsv`，再把主方案文件按仓库规则改名为 `【已实现】...`。
