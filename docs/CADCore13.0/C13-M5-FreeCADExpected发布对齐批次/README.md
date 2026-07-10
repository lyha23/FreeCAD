# C13-M5 FreeCADExpected 发布对齐批次

C13-M5 的目标是把 `cad-core` 的正式发布输出对齐到 fixture-role manifest 选中的 native `fixtures/<phase>/expected/*.freecad.json`。`fixture_roles.v1.json` 是 native / protocol-only / unsupported 的唯一机器来源；`*.expeted.json` 不进入 native release verdict，`.freecad.ledger.json` 不进入 runtime。

本批次承接 C13-M4：C13-M4 已关闭 `c4m6` child path public projection，但那只是 topoNamingState 最小闭环。C13-M5 要把同一原则推广成 release gate：每个 phase 先用统一比较器生成 cad-core 当前输出、规范化非语义漂移，再把差异归类为 runtime publication、feature geometry、diagnostics、oracle/collector 或环境兼容性问题。

## 当前结论

- S0 live inventory 已冻结：当前仓库有 42 个 phase、475 个 checked-in `expected/*.freecad.json`。
- 所有 expected 都有同名 input `fixtures/<phase>/<case>.json`，也都有同名当前输出 `fixtures/<phase>/cad-core-res/<case>.cad-core.json`。
- `expected/*.freecad.json` 是 native FreeCADCmd oracle；`cad-core-res/*.cad-core.json` 是 cad-core 当前实现输出，不能混放。
- `cad-core-res` 额外文件只记录 extra count，不参与 expected discovery，也不自动成为 release parity 缺口。
- 不能手改 expected 来追 cad-core；cad-core 输出应通过实现、发布策略或明确 known-gap 机制对齐 expected。
- FreeCAD raw mapped-name 中的 `:H...` token 允许随机漂移，严格比较必须先 canonicalize；对象集合、subshape 数量、diagnostic code、stableSubname、elementMap key 不能因 hash 漂移被放宽。
- 第一批 strict lane 仍选 `c4m6`，因为它覆盖 first recompute、Body/Tip recovery、CompoundLink、ReferenceShadow、schema/producer/hash/owner validation；mapperHistory probe 作为同批 protocol-only 合同保留。
- S1/S5 comparator 与生成入口已关闭：`freecad_expected_parity` 深模块统一 catalog、diff、registry、live source 与 current materialization；两个 CLI 只是 adapter。report schema 为 v2，`--write-current` 先验证 live JSON 再原子替换同名 current。
- S2 c4m6 strict red baseline 已关闭：每个 red diff 都带 `owner`、`owner_step`、`decision`、`freecad_authority`、`next_action`、`close_condition`，红灯基线曾记录 `runtime_publication_gap`=470、`mapper_history_publication_gap`=328、`stable_subname_diagnostic_policy`=13、`hash_mismatch_policy`=6、`protocol_decision_required`=5。
- S3/S5 topoNamingState 发布与 release 策略已关闭：schema、producer、document/object hash、object/child encoding 与 foreign top-level owner 均在 recompute 前 diagnostics-only hard fail。`c4m6` native scope 由 9 个 pair 组成，另有 1 个 protocol-only HistoryProbe；exact report 可以为 red，但只有 registry 中五个精确 transport selector 且 actual contract 成立时 semantic 才是 green、live release 才是 `protocol_divergence`。不存在 whole-result、whole-category、`.mesh` 后缀或 `results.subshapes` 的宽泛豁免。
- S4 phase family 扩展已关闭：comparator 已按五个语义家族输出 family-aware classification，并为每个 diff 增加 `source` 字段；首批 representative tranche 为 `c3m1`、`c10m1`、`c12m12`、`c3m5`、`c3m6`，均按 expected discovery 重生成同名 `cad-core-res` 并生成 strict classified red report。S4 不把这些 phase 标记 green，只把每个 family 的 known-gap id、原因、删除条件和下一步登记到矩阵。
- S5 release gate 已闭合：live binary、strict ledger preflight、role audit、registry audit、current freshness 与 registry-selected contract tests 必须同时成立；`protocol_divergence` 不被写成 exact green。
- 本轮不纳入无关 dirty 文件：`DESIGN.md`、`docs/框架/7-9-15-53-FreeCADCmd权威账本与topoNamingState裁剪原则.md`。

## S0 比较边界

Discovery 由 `cad-core/tools/freecad_expected_parity/fixture_roles.v1.json` 驱动：每个 input 必须恰好一个 role。native role 需要同名 `.freecad.json` + ledger，protocol-only 必须有 `.expeted.json`，unsupported 必须写 reason / authority / next action / close condition。role audit 失败或 0 native case 直接 `invalid`。

固定排除：

- `*.expeted.json`：focused protocol 合同，不是 native release input。
- `*.freecad.ledger.json`：expected provenance sidecar，不是 runtime 输入。
- `cad-core-res/*.cad-core.json` extra：不反向扩大 native discovery。

字段策略：

