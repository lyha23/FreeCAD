# CAD Core 建模可用性与 Producer Trace 门禁边界

## 结论

判断“CAD Core 能不能拿来构建 CAD”，看的是当前 binary 的公开建模结果、引用恢复语义和 release gate，不是 producer trace 是否已与 FreeCAD 内部过程逐字段对齐。

> 只要当前 CAD Core 对所用 feature/case 的 live public semantic parity 为 green，必要的 geometry、subshape、`stableSubname`、引用更新和 diagnostics 合同成立，就可以用于该范围的实际建模。producer trace 的缺口不应单独阻塞这个使用结论。

这个结论只对已经过 live 公共语义门禁的 feature/case 成立，不能从一个 phase 的 green 外推为“CAD Core 所有建模能力都已完成”。

## 两类问题必须分开

| 问题 | 裁决依据 | 结论含义 |
| --- | --- | --- |
| “用户能否用 CAD Core 建模？” | 当前 binary 的 public semantic parity、focused feature tests、reference recovery 与 release gate | 回答几何、拓扑、引用和诊断的对外行为是否可用 |
| “CAD Core 内部是否与 FreeCAD 同过程？” | producer trace 的 transaction/scope/StringHasher/ElementMap/mapper/child closure 和 first divergence | 回答内部生产顺序是否可证明、差异出现在哪一步 |

public result 门禁是产品可用性门禁。producer trace 是按需使用的实现定位、回归取证和调试工具，不是第二道产品门禁。后者不能替代前者，也不应在前者已通过时反向宣称“CAD Core 不可用”。

默认流程只看 public/ledger。只有 public/ledger 行为无法对齐、差异原因无法从公开输出判断，或者任务明确要求内部过程审计时，才查 producer trace。trace 是解释“为什么没对齐”的参考证据，不是证明“已经对齐”的必要条件。

## 固定双 verdict 与报告路由

需要启用 trace 诊断时，同一次检查保留两个互不覆盖的结论；未触发 trace 诊断时，只给出 public/ledger consistency，并把 producer trace 标为 `not_evaluated`：

| verdict | 输入 | 可以否决什么 |
| --- | --- | --- |
| public/ledger consistency | public expected、当前 CAD Core public projection、strict ledger、reference/release evidence | 可以否决公开行为一致性和建模可用性 |
| producer trace diagnostic | transaction、scope、event、SID、mapper、before/after snapshot、first divergence | 只能否决“内部过程已严格对齐/可完整解释” |

因此，只要 public/ledger consistency 为 green，就不需要为了放行一致性继续比较 trace。若 trace 因专项审计已经产生 `semantic different`，它必须进入独立的 trace diagnostics 或 variation 清单，不能进入 public/ledger `differences`，不能成为 public/ledger `firstFailure`，也不能把 consistency/release verdict 改成 red。trace `missing/invalid` 可以令 producer diagnostic lane 失败，但同样不能伪造 public 行为差异。

反过来也成立：producer trace equal 并不能放行 public red。产品一致性始终由 public/ledger 行为证据裁决。

## 什么才能证明“可以用于建模”

对用户实际依赖的 phase/case，至少确认：

1. 使用当前 `build/cad-core` 重新执行 fixture，不只读 checked-in `.cad-core.json`。
2. `semanticStatus = green`，或者只存在已批准且 consumer contract 成立的 `protocol_divergence`。
3. 几何构造成功，`diagnostics` 没有未批准的失败。
4. 用户会依赖的 subshape 拾取、`stableSubname`、`elementReferenceUpdates` 和 `topoNamingState` 语义通过对应 focused test。
5. 若用于发布或正式基线，`releaseStatus` 必须是当前、可复现的通过状态，不能只看旧 snapshot。

`exactStatus = red` 不一定表示不可用。例如 CAD Core 比 FreeCAD public expected 多返回 mesh，或 raw `:H<hex>` 因运行期 Tag 不同而字节不同，可能导致 exact red，但在 canonical identity、引用恢复和共有建模语义一致时，semantic 仍可为 green。

