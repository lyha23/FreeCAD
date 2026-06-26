# C8-M2-S5 capability 协议与前端接入边界发布

## 目标

同步 FreeCAD `cad-core` capability、diagnostics、known_gap 和前端接入边界。S5 必须把 supported、sync-required、known_gap、oracle-blocked 和 non-goal 明确分开。

## 必须发布的能力口径

- `part_design.shape_binder.status`
- `part_design.sub_shape_binder.status`
- `part_design.sub_shape_binder.remaining_gaps`
- `part_design.sub_shape_binder.known_gaps.copy_on_change_full_temporary_document_cache`
- 下游同步合同中的 type ids / fixtures / diagnostics
- 前端不得持久保存 full BREP / NamedShape / ElementMap cache 的边界

## 发布规则

- C8-M1 已 expected-backed pass：保持 supported。
- 下游同步：发布 `sync_required` 或合同任务，不写成 FreeCAD runtime support 缺口。
- Full CopyOnChange temporary-document cache：保持 `oracle_blocked` / known_gap，写 delete/reopen condition。
- GUI / session / Rust implementation：发布 `diagnostic_non_goal`。

## 必须回写的矩阵行

- `C8M2-SCOPE-101..103`
- `C8M2-SCOPE-201..203`
- `C8M2-BLOCKER-501`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
./cad-core capabilities > /tmp/c8m2-capabilities.json
python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics
```

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'shape_binder|sub_shape_binder|copy_on_change_full_temporary_document_cache|known_gap|oracle_blocked|sync_required|diagnostic_non_goal' cad-core/src/runtime/capability_contract.cpp cad-core/tests docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/矩阵/*.tsv
git diff --check
```

验收通过后，将本文件重命名为 `6-26-22-26-【已实现】C8-M2-S5-capability协议与前端接入边界发布.md`。

## 非目标

- 不把 oracle-blocked lifecycle 写成 supported。
- 不用 docs-only 发布替代 focused tests。
- 不修改下游 Rust adapter。
