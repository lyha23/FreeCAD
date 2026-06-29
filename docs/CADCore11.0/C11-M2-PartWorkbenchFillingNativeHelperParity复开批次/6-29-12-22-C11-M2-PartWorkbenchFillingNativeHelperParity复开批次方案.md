# C11-M2 PartWorkbench Filling NativeHelper Parity 复开批次方案

## 背景

C11-M1 已把 Sweep Location overload 的复开线关闭为 no-code retained non-parity：当前 FreeCADCmd 对 located overload 仍不可采，不能把 C6-M4 product contract 升级成 native parity。

C11-M2 转向同属 Part Workbench surface builder 的 `Part.makeFilledFace`。C6-M5 已把 Surface、Supports/Orders、ExplicitParams 和 non-boundary support/order 发布为 CAD Core request-local product contract，并把 `remaining_gaps` 清空；但 C6-M5 保留了 six native helper evidence，说明当时 `Part.makeFilledFace` helper 的部分 native oracle 仍是 crash、timeout、ConstructionError 或 `notCollected`。

本批次目标是重新判断这些 helper evidence 在当前 FreeCAD / OCCT 基线下是否还能稳定采集。如果能采到 stable native expected，再进入 comparison 和可能的 C++ 落点；如果仍不可采，则保持 C6-M5 product contract non-parity，不新增实现。

## FreeCAD 调用链

- `src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()`：Python helper 入口，解析 `shapes`、`surface`、`supports`、`orders` 和全部 filling params，然后调用 `TopoShape::makeElementFilledFace(...)`。
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()`：构造 `BRepOffsetAPI_MakeFilling`，按 boundary、support、order、non-boundary source 组织 `LoadInitSurface` 和 `Add(...)` 调用，再 `Build()`。
- `src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp`：direct wrapper 控制面，暴露构造参数、`loadInitSurface`、`add`、`setConstrParam`、`setResolParam`、`setApproxParam`、`build` 和 `shape`，用于对照 helper oracle，不作为跨请求 mutable builder 合同。

## cad-core 当前边界

- `cad-core/src/part/part_filling.cpp` 已支持 source-backed `Part::FilledFace` helper DTO，解析 Boundary、Surface、Supports、Orders、ExplicitParams 和 non-boundary source，输出 object_fields 与 locatable diagnostics。
- `cad-core/src/part/topo_shape_expansion.cpp` 已实现 request-local `BRepOffsetAPI_MakeFilling` builder route，覆盖 `LoadInitSurface`、boundary edge support/order、non-boundary IsBound=false、face/order 和 point constraint evidence。
- `cad-core/src/runtime/capability_contract.cpp` 发布 `part_workbench.filling.status=supported_expected_backed_plus_c6m5_product_contract_non_parity`，`remaining_gaps=[]`，six helper evidence 仍在 `narrowed_gaps` / `historical_native_helper_evidence`。
- `cad-core/tests/test_p8_features.py` 与 `cad-core/tests/test_adapters.py` 已锁定 C6-M5 product contract 和 retained evidence，不允许把 product contract 误报为 FreeCAD parity。

## 实施原则

- 先 native helper oracle，后 parity comparison；没有 stable `shape_summary` 不打开 C++ gate。
- 先区分 helper DTO、direct wrapper 和 native DocumentObject：`Part.makeFilledFace` 是 Python helper，当前不声明 upstream native `Part::FilledFace` DocumentObject parity。
- 同一最小完整语义批次覆盖 Surface、Supports/Orders G1/G2、ExplicitParams 和 non-boundary support/order；不把 Filling 再拆成单个 fixture 小批次。
- S6 只消费 `notCollected`、`backendGap`、`unsupported` 和 `releaseGate`。`notCollected` 不能自动变成 `backendGap`。
- 禁止 fixture 名称分支、bbox/面积/输出顺序修补、adapter 层业务逻辑、cross-request TopoDS/BREP/wrapper state。

## S0-S6 拆分

| 步骤 | 目标 | 关键输出 |
| --- | --- | --- |
| S0 | 冻结 live baseline 与声明口径 | README、总入口、capability grep、dirty boundary、C11-M1/C6-M5 继承口径和 validation matrix 对齐。 |
| S1 | 复核 FreeCAD 源码与 helper 候选 | source candidate TSV 回写 `makeFilledFace`、`makeElementFilledFace`、`BRepOffsetAPI_MakeFillingPyImp` 和 current cad-core path。 |
| S2 | 做范围准入与 blocker 路由 | scope / blocker / nonGoal / backend-gap TSV 全部有 owner step 和 close condition。 |
| S3 | FreeCADCmd 原生 Filling helper 复采集 | 重新采 Surface、Supports/Orders G1/G2、ExplicitParams all params、non-boundary support/order；写入 stable oracle 或 `notCollected`。 |
| S4 | ProductContract 到 Parity 升级审计 | 只在 S3 stable oracle 存在时比较 native expected 与 C6-M5 current product contract；产生 `no_gap`、`diagnostic_retained` 或 `backend_gap_candidate`。 |
| S5 | 协议边界与 non-goal 复审 | 明确 native DocumentObject、Surface Workbench GUI、persistent wrapper lifecycle、direct wrapper UV branch、adapter fixup 不进入本批次。 |
| S6 | Oracle 实现与发布闸门 | 有 backend gap 则落 C++ / fixtures / focused tests / capability；否则发布 no-code retained non-parity release gate。 |

## S6 代码落点规则

S6 只有在 S3-S4 产生 `backend_gap_candidate` 时才改代码。允许落点包括：

- `cad-core/src/part/part_filling.cpp`：DTO 解析、locatable diagnostics、metadata、Surface / Supports / Orders / params / non-boundary product fields。
- `cad-core/include/cad_core/part/topo_shape_expansion.h` 与 `cad-core/src/part/topo_shape_expansion.cpp`：`FilledFaceBuildInput`、`BRepOffsetAPI_MakeFilling` builder route、maker history 和 constraint evidence。
- `cad-core/src/runtime/capability_contract.cpp`：从 non-parity / narrowed evidence 升级或保留状态。
- `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_expected_fixtures.py`、`cad-core/tests/test_adapters.py`：focused parity / diagnostic / capability assertions。
- `cad-core/fixtures/c11m2` 或 current package documented fixture route：只有 native oracle stable 时才新增 expected。

禁止在 adapter 层补业务语义、按 fixture 名称分支、按 bbox/面积/输出顺序判定 Filling 结果、删除 C6-M5 historical helper evidence，或把 Surface Workbench GUI / mutable wrapper lifecycle 当作 headless CAD Core support。

## 验收分层

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore11.0/C11-M2-PartWorkbenchFillingNativeHelperParity复开批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore11.0/C11-M2-PartWorkbenchFillingNativeHelperParity复开批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore11.0
git diff --check
```

代码闸门触发后：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

重型收口只在 S6 实际修改 collector、fixtures、capability 或核心 C++ 后执行；docs-only 或 `notCollected` gate 不跑 cad-core build。