## 哪些 Producer Trace 缺口不阻塞正常建模

以下缺口影响的是内部取证和实现差异定位，不会直接作为 shape、recompute 或 public response 的输入：

- raw mapper snapshot 尚缺 `sourceTag`、`rawCanonicalSha256`、target `relationStatus` 或 `outputMembers`；
- child range 尚缺完整 `elementIdRefs` 取证；
- snapshot identity 尚未覆盖 SID refs 或 nested refs；
- validator 对 mapper/child 字段还不够严格；
- fixture tree 里的 CAD Core actual trace 还没有与最新 native trace 成对物化；
- first-divergence 因缺完整证据而暂时无法精确报告第一处内部分叉。

只要 producer trace 仍保持只读旁路，不参与几何构造、StringHasher 分配、ElementMap 决策、`topoNamingState` 输入或 public response，上述缺口就属于“对齐/调试能力尚未完整”，而不是“建模功能不可用”。

## 哪些问题会真正阻塞用户建模

以下任一情况才应该把对应 feature/case 标为不可用或有限可用：

- 当前 binary 的 `semanticStatus = red`；
- OCCT/feature 构造失败，返回 null/empty/wrong shape 或未批准 diagnostics；
- 公开 subshape inventory、几何、拓扑、placement 或 feature 参数语义错误；
- `stableSubname` / `StableSubList` 无法在参数修改和 recompute 后恢复，或 split/deleted/ambiguous 被误判；
- `elementReferenceUpdates` 丢失、指向错误 owner/target，或需要用户重选时未发布诊断；
- 实际依赖的 feature 尚未实现，或只有 protocol-only/internal probe 证据，没有 public native/live 门禁；
- release gate 因 binary 陈旧、fixture/ledger 无效、环境不可复现或 current freshness 失败而无法给出可信结论。

这些都是用户可见的建模语义或发布证据问题，与“内部 trace 字段还不够详细”不是同一类风险。

## C4M6 的具体裁决

`cad-core/tests/test_topo_naming_state_response.py` 对 C4M6 的当前合同是：

- Body/Tip stable recovery、first recompute、Compound child maps 和 ReferenceShadow 的 public `semanticStatus = green`；
- schema/producer/document hash/object hash/foreign owner 等无效状态输入必须精确返回 diagnostics-only rejection；
- `exactStatus = red` 可以由已分类的表现差异引起，不自动等于 public 语义失败。

因此，就 C4M6 已覆盖的范围而言，CAD Core 可以用于实际建模和状态回传。当前 mapper/child producer trace 尚有取证缺口，但它们只阻塞“与 FreeCAD 内部过程完全对齐”和“可靠报告 first divergence”，不阻塞 C4M6 已通过的 public 使用语义。

但 C4M6 不包含所有 CAD Core feature family。不得仅因 C4M6 green 就外推 Chamfer、Fillet、Pattern、Loft、Sweep、split/open-wire 或其他未经对应 live gate 的能力也已可用。

## 固定判断口径

遇到“能不能用”的问题时，按以下顺序回答：

1. 先定位用户真正依赖的 feature/case，不问整个 CAD Core 的笼统状态。
2. 查当前 binary 的 live public semantic/release gate。
3. public 语义 green 则回答“该范围可用”，并单独列出 trace/debug 缺口。
4. public 语义 red 或 invalid 则回答“不可宣称可用”，并按 geometry/topology/reference/diagnostic/freshness 分类。
5. trace 缺口只在 producer 对齐、first-divergence 或调试任务中 hard fail，不改写 public/release verdict。
6. 报告 producer semantic differences 时同时给出 trace verdict，但 public/ledger consistency 和它的 `firstFailure` 保持独立。

一句话归纳：

> public/release gate 决定“产品能不能用”；只有 public/ledger 无法对齐时，producer trace 才帮助解释“内部从哪里开始不同”。
