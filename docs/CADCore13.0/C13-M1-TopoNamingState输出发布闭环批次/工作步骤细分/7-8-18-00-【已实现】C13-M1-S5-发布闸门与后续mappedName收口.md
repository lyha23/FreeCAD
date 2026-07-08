# 【已实现】C13-M1 S5 发布闸门与后续 mappedName 收口

## 目标

关闭 C13-M1 输出发布闭环，并把未完成的 FreeCAD mapped-name parity 明确拆成后续批次。

## live 基线

- `pwd`: `/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`: `43770d1d7b`
- `git log -1 --oneline`: `43770d1d7b 验证 C13-M1 S4 topoNamingState 对齐`
- `git -c core.quotepath=false status --short -uall`: 无输出，S5 开始前工作区干净。
- `step_goal_queue.py ... --format markdown`: S5 开始前唯一 pending 为 `7-8-18-00-C13-M1-S5-发布闸门与后续mappedName收口.md`。

## 发布结论

- C13-M1 输出发布闭环已关闭，`docs/CADCore13.0/README.md` 和本包 README 均标记为 completed / 已完成。
- 正式 response 已发布顶层 `topoNamingState`，schema 为 `cad-core.topo-state.v1`。
- response state 可注入下一次请求，`c4m6/topo-state-body-tip-stable-recovery` 的 Body/Tip stable reference 不回退。
- CLI / C API / worker / wasm 的 `topoNamingState` channel 一致。
- 5 个 focused `cad-core-res` 输出已保存到 `cad-core/fixtures/<phase>/cad-core-res/<case>.cad-core.json`，未写入 `expected/`。
- full `CadCoreAdapterTest` 仍保留既有 `6 failures, 8 errors` baseline；C13-M1 focused 验证通过，不作为本批次 blocker。
- `C13M1-BLOCKER-601` 已关闭，其它 C13-M1 blocker 已在 S0-S4 关闭。

## 后续 gap

- `freecad_mapped_name_encoding_gap`：FreeCAD raw mappedName（如 `#...:H...,F`）字节级编码需要后续批次实现 encoder / canonicalizer；C13-M1 不从 expected 字符串反推 runtime 输出。
- `child_element_map_key_gap`：FreeCAD `childElementMapKey` 精确 parity 需要补 ElementMap child map key 账本；C13-M1 仅保留当前 schema 分类。
- `mapper_history_id_gap`：FreeCAD `mapperHistoryIds` 精确 identity parity 需要后续 mapper history id 映射；C13-M1 保留 request-local `mapperHistory` / `mapperHistoryIndexes` 投影。
- 上述 gap 是后续路线，不在本包内创建 C13-M2，也不把 non-goal 标成 supported。

## 最终验证

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/矩阵/*.tsv
git diff --check
```

最终结果：

- 队列命令通过；S5 重命名后队列只剩表头，无 pending 步骤。
- TSV field count 校验通过。
- `git diff --check` 通过。
