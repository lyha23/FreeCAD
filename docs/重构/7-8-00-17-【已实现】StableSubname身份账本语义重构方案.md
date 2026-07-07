# StableSubname 身份账本语义重构方案

## 背景

当前 `cad-core` 已经开始输出 `subname`、`fullSubname`、`stableSubname` 和 `identityStatus`，但身份语义还没有收敛到一个统一账本。几个关键路径仍在用“当前枚举名可解析”替代“稳定身份可证明”：

- `cad-core/src/runtime/recompute.cpp::stableSubnameFor()` 在没有 `NamedShape` 时仍会把普通 `FaceN` / `EdgeN` 当作 `stableSubname` 返回；`responseSubshapes()` 里还有多处 Body / Tip display 修正逻辑。
- `cad-core/src/app/link.cpp::linkedSubshapeAt()` 在 `StableSubList` 缺失时会把 `SubList` 回填成 `rawStableSubname`，等于把当前 `FaceN` 伪装成稳定身份。
- `cad-core/src/part_design/profile_resolver.cpp::linkHasStrongSubshapeEvidenceAt()` 把带 owner 前缀的 `FullSubList` 当强证据；`stableSubnameForElementReference()` 也允许裸 `FaceN` 在某些条件下继续作为稳定引用。

这会导致类似 `RevolutionBody:Face11` 的引用在拓扑变化后被继续当作同一个面使用。后端确实能按当前 shape 找到 `Face11` 并生成形体，但这不是长期建模语义，只是一次请求里的当前枚举。

## 目标语义

三层字段必须分工明确：

- `subname`：当前可选中的名字，例如 `Face11` 或 `Pad.Face5`。它只说明“这次结果里可以这样选到”，不说明下次重建还是同一个几何身份。
- `fullSubname`：展示 / 路径证据，例如 `RevolutionBody.Revolution.Face11`。它帮助前端定位 UI 路径，但不能证明稳定身份。
- `stableSubname`：长期建模身份。它必须来自 FreeCAD 风格的 `NamedShape` / `ElementMap` / `mapper_history` / `getElementMappedName()` 证据，不能由裸 `FaceN`、`EdgeN` 或 `fullSubname` 推出来。

因此，裸 `FaceN` 有两种完全不同的含义：

- 作为 `subname`：可以，表示当前 shape 的第 N 个面。
- 作为 `stableSubname`：默认不可以，除非 `ElementMap` / `mapper_history` 能证明它不是普通当前枚举，而是某个长期身份的唯一映射结果。

如果 `getElementMappedName("Face11")` 或内部映射结果仍然只是裸 `Face11`，也不能直接按稳定身份发布；必须继续检查账本证据。没有账本证据时，`stableSubname` 为空，并把身份状态标成 current-only / display-only。

## 报错边界

报错边界按“是否进入长期建模引用”划分：

- 仅输出目标 shape 的面列表用于显示 / 拾取：允许返回 `subname=Face11`、`fullSubname=RevolutionBody.Revolution.Face11`、`stableSubname=""`，并标注 `identityStatus=current_only` 或 `body_display_only`。这类面只能展示，不能被前端保存成建模引用。
- `Pad`、`Pocket`、`Revolution`、`Fillet`、`Chamfer`、`Draft`、外部引用、Profile 选择等消费 subshape 作为长期输入：如果不能证明稳定身份，后端必须产生 error diagnostics，并停止按裸 `FaceN` 静默建模。
- `FullSubList` 只说明用户当时从哪个显示路径选中，不是稳定身份证明。它不能单独把 `Face11` 升级成 `stableSubname`。
- `ReferenceShadow` / `ShadowSub` 可以作为恢复证据或旧几何快照线索，但不能绕过 `NamedShape` / `ElementMap` / `mapper_history` 直接制造稳定名。

前端不需要拿到完整账本。账本属于后端单次 recompute 的内部运行态；响应只暴露派生结果：`stableSubname`、`identityStatus`、`elementReferenceUpdates`、`diagnostics`、必要的 `ReferenceShadow` 建议。

## FreeCAD 权威来源

这次重构必须以 FreeCADCmd 采集结果为权威，而不是从现有 `cad-core` 输出倒推：

- 需要确认 expected 时，调用 `cad-core/tools/collect_freecad_expected.py`，用本机 `FreeCADCmd` 采集。
- 对响应式 subshape 结果，优先采集 FreeCAD `Shape.getElementMappedName(indexed)` 的值，再结合 `NamedShape` / `ElementMap` / `mapper_history` 判定它是否真能作为稳定身份。
- 如果 FreeCAD 返回的是类似 `Pad.#f:1;FAC;:H293:4,F` 的 mapped name，可以作为稳定身份候选。
- 如果 FreeCAD / 当前 `cad-core` 只能得到裸 `FaceN`，且没有 mapper history 或 ElementMap 唯一映射证据，就按“不能证明稳定身份”处理。

## 重构落点

新增一个拓扑身份判定模块，把当前散落在 runtime、link、profile resolver 里的规则收拢：

- `cad-core/include/cad_core/topo/subshape_identity.h`
- `cad-core/src/topo/subshape_identity.cpp`
- focused tests：`cad-core/tests/test_p6_topology.py` 或新增 C++/Python identity contract tests

建议核心类型：

```cpp
enum class SubshapeReferenceUse {
    DisplayPublication,
    DurableFeatureReference,
    ReferenceRecovery,
};

enum class StableIdentityStatus {
    Stable,
    CurrentOnly,
    BodyDisplayOnly,
    MissingEvidence,
    Ambiguous,
    Split,
    Deleted,
    Unsupported,
};

struct SubshapeIdentityDecision {
    std::string subname;
    std::string fullSubname;
    std::string stableSubname;
    StableIdentityStatus status;
    std::vector<runtime::Diagnostic> diagnostics;
};
```