- strict：public object set、diagnostic code、results key、subshape path/type/count、stableSubname、mappedName.canonical、canonical elementMap key、childElementMaps key、mapperHistory public event identity、ReferenceShadow 边界。
- canonicalized：`mappedName.raw` 等 raw FreeCAD token 中的随机 `:H...` 片段，只在 comparator 内规整，不改 expected 和 runtime。
- tolerant：明确为数值输出的 bbox、placement、matrix、volume、area、length 等浮点字段，只允许小容差，不允许掩盖拓扑数量或诊断差异。
- provenance-only：`.freecad.ledger.json` 的 coverage/projection/roundTrip/inputReferences 不进入 response diff。CAD Core response transport metadata 不会被“忽略”；只有精确 `protocol_divergences.v1.json` selector、nested actual contract 与 consumer contract test 可形成 `protocol_divergence`。

## 批次目标

1. 建立 `fixtures/<phase>/expected/*.freecad.json` 到 `fixtures/<phase>/cad-core-res/*.cad-core.json` 的统一发现、生成和比较入口。
2. 明确 strict public expected 比较规则：哪些字段 canonicalize，哪些字段必须严格一致，哪些差异只能归入 known gap。
3. 先关闭 `c4m6` strict public parity，再按 phase 家族扩展到 Part primitives、Sketch/InternalShape、PartDesign Body/DressUp/Pattern、Assembly/App::Link。
4. 把差异归类落到 `runtime/topo_naming_state.cpp`、`runtime/recompute.cpp`、对应 feature executor、geometry/topo helper 或 collector/expected 证据，而不是在 adapter 或测试里修剪输出。
5. 形成 release gate：每个 phase 只有在 expected、cad-core-res、比较报告和 focused tests 都闭合后才能标记 green。

## 入口文件

- 方案：`7-10-00-15-C13-M5-FreeCADExpected发布对齐批次方案.md`
- 总入口：`7-10-00-15-C13-M5-FreeCADExpected发布对齐批次总入口.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 工作步骤

| 步骤 | 主题 | 关闭条件 |
| --- | --- | --- |
| S0 | expected inventory 与比较边界冻结 | 已实现：phase inventory、字段策略、非目标和首批 lane 已冻结。 |
| S1 | strict comparator 与 cad-core-res 生成入口 | 已实现：可按 phase 生成 cad-core-res，并输出 canonical strict diff report。 |
| S2 | c4m6 strict public parity 红灯基线 | 已实现：当前 c4m6 strict diff 被机器化记录，区分发布缺口和协议决策。 |
| S3 | topoNamingState 发布策略对齐 | 已实现：request integrity failures 在 recompute 前 hard fail；CompoundLink native semantic result 与 ReferenceShadow evidence-only 边界已固定。 |
| S4 | phase family 扩展 | 已实现：五个代表 phase 已生成 classified strict report；每个 family 都有 known-gap id、原因、删除条件和下一步。 |
| S5 | release gate 收口 | 已实现：v2 three-layer verdict、role/ledger/registry audit、live freshness、atomic current materialization 与 contract-test CLI 已闭合；S4 family known gaps 仍保持 red/known-gap，不伪装为 green。 |

## S4 family tranche

| 家族 | representative phase | strict status | 主要 known gap |
| --- | --- | --- | --- |
| TopoNamingState / ElementMap / App::Link | `c3m1` | red classified | `C13M5-S4-KG-TOPO-001`：ElementMap/childElementMaps/mapperHistory public state 与 subshape identity 仍未对齐。 |
| Sketch / InternalShape / split fragment | `c10m1` | red classified | `C13M5-S4-KG-SKETCH-001`：Sketch `object_fields`、`sketch_internal`、`sketch_external` 和 topo state release view 仍缺口。 |
| Part primitives / boolean / sweep / loft / pipe | `c12m12` | red classified | `C13M5-S4-KG-PART-001`：PartDesign Pipe 多线 sewing 的 topo state 与 subshape identity 需要后续 Part/geometry/topo 批次。 |
| PartDesign Body / dress-up / pattern / hole | `c3m5` | red classified | `C13M5-S4-KG-PD-001`：Body/Tip、DressUp、Pattern 历史账本已可归类，但 release public state 未 green。 |
| Assembly / placement / App::Link | `c3m6` | red classified | `C13M5-S4-KG-ASM-001`：Assembly solver DTO、placement writeback、native marker oracle 与 topo state release view 仍需 S5/后续实现拆分。 |

完整 status、decision、删除条件和下一步见 `矩阵/c13m5_expected_alignment_family_rollout_matrix.tsv`。这些红灯是 S4 的 known-gap surface，不是 expected 错误，也不允许通过手改 expected 或 fixture 名称分支关闭。

Focused test 边界：本轮已跑完整 `tests.test_freecad_expected_public_parity`、完整 `tests.test_topo_naming_state_response` 和代表 family 的 targeted methods；`test_feature_flows` 全模块仍有旧缺失 expected / p3b parity 失败，`c3m1` child-map 与 `c3m5` fillet-face volume 方法落在 S4 known-gap surface，完整 `test_p7_features` / `test_p8_features` 不作为 S4 release gate。

## 验收入口

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次/工作步骤细分 --format markdown
(cd cad-core && python3 tools/compare_freecad_expected.py --phase c4m6 --strict)
(cd cad-core && python3 tools/compare_freecad_expected.py --phase c4m6 --release-gate --run-contract-tests)
(cd cad-core && python3 -m unittest tests.test_freecad_expected_public_parity)
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次/矩阵/*.tsv
git diff --check -- docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次 docs/CADCore13.0/README.md
```
