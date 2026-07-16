# FreeCAD 原生 fixture authority 报告

本目录保存 `collect_freecad_expected.py`、`validate_freecad_expected_ledger.py`
和 `audit_freecad_fixture_authority.py` 生成的可复查报告。权威 producer 固定为：

```text
/Users/li/Chili3DProject/FreeCAD2/build/relwithdebinfo/bin/FreeCADCmd
```

当前 producer 报告身份为 FreeCAD `1.2.0`、revision `47063`、OCCT `7.8.1`，
二进制 SHA-256 为
`51a6fb775b1f2a4edff5cee1b90a4395674f988748f75d01deb0ce5add0ca674`。
producer 源工作区的 build directory、CMake 信息和 dirty 状态属于可选 provenance：
缺失时只产生 warning，不改变 public expected 或 ledger 门禁结论。

## 当前 inventory

`fixture_authority_inventory.v1.json` 是由 fixture、role manifest、已有 authority
产物和探针报告重建的全量清单，不是手工统计。

| 项目 | 数量 |
| --- | ---: |
| input | 775 |
| native | 480 |
| protocol_only | 14 |
| unsupported | 281 |
| public expected | 480 |
| ledger | 480 |
| producer trace | 480 |

281 个 unsupported case 全部逐项记录了 collector/oracle 分类、证据、下一步和
是否可作为下一批 native 候选：

| 分类 | 数量 |
| --- | ---: |
| `collector_general_gap` | 120 |
| `freecad_native_not_expressible` | 94 |
| `non_native_fixture` | 67 |
| `not_investigated` | 0 |

`probes/` 保存 109 个跨 Part、Sketch、Body/Pad、Link、topology-state、Boolean、
Revolution、Dressup、Loft/Pipe 家族的实际探针回执。106 个在 collection 阶段
fail-closed；另外 3 个虽然执行完成，但只生成 rejected request ledger。没有 case
同时满足 intended public-root 语义、strict-valid accepted ledger、独立 repeat 2
和原子 authority 产物要求，因此本轮没有通过弱化语义或接受 rejected ledger
来 promotion。当前 `nextNativeCandidates` 为空；后续先按分类修复通用
collector/oracle 语义批次，清单才会重新产生候选。

## 已签入 Gate 报告

| 报告 | 结论 |
| --- | --- |
| `all-native-check.v1.json` | 480/480 执行，0 skip，0 failure；public expected 与严格 ledger validation 均通过 |
| `all-native-repeat2.v1.json` | 两个独立进程、两个独立输出根目录；每轮 480/480，跨轮 public/ledger 差异为 0 |
| `all-native-ledger-validation.v1.json` | strict 模式 480/480 合法，0 error |
| `representative/*.json` | Box、Sketch、Body/Pad、Link、topology-state 单 case 以及 topology-state phase 的独立 repeat 2 |
| `probes/*.json` | 109 个 unsupported authority 隔离探针及可重复性证据 |

all-native 报告的四个 Gate 字段必须分别读取：

- `publicExpectedStatus = passed`
- `ledgerValidationStatus = passed`
- `ledgerDriftStatus = drifted`
- `producerTraceStatus = not_evaluated`

这里的 `drifted` 表示本次候选 ledger 与签入 ledger 的内部版本、哈希或命名细节
不同；480 个 public expected 都语义相等，且候选 ledger 全部结构合法、自洽，
所以 CAD 最终结果门禁通过。producer trace 默认不生成、不比较；只有
expected/ledger 分叉时才进入诊断。

## 重建与使用

以下命令均从 `cad-core/` 执行。清单可以纯本地、确定性重建：

```bash
python3 tools/audit_freecad_fixture_authority.py \
  --roles tools/freecad_expected_parity/fixture_roles.v1.json \
  --report tools/freecad_expected_parity/reports/fixture_authority_inventory.v1.json
```

single-case 和 phase 先做 check-only，再用两个独立子进程和目录做 repeat：

```bash
python3 tools/collect_freecad_expected.py \
  --freecadcmd /Users/li/Chili3DProject/FreeCAD2/build/relwithdebinfo/bin/FreeCADCmd \
  fixtures/p8/part-box.json --check --validate-ledger

python3 tools/collect_freecad_expected.py \
  --freecadcmd /Users/li/Chili3DProject/FreeCAD2/build/relwithdebinfo/bin/FreeCADCmd \
  fixtures/p8/part-box.json --check --repeat 2 \
  --candidate-root /tmp/part-box-repeat2 \
  --report /tmp/part-box-repeat2.json

python3 tools/collect_freecad_expected.py \
  --freecadcmd /Users/li/Chili3DProject/FreeCAD2/build/relwithdebinfo/bin/FreeCADCmd \
  --phase c4m6 --check --repeat 2 \
  --candidate-root /tmp/c4m6-repeat2 --report /tmp/c4m6-repeat2.json
```

all-native check、独立 repeat campaign 和严格 ledger validation：

```bash
python3 tools/collect_freecad_expected.py \
  --freecadcmd /Users/li/Chili3DProject/FreeCAD2/build/relwithdebinfo/bin/FreeCADCmd \
  --all-native --check --validate-ledger --report /tmp/all-native-check.json

python3 tools/collect_freecad_expected.py \
  --freecadcmd /Users/li/Chili3DProject/FreeCAD2/build/relwithdebinfo/bin/FreeCADCmd \
  --all-native --check --repeat 2 \
  --candidate-root /tmp/all-native-repeat2 \
  --report /tmp/all-native-repeat2.json

python3 tools/validate_freecad_expected_ledger.py \
  --all --strict --report /tmp/all-native-ledger-validation.json
```

每次都要读取报告中的 `selectedCaseCount`/`executedCaseCount` 或
`discovered`/`processed`，确认非零且相等，不能只看进程退出码。explicit zero-case、
缺 expected、缺 ledger、collection failure，以及 repeat 子报告缺失或陈旧都会
fail closed，并尽可能留下机器可读报告。

promotion 必须一次性闭合 input、public expected、ledger、role manifest 和 producer
report；禁止手改 `*.freecad.json` 或 `*.freecad.ledger.json`，也禁止按 phase/case
名称向 collector 添加特判。

## 方案闭合状态

- E0：775 条全量 inventory、authority 产物和异常检查已闭合。
- E1：single/phase/all-native check、独立 repeat 2、四状态字段和 fail-closed 已闭合。
- E2：281 条 unsupported 全分类；109 条实探针已记录，当前候选队列为 0。
- E3：promotion 原子门槛已执行；当前 producer 可探候选已跑空，本轮没有可安全提升的 case。
- E4：all-native 重采、严格 ledger、provenance、unsupported 和使用说明均已签入。

方案的 10 条完成标准均由上述 inventory、测试及报告覆盖：全量可重建、角色和
authority 对齐、显式选择 fail-closed、独立重复采集、public/ledger/trace 分离、
最低 producer 身份、代表家族重采、unsupported 全分类、原子 promotion 纪律，
以及 all-native 最终 Gate 报告。