模块职责：

- 输入当前 `indexed`、owner / Tip 上下文、`NamedShape`、`ElementMap`、mapper history、`ReferenceShadow` / `ShadowSub` 摘要和请求用途。
- 输出统一的 `SubshapeIdentityDecision`。
- 不依赖 JSON 字段名做业务判断，runtime / adapter 只负责 JSON 投影。
- 不从 `subname` 或 `fullSubname` 合成 `stableSubname`。
- 对 `StableSubList=["Face11"]` 这类裸当前枚举给出明确状态：显示可用但建模不可用。

## 分阶段实施

### 1. 冻结身份判定测试

先补最小但能卡住语义的测试，不先改大段逻辑：

- `subname=Face11, stableSubname=Face11, no ElementMap`：作为 durable reference 必须 error。
- `subname=Face11, fullSubname=Body.Tip.Face11, stableSubname=""`：作为 display publication 可以输出，但 `identityStatus` 不能是 `stable`。
- `subname=Pad.Face5, stableSubname=Pad.#f:...`：有 mapped name 证据时可作为稳定身份。
- `mapper_history` 标记 split / ambiguous / deleted：不得选一个“看起来唯一”的当前 `FaceN` 凑 stable。

### 2. 抽出 `topo::SubshapeIdentityDecision`

把 `runtime/recompute.cpp` 中的这些逻辑搬到 `topo` 层：

- `stableSubnameFor()`
- Body / Tip local stable 判断
- display compound stable 判断
- `identityStatus` 降级规则

第一步可以保持现有输出行为不变，只做机械抽取和测试覆盖；第二步再删除旧 fallback。这样便于 review，也能看清哪些 fixture 依赖了错误语义。

### 3. 删除输出层 stable fallback

在 `responseSubshapes()` 的新调用路径里落实：

- 无 `NamedShape` / `ElementMap` / mapper history 证据时，不再返回 `stableSubname=indexed`。
- `fullSubname` 继续输出，但只作为 display path。
- Body display-only 面输出 `identityStatus=body_display_only` 或 `current_only`，`stableSubname` 为空。
- 只有判定模块返回 `Stable` 时，才允许响应里出现非空 `stableSubname`。

### 4. 收紧输入消费层

修改 `app/link.cpp::linkedSubshapeAt()`：

- `StableSubList` 缺失时保持为空，不再回填 `rawSubname`。
- `SubList` 可解析只能说明当前 shape 上有这个元素，不能说明引用稳定。
- 对 durable reference，调用 `topo::resolveDurableSubshapeReference()`；如果状态是 `MissingEvidence` / `CurrentOnly` / `Ambiguous` / `Split` / `Deleted`，写入 error diagnostics 并停止构造。

修改 `part_design/profile_resolver.cpp`：

- `FullSubList` 不再算强稳定证据，只保留 owner / display path 用途。
- `stableSubnameForElementReference()` 不再把 `stableSubname == subname == FaceN` 当有效稳定引用。
- `ReferenceShadow` / `ShadowSub` 只进入恢复流程，恢复成功后也必须产出可追踪的 mapped stable identity 或 `elementReferenceUpdates`。

### 5. 明确 diagnostics 和写回建议

新增或统一 diagnostics code：

- `missing_stable_subname`：建模引用缺少 `StableSubList` 或为空。
- `unstable_subshape_reference`：引用只有裸 `FaceN` / `EdgeN`，不能证明长期身份。
- `full_subname_not_stable_identity`：调用方试图用 `FullSubList` 证明稳定身份。
- `stable_identity_ambiguous`：mapper history 显示一对多、split 或多候选。
- `stable_identity_deleted`：旧稳定身份已删除且无法恢复。

`elementReferenceUpdates` 只能从真实账本恢复结果生成，不能因为当前 `FaceN` 可解析就建议写回。

### 6. 更新 oracle 和接口文档

`cad-core/tools/collect_freecad_expected.py` 继续输出响应式 subshape entries，但 expected 里也要区分：

- `indexed` / `subname` 可为裸 `FaceN`。
- `stableSubname` 只能填 FreeCAD mapped name 或经账本证明的稳定名。
- mapped name 仍是裸 `FaceN` 且没有额外账本证据时，expected 应为空稳定名，并标记 current-only / display-only。

同步更新 `cad-web-background/docs/接口规定/01-cad-recompute全量输入输出接口.md` 后，前端规则应是：不能把 `stableSubname=""` 或裸 `FaceN` 的 display-only 面保存成建模引用。

## 非目标

- 不把完整 `NamedShape` / `ElementMap` / `mapper_history` 账本返回给前端。
- 不引入跨请求 shape / BREP 缓存；`ReferenceShadow.brep` 仍只允许作为单个旧 subshape 快照证据。
- 不用 fixture 名称、几何排序、`FaceN` 数字大小或 `fullSubname` 字符串拼接来猜稳定身份。
- 不在 adapter / JSON 层修输出；业务判定必须在 `topo` / feature reference 层完成。

## 验收

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/重构/7-8-00-17-StableSubname身份账本语义重构方案.md
```

实现阶段 focused 验收：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests/test_p6_topology.py tests/test_p7_features.py
python3 -m py_compile tools/collect_freecad_expected.py
```

FreeCAD oracle 验收示例：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/p8/part-box.json --check
```

阶段收口时再跑更多 fixture 和前后端接口回归；普通小步重构不要求全量 FreeCAD build。
