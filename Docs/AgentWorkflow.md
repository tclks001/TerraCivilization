# AI Agent 工作流报告（TerraCivilization R1~R7 阶段总结）

> 本文档基于 TerraCivilization 项目 R1（纯白材质）→ R7（Triplanar 真实地表）阶段中累积的协作经验，沉淀适合后续 R8+ 及自研球面网格（T 阶段+ TessellatedMesh）生产路线的 AI Agent 工作流规范。
>
> 阅读对象：本项目后续阶段的 AI 助手 / 维护者；也作为「AI 与人类配对编程」一般性参考。

---

## 0. 全景：四段式工作 Loop

```
┌──────────────────────────────────────────────────────────────────────────┐
│                         单期阶段（R<N>）的工作 Loop                          │
└──────────────────────────────────────────────────────────────────────────┘

       ┌────────────────────────┐
       │ ① 需求澄清               │       ← 用户提出方向性需求时
       │   ? 列 2~3 条候选方案   │       ← 不立刻动手；列权衡
       │   ? 等用户拍板           │
       └─────────┬──────────────┘
                 ▼
       ┌────────────────────────┐
       │ ② 主稿简述               │       ← 在 SphericalSDFTerrainDesign.md
       │   ? §6.x.1 / §11        │           只写公式 + 跳转
       │   ? 不超过 30 行         │
       └─────────┬──────────────┘
                 ▼
       ┌────────────────────────┐
       │ ③ 当期独立详稿            │       ← R<N>_<Topic>.md
       │   ? 8 主章节 + 附录       │           几何/HLSL/cpp/材质/验收/排错
       │   ? 含完整可粘贴 HLSL    │
       └─────────┬──────────────┘
                 ▼
       ┌────────────────────────┐
       │ ④ cpp 落地 + 反射诊断    │       ← 编辑器内手工材质验收
       │   ? 编译验证 0 警 0 错   │       ← Output Log 反射诊断
       │   ? Roadmap 状态联动    │
       └─────────┬──────────────┘
                 ▼
            视觉验收 + 踩坑沉淀
                 │
                 └─→ 补回 §6 排错表（每次踩坑后立刻写入）
```

每一段都不可省略；任何省略都会在后续阶段以"返工"或"踩同样的坑"的形式偿还。

---

## 1. 单期阶段的 4 个段落（细则）

### 1.1 段 ① — 需求澄清

#### 1.1.1 必须做
- **先列候选方案**：用户给出方向性需求（如"实现软边"）时，提供 2~3 条可行路径并标明权衡。
  - R5 软边：路径 A（λ smoothstep）/ 路径 B（acos δ smoothstep）/ 路径 C（归一化 dot）
  - R6 噪声：方案 A（hash sin）/ 方案 B（dir 切向偏移）/ 方案 C（per-cell value noise）/ 方案 D（fbm）
  - R7 纹理格式：纹理集 vs GLTF 双方案
- **要求用户拍板**：在用户明确选择后再推进；*绝不擅自决定*。
- **明确"为什么、做什么、怎么验收"**：把视觉效果、量化指标（弧度数、ms 增量、显存）一次说清。

#### 1.1.2 禁止做
- ? 用户没明确选择就直接编码或写文档。
- ? 用户已选 A 路径，私下用 B 路径"顺便"实现。

### 1.2 段 ② — 主稿简述

主稿是 [SphericalSDFTerrainDesign.md](C:\workspace\TerraCivilization\Docs\SphericalSDFTerrainDesign.md)。

#### 1.2.1 必须做
- 当期 §6.x.1（落地小节）写：核心公式、跨期约束、跳转链接，**不超过 12~30 行**。
- §11 Roadmap 表当期行写：动作摘要、保留链路、关键约束、可量化验收期望（一行）。
- §12 风险表新增对应风险（如"NoiseAmplitude > TriRadius 互锁伪影"）。
- §14 GPU 资源表新增对应资源（如新 LUT、新 Texture2DArray）。

#### 1.2.2 禁止做
- ? 把当期完整 HLSL / cpp 改动塞进主稿。
- ? 主稿章节超过 30 行还不拆出独立详稿（R4 早期把详细方案塞进 §11.3 144 行，后被压缩至 12 行并移出独立文档；这是教训）。

### 1.3 段 ③ — 当期独立详稿

命名：`R<N>_<Topic>.md`，放在 [Docs/](C:\workspace\TerraCivilization\Docs)。

#### 1.3.1 固定 8 章 + 附录结构（来自 R5/R6/R7 的实操沉淀）

| 章 | 内容 | 是否必须 |
| --- | --- | --- |
| §0 | 一句话目标 + 视觉对比矩阵 | ? |
| §1 | 几何与数学（含与上一期的差分） | ? |
| §2 | HLSL 实现（UE Custom 节点限制 + 完整可粘贴代码） | ? |
| §3 | cpp 端改动（UPROPERTY、MID 注入、反射诊断升级） | ? |
| §4 | 材质资产搭建（节点级 + Sampler Type + Inputs 表） | ? |
| §5 | 验收清单（A~J 项可勾选） | ? |
| §6 | 排错表（症状 → 根因 → 修复） | ? |
| §7 | 与上下游关系（与 R<N-1> / R<N+1> / T 阶段自研网格同构性） | ? |
| 附录 | 数值上限分析、参数调参参考、跨阶段同构性论证 | 推荐 |

#### 1.3.2 必须做
- §2 给出**直接可复制粘贴**的完整 HLSL Code，不要让用户"自己拼"。
- §6 排错表必须列出每个已知踩坑的完整三段式（症状 / 根因 / 修复）。
- §7 必须明确与上下游阶段的关系（关键约束：本项目 PMC 与 T 阶段自研网格共用同一套 HLSL。原 R11 PTG 同构性要求已作废）。

#### 1.3.3 禁止做
- ? 在详稿里写「请参考 §X.Y」却不在 §6 排错表给出操作步骤。
- ? 详稿落地后忘记反向更新主稿 §11 Roadmap 状态字段。

### 1.4 段 ④ — cpp 落地 + 编辑器验收

#### 1.4.0 cpp 编码前置：字段名以源码为准（强制）

> **核心原则**：在写跨模块调用代码时（例如 `Topology->Cells[i].XXX`），**绝不能凭直觉/记忆/文档摘要写出字段名**——必须先打开真实源码（`.h` 头文件）确认字段名拼写。
>
> **典型踩坑**（W2 落地，2026-06）：在 [WorldGenerator.cpp](../Source/WorldGen/Private/WorldGenerator.cpp) 中按"邻接列表"语义直觉写出 `Topology->Cells[i].Neighbors`，编译失败——真实字段名是 `NeighborCellIds`（[FCell.h:18](../Source/Grid/Public/FCell.h)：`TStaticArray<int32, 6> NeighborCellIds{...}`）。设计稿 [SphereTopologyReference.md §3.2](SphereTopologyReference.md) 与 [WorldGenDesign.md §10.1](WorldGenDesign.md) 早已锁定真实字段名，但凭语义直觉写代码时仍然漏掉了核对环节。

##### 必做工作流（写每一行 `obj->field` / `obj.field` 时）

```
┌──────────────────────────────────────────────────────┐
│ 1. 写代码前先 grep_search / view_code_item 拉真实字段定义 │
│    例：要用 FCell 的邻接 → 先 grep 'struct FCell' / 读 FCell.h │
│ 2. 把光标对准字段名，逐字符核对（Neighbors ≠ NeighborCellIds）│
│ 3. 注意类型差异：TStaticArray<int32,6> vs TArray<int32> │
│    （TStaticArray 用满 N 项；空位是 INDEX_NONE，迭代时必判）│
│ 4. 编辑后立即 grep 验证字段名在新代码中拼写一致 │
└──────────────────────────────────────────────────────┘
```

##### 反模式（必须避免）

| ? 反模式 | 为什么错 | 正确做法 |
| --- | --- | --- |
| 凭语义记忆写字段名（"应该叫 Neighbors 吧"） | 工程实际命名通常更精确（`NeighborCellIds` / `NeighborCornerIds`） | 写之前 grep / 读 .h |
| 只看设计稿摘要不看源码 | 设计稿可能省略前缀/后缀；摘要可能截断 | 设计稿确定语义后**仍要核对源码** |
| 拷贝其它语言/项目的相似命名 | UE 项目命名规范常含 `Ids`/`Indices` 后缀 | 总是核对当前项目的字段定义 |
| 编辑后只编译一次就提交 | 字段名错误编译期能抓到，但相邻字段（同类型不同语义）写错时编译能通过 → 运行时错误 | grep 验证 + 编译 + 运行验收三件套 |

##### 字段名核对清单（高频踩坑字段）

| 场景 | 错误猜测 | 真实字段（源码） |
| --- | --- | --- |
| FCell 邻接 cell 列表 | `Cells[i].Neighbors` | `Cells[i].NeighborCellIds` |
| FCorner 邻接 corner 列表 | `Corners[i].Neighbors` | `Corners[i].NeighborCornerIds` |
| FCell 边 ID | `Cells[i].Edges` | `Cells[i].EdgeIds` |
| FCell 角 ID | `Cells[i].Corners` | `Cells[i].CornerIds` |
| FCellEdge 两侧 cell | `Edges[i].Cells` | `Edges[i].CellIds` |
| FCorner 关联三个 cell | `Corners[i].Cells` | `Corners[i].CellIds` |

> 任何新跨模块调用都先到 [SphereTopologyReference.md §3](SphereTopologyReference.md#3-数据结构详解) 表格 + 真实头文件双检；本表持续追加。

#### 1.4.1 cpp 改动 — 必须做

1. **加 UPROPERTY**：分类命名 `PlanetTopology|R<N>`；含 `meta=(ClampMin=, ClampMax=)`；含完整 doxygen 注释（典型值表 + 上限说明）。
2. **MID 注入**：在 `Rebuild()` 中按当期参数顺序追加 `SetScalarParameterValue` / `SetVectorParameterValue` / `SetTextureParameterValue`。
3. **反射诊断升级**（核心）：把 `ExpectedR<N-1>Inputs` 升级为 `ExpectedR<N>Inputs`；新增的 Input 名要小写。
4. **Output Log 升级**：阶段标题字符串 + 新增字段（如 `EdgeWidth=%.4f rad (%.2f°)`）。
5. **立即编译**：`Result: Succeeded` + 0 warnings + 0 errors。

   - 本项目当前验证通过、可直接复用的 Unreal Editor 目标编译命令如下：

   ```powershell
   & 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' TerraCivilizationEditor Win64 Development -Project='C:\workspace\TerraCivilization\TerraCivilization.uproject' -WaitMutex -NoHotReloadFromIDE
   ```

   - 注意：
     - `Build.bat` 的完整路径必须包在单引号里，因为 `Program Files` 路径中含空格。
     - `-Project=` 后面的 `.uproject` 绝对路径也必须包在单引号里，避免路径解析失败。
     - 后续若 AI Agent 在本项目内执行“改完代码立即编译”，默认优先复用上面这条命令，不要手写省略引号的变体。

##### 无资产编辑任务：编译、独立运行与日志/命令验收（强制）

适用范围：本次任务**不需要在 UE Editor 中手工编辑或验收 `.uasset` / 材质 / 蓝图等资产**，只需验证 C++ 运行时、命令行或外部联调功能。例如当前 LLM Agent 接入 UE MCP 阶段，可直接启动游戏进程，或启动 Agent 脚本连接 UE MCP 完成验证。

1. 先执行本节的 `TerraCivilizationEditor` 编译命令，并以 `Result: Succeeded` 为成功标准。
2. 若链接阶段因 DLL 被占用失败（典型为 `LNK1104`，或构建日志明确指出 `UnrealEditor-TerraCivilization.dll` 被 `UnrealEditor.exe` / 本项目独立游戏进程占用）：
   - 只结束**本次任务启动、且已确认属于本项目**的 Editor / 独立游戏进程；不得按进程名批量结束未知用户会话。
   - 等待进程退出后，使用**同一条**编译命令自动重试；重试仍失败时，保留完整链接错误并报告，不可猜测成功。
3. 编译成功后，按任务类型选择验收路径：
   - **游戏运行时功能**：启动独立游戏进程（`UnrealEditor.exe <项目>.uproject -game -log`），等待目标地图完成加载，再从本次运行对应的日志中检索新增功能的唯一成功标识、关键状态和 Error/Warning。
   - **LLM Agent / UE MCP 等外部联调功能**：按当期设计稿启动 Agent 脚本或命令，使其连接运行中的 UE MCP；读取该命令的结构化输出和 UE 日志，核对连接、调用、返回值及游戏侧实际状态。
4. 日志验收必须使用本次运行的时间窗口或专用日志文件（可传 `-AbsLog=<绝对路径>`），避免把前一次运行遗留的成功行误判为本次通过；需要验证“日志已清理”时，也只在第二次运行的新日志中搜索该唯一标识。
5. 最终报告至少给出：构建结果、是否发生 DLL 占用及处理的 PID、实际启动/调用的命令、日志或命令输出中的验收证据，以及仍在运行的项目进程。若为用户后续操作保留进程，必须明确说明；否则只关闭本次 Agent 启动的进程。

#### 1.4.2 反射诊断（贯穿 R3~R7 的核心机制）

> **核心原则**：UE 材质资产是用户在编辑器手工搭的，C++ 不能直接读 `.uasset` 验证连线 → 必须在 cpp `Rebuild()` 中遍历 Custom 节点的 Inputs 反射数据做合规性检查。

固化流程：

```cpp
#if WITH_EDITORONLY_DATA
if (UMaterial* BaseMat = Material->GetMaterial())
{
    const TConstArrayView<TObjectPtr<UMaterialExpression>> Exprs = BaseMat->GetExpressions();
    int32 CustomFound = 0;
    const TArray<FString> ExpectedR7Inputs = { /* 14 项小写 */ };
    bool bAnyR7Compliant = false;

    for (UMaterialExpression* Expr : Exprs)
    {
        if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr))
        {
            // 1. 打印 Description / OutputType / Code.Length / Inputs.Num
            // 2. 逐个 Input 输出 Name=...  Connected=YES/NO
            // 3. 按 8 KB 分段打印 Code 字段（用于排查 HLSL 语法错）
            // 4. 检查与 ExpectedR<N>Inputs 交集 → ? R<N> compliance / ? missing / ? disconnected
        }
    }
    if (CustomFound == 0)        UE_LOG(..., Error, TEXT("NO Custom nodes!"));
    else if (!bAnyR7Compliant)   UE_LOG(..., Error, TEXT("NONE is R7-compliant"));
    else                         UE_LOG(..., Log,   TEXT("is R7-compliant ?"));
}
#endif
```

各期 Input 数量必须严格递增、不得遗漏：

| 阶段 | Input 数 | 新增项 |
| --- | --- | --- |
| R3 | 4 | UV0~UV3 |
| R4 | 8 | + WorldPos / PlanetCenter / CellAttrLUT / CellDirLUT |
| R5 | 9 | + EdgeWidth |
| R6 | 11 | + NoiseAmplitude / NoiseScale |
| R7 | 14 | + TerrainAlbedoArray / TileScale / TriplanarSharpness |

#### 1.4.3 LUT 自检日志

每次 `Rebuild()` 末尾打印前 16 个 cell 的 LUT 内容；格式必须含可肉眼校验的字段：

```
Cell0 : R=-0.5257 G=+0.8507 B=+0.0000 A=1.0  |V|=1.0000  isPent=YES
```

包括 `expected layer`、`isPent`、`|V|=1.0000` 等。

#### 1.4.4 Roadmap 状态联动（强制）

cpp 编译通过后必须立即把 §11 Roadmap 当期行状态从 `? 待开始` 升级为：
- `?? cpp 完成（待材质验收）`：cpp 已落地、等用户在 UE Editor 内手工创建材质资产
- `? 已完成`：用户视觉验收通过

漏改这一步是该工作流最常见的疏忽。

#### 1.4.5 用户操作清单

cpp 落地后给用户一份"下一步该做什么"清单，包含：
1. 复制 `M_TopologyDebug_R<N-1>.uasset` 重命名为 R<N>
2. 新增 Material 参数（标量 / 向量 / Texture Object Parameter）
3. Custom 节点 Inputs 顺序（必须与 HLSL 形参一一对应）
4. Code 字段直接复制详稿 §4.3.2 完整 HLSL
5. 输出连 BaseColor（不是 Emissive，除非验证期偷懒）
6. 挂到 Actor 的 `PlanetTopology > Material` 槽位（**不是** PMC 通用槽 Element 0）

---

## 2. 工具使用纪律

### 2.1 并行调用（核心效率原则）

> 工具调用是顺序的代价比并行高几倍。

#### 2.1.1 必须并行的场景

- 多文件读取：一次性发起 N 个 `read_file`
- 多关键词搜索：N 个 `grep_search` 并行（使用不同正则）
- `codebase_search` + `grep_search` 互补：同一查询的语义检索 + 文本检索同时发起
- 多文件编辑：一次发起 N 个 `replace_in_file`（前提：互不依赖）

#### 2.1.2 顺序的唯一合法场景

- B 工具的入参必须依赖 A 工具的输出（如先 `grep_search` 找位置，再 `read_file` 读上下文）

### 2.2 工具选型矩阵

| 任务 | 首选工具 | 替代工具 | 禁用工具 |
| --- | --- | --- | --- |
| 读已知函数/类 | `view_code_item` | `read_file`（部分行） | — |
| 探索语义概念 | `codebase_search` | — | `grep_search`（不擅长语义） |
| 已知字符串精确匹配 | `grep_search` | — | `codebase_search`（语义模糊） |
| 改单个文件多处 | `multi_replace` | `replace_in_file` × N | `edit_file`（大文件） |
| 改大文件（isBigFile=true） | `replace_in_file` / `multi_replace` | — | ? `edit_file` |
| 创建新文件 | `edit_file`（write 模式） | — | — |
| 读 `.uasset` 二进制资产 | ? 都不行 | cpp 反射诊断 | `read_file`（返回乱码） |
| 验证刚编辑过的文件 | `read_file` / `grep_search` | — | ? `codebase_search` / `view_code_item`（有索引延迟） |
| 用户截图诊断 | `read_image` | — | 仅读文字日志（会臆断） |
| 探索历史踩坑 | `grep_history_context` / `read_history_context` | — | — |

### 2.3 `replace_in_file` / `multi_replace` 规范

> 见 `<search_replace_spec_rule>` 顶部规范。复述要点：

- `old_string` 必须是文件中**真实字符**（含真实 `\t` 制表符、空格、换行）
- ? 绝不要把 `\t` 错误转义成 `\\t`
- `old_string` ≠ `new_string`（否则替换无效）
- 编辑大文件（`isBigFile=true` 或 > 2000 行）必须用 `replace_in_file` / `multi_replace`，不用 `edit_file`
- 一个文件多处改动 → 用 `multi_replace`，不要发 N 次 `replace_in_file`
- 改完后必须用 `grep_search` 验证（不用 `codebase_search` 或 `view_code_item`，它们有索引延迟）

### 2.4 历史上下文工具

#### 2.4.1 `grep_history_context`
- 关键词：函数名 / 错误关键字 / 阶段名（"R6 BoundaryNoise"）
- 用于快速定位历史踩坑

#### 2.4.2 `read_history_context`
- 输入 contextID（在 `<system_reminder>` 中给出）
- 用于读完整历史对话细节（参数、文件路径、返回值等）

### 2.5 ? 文件 / 资产删除策略：禁止主动调用删除命令

> **用户偏好（2026-06-30 锁定）**：用户可能在不值守状态下让 Agent 长时间自跑，对话中弹出的删除型 terminal 命令（`Remove-Item` / `rm` / `del` 等）很难被及时同意，会阻塞工作流；同时"残留过时文件 / 临时脚本"在最终对话末尾集中列出由用户手动清理是更安全的做法。

#### 2.5.1 硬规则
- **不得**在工具调用中执行任何删除命令（`Remove-Item` / `rm -rf` / `del` / `git rm` / `unlink` / 任何破坏性 `mv -f` 覆盖）。
- **不得**用 `edit_file` 把别的文件改成空内容来"伪删除"。
- **可以**用 `edit_file` 将旧文件重写为短小的"已废弃 / 跳转 stub"占位（保留版本说明 + 跳转链接，约 5~10 行），让外部链接平滑迁移。

#### 2.5.2 标准做法（替代删除）
1. **重命名 / 迁移内容**：把内容迁到新文件后，将旧文件改写为废弃 stub（如 [R8.5_TessellatedMesh.md](R8.5_TessellatedMesh.md) → [TessellatedMeshDesign.md](TessellatedMeshDesign.md) 的实践）。
2. **临时调试脚本**：放在 `Docs/_tmp/` 或 `Scripts/_tmp/` 子目录，文件名加 `tmp_` 前缀，便于一次性 grep 出来。
3. **过时 cpp / 资产**：仅修改逻辑使其不被引用，不要 `Remove-Item`。

#### 2.5.3 对话末尾"待清理清单"格式（强制）

每次完成会引入"过时文件 / 临时脚本"的任务时，在最终回复**末尾**追加固定格式块：

```
### 待用户手动清理清单

| # | 路径 | 类别 | 已无下游引用？ | 建议操作 |
|---|------|------|----------------|----------|
| 1 | C:\workspace\...\R8.5_TessellatedMesh.md | 废弃 stub | ? 是（已 grep 确认） | `Remove-Item <path> -Force` |
| 2 | C:\workspace\...\Scripts\_tmp\xxx.ps1 | 临时调试脚本 | ? 是 | `Remove-Item <path> -Force` |
```

要求：
- **每条都附完整绝对路径**（用户可直接复制粘贴）。
- **类别**字段：`废弃 stub` / `临时调试脚本` / `过时 cpp` / `过时资产` / `临时日志` 等。
- **已无下游引用**列必须先 grep 验证后再写 `? 是`，未验证则写 `? 待复核`。
- **建议操作**给出可直接执行的 shell 命令字符串，但**不要**自己尝试执行。
- 若本次任务无任何待清理项，则无需追加该清单（保持回复整洁）。

---

## 3. UE 材质 / 纹理领域踩坑通则

> 这是本项目最重要的"领域知识沉淀"——AI 助手在 R8+ 必须默认掌握。

### 3.1 ? 三重 Material 槽位陷阱（R3 / R7 都踩过）

| 槽位 | 在 Details 哪里 | 数据流向 | cpp 反射诊断会触发吗 |
| --- | --- | --- | --- |
| ? 错的：Element 0 | `Rendering > Materials > Element 0`（PMC 通用槽） | 直接覆盖 PMC 材质，但 cpp 看不到 | **不会** |
| ? 对的：PlanetTopology > Material | 自定义分组 `PlanetTopology > Material`（cpp UPROPERTY） | cpp 读取 → 包装 MID → SetMaterial(0) | **会** |

判别法：Output Log 没有反射诊断 Warning 行 = 没进入 `if (Material)` = 挂错槽。

### 3.2 ? Texture Object Parameter 的两套默认值（R7 经典踩坑）

```
┌───────────────────────────────────────────────────────────┐
│ 位置 A：材质图节点本身的 Details > Texture                  │
│ ? shader 编译期资源绑定源                                   │
│ ? 留空 → 1×1 白纹理 fallback（不报错！）                   │
│ ? R7 必须填 T_TerrainAlbedoArray                           │
└───────────────────────────────────────────────────────────┘
┌───────────────────────────────────────────────────────────┐
│ 位置 B：主材质 / MI 的 Parameter Defaults 聚合面板          │
│ ? 仅 UI 投影 / MI 覆盖                                     │
│ ? 不回写位置 A、不影响 shader 编译                          │
└───────────────────────────────────────────────────────────┘
```

**典型症状**：Stats ? + R7-compliance ? + cpp 反射 ?，但 Lit 模式米白、Unlit 纯白 → 位置 A 空槽 fallback 到 1×1 白纹理。

**强制要求**：位置 A 必须挂 Texture2DArray 资产、`Sampler Type=Color`、`Sampler Source=Shared: Wrap`、`Parameter Name=TerrainAlbedoArray`（与 cpp `SetTextureParameterValue` 名一致）。

### 3.3 ? HLSL 在 UE Custom 节点的 4 项硬性限制

| 限制 | 错误现象 | 正确写法 |
| --- | --- | --- |
| ? 不允许嵌套定义函数 | `function definition is not allowed here` + `use of undeclared identifier 'Noise3D'`；**或编辑器静默失败、材质 stats 显示 0 instructions** | scoped block 内联（`{ ... }` 包局部变量）**或** `#define MACRO(IN, OUT) { ... }` 宏 |
| ? 不允许 `#include` | 无（要用节点的 `IncludeFilePaths` 字段） | inline 全部依赖 |
| ? RT ClosestHit 阶段不允许 `Sample(...)` | `Opcode Sample not valid in shader model lib_6_6(closesthit)` | 一律 `SampleLevel(samp, uv, 0)` |
| ? Inputs 顺序必须与 HLSL 形参一一对应 | 颜色出现不连续色块 / 编译错位 | 严格按详稿 §4.3.1 顺序声明 Inputs |

> R6 函数定义错误 + R7 RT Sample 错误 = HLSL 在 UE Custom 节点的两个最经典坑，必须默认掌握。

#### 3.3.1 ? "不允许函数定义"的根因与修复模板（R6 / R8 反复踩坑）

**根因**：UE 把 Custom 节点的 Code 字段直接拼接进 PS / VS 主函数体——所以 Code 字段本身就**已经在一个函数体内**了。在函数体里再写 `float Foo(...) { ... }` = 嵌套函数定义，HLSL 不允许。

**症状**：
- 如果 Code 编译成功了：Stats 面板显示 0 instructions（材质 fallback 到默认黑），运行时整球漆黑或不显示
- 如果 Code 编译失败：Output Log 出现 `function definition is not allowed here` 或 `use of undeclared identifier 'XXX'`（主函数找不到嵌套函数）

**两条修复路径**：

**路径 A — `#define` 宏（推荐，多处复用时）**：

```hlsl
// ? 错误：嵌套函数
float ValueNoise(float2 p)
{
    return frac(sin(dot(p, float2(12.9, 78.2))) * 43758.5);
}
float n1 = ValueNoise(uvA);
float n2 = ValueNoise(uvB);

// ? 正确：用宏，宏体 { ... } block 创建子作用域，输出变量在外部声明
#define VN(IN_P, OUT_N)                                          \
{                                                                \
    float2 _vp = (IN_P);                                         \
    OUT_N = frac(sin(dot(_vp, float2(12.9, 78.2))) * 43758.5);   \
}
float n1, n2;
VN(uvA, n1);
VN(uvB, n2);
```

宏的 4 个边界陷阱：
- ① 宏体外层必须 `{ ... }`，**不要写 `do { ... } while(0)`**——HLSL 不支持 `do-while`
- ② 续行符 `\` **后面不能有空格**——否则反斜杠续行失效，编译报 `unbalanced braces`
- ③ 宏内局部变量统一加 `_` 前缀（如 `_vp`），避免与调用方变量名冲突
- ④ 宏内**只赋值不能 `return`**——主函数体的 return 才能真正退出 PS

**路径 B — `{ ... }` block 内联（一次性、无复用时）**：

```hlsl
// 单次使用直接 inline：无需 macro，作用域天然隔离
float n1;
{
    float2 _vp = uvA;
    n1 = frac(sin(dot(_vp, float2(12.9, 78.2))) * 43758.5);
}
```

#### 3.3.2 三个不要犯的子错

| # | 错误写法 | 原因 |
| --- | --- | --- |
| ① | 直接复制 ShaderToy / Substance 的 `float Foo(...) { return ... }` 代码到 Code 字段 | ShaderToy 是顶层函数级（`mainImage` 主函数外可定义辅助函数）；UE Custom 是 PS 主函数体内，规则不同 |
| ② | 误以为"`#define MACRO(p) frac(sin(p))` 这种单行表达式宏可以"——然后宏内有多个语句 | 单行表达式宏只能写**单个表达式**，多语句必须用 `{ ... }` block 包装 |
| ③ | 文档里写"`float Foo(...) { ... }`"作为算法参考，没标注"不能直接粘贴" | 后人会直接 Ctrl+C/V 然后踩坑——务必在算法参考块顶部标记"? 这是数学参考，落地必须改为宏 / 内联" |

#### 3.3.3 项目内已落地样板

- [R8_ParametricTint.md §4.4.2.1](R8_ParametricTint.md)：`#define VN3D` + `#define SAMPLE_PARAM_COLOR` 主材质宏化版（推荐范式）
- [R8_ParametricTint.md §4.5.3.3 节点 ③](R8_ParametricTint.md)：`#define HASH21` + `#define VN2D` 水面材质宏化版
- [R8_ParametricTint.md §2.2 / §2.3](R8_ParametricTint.md)：函数语法只作算法参考，章节顶部已加显著警示

### 3.4 ? `SetTextureParameterValue` 类型一致性

> UE 5.x 隐藏约束：注入纹理类型必须与位置 A 默认纹理类型完全一致。

- 位置 A 是空 → 类型推断错误 → MID 注入静默失败
- 位置 A 是 Texture2D，注入 Texture2DArray → UE 拒绝但不报错

修复：位置 A 必须先挂正确类型的占位 Texture2DArray，MID 才能在运行时覆盖。

### 3.5 ? Sampler Type 与纹理格式必须对齐

| 纹理类型 | Sampler Type | Compression Settings |
| --- | --- | --- |
| BaseColor / Albedo（sRGB） | `Color` | Default (BC1/BC3) 或 BC7 |
| Normal Map | `Normal` | Normalmap (BC5) |
| Mask / 灰度 | `LinearGrayscale` 或 `Grayscale` | Mask 或 Default |
| LUT (FP32 RGBA) | `Linear Color` | UncompressedFP32 |

不对齐：编辑器红色错误，材质 fallback 到 WorldGridMaterial。

#### 3.5.1 ? Normal 贴图 Sampler Type 错设为 `Color` → World Normal 整片黄色（TerraSphericalTileGenerator 案例）

> **核心结论**：把法线贴图的 `Texture Sample` 节点 `Sampler Type` 保持默认的 `Color` 而不切换到 `Normal`，即便贴图本身导入设置正确（`TC_Normalmap` + `sRGB=false`），采样出来的 RGB 也**不会被 UE 自动 unpack 到 `[-1, +1]`**，会以 `[0, 1]` 的原始颜色直接进入 `Material.Normal` 引脚，经 TBN 变换后世界法线被整体扳向 `(+X, +Y, 0)`，表现为"XY 正方向光照亮、XY 负方向光死黑"。

##### 症状指纹

1. 场景里同高度的方向光，只要 XY 分量取正就能照亮 mesh，取负就死黑（`dot(L, N)` 恒为负被 clamp）。
2. Buffer Visualization → **World Normal**：正确朝上的地面应呈现**蓝色** `(0.5, 0.5, 1.0)`；本坑下 mesh 整体呈现**黄色** `(1.0, 1.0, 0.5)`，即世界法线 ≈ `(+1, +1, 0)`。
3. **不是**顶点绕序反了（那样会整片朝下 / 偏黑，不是黄色）。
4. **不是** UV 的 U/V 反了（那只会让法线贴图 XY 分量互换，Z 分量仍 ≈ 1，World Normal 仍近似蓝色）。
5. 关闭高度图（`HeightmapExtensionAmplitude = 0`）后现象**依然存在** → 排除几何顶点法线来源。

##### 排查方法（推荐顺序）

1. **先看 World Normal 可视化**：整片黄色 ≈ `(+1, +1, 0)` 是"切线空间法线未 unpack"的稳定指纹（因为未 unpack 时 `N_tangent ≈ (0.5, 0.5, ~1)`，经 TBN 展开约等于 `+T + +B + N`，加权后偏向 `(+X, +Y, 0)`）。
2. **打开父材质**，选中接到 `Material.Normal` 引脚的 `TextureSample` / `TextureSampleParameter2D` 节点：
    - Details 面板 → `Sampler Type` 应为 **`Normal`**，若为 `Color` / `LinearColor` 即为此坑。
3. **打开法线贴图资产**，确认：
    - `Compression Settings = TC_Normalmap`
    - `sRGB = false`
    - `Texture Group = WorldNormalMap`（推荐）
    - `Flip Green Channel = false`（NormalDX 无需翻绿）
4. 如果 `Sampler Type=Normal` 但贴图的 `Compression Settings` 不是 `Normalmap`，UE 会在材质编译时抛出 `"Texture is not a normal map"` 警告——**看到这类警告立即改贴图导入设置**，不要在材质里 workaround。

##### 修复

- **首选**：把 `Texture Sample` 的 `Sampler Type` 改成 `Normal`，同时确保贴图 `TC_Normalmap` + `sRGB=false`。UE 会在采样阶段内部执行 `2*rgb - 1` 的 unpack，直接把切线空间法线送进 `Material.Normal`。
- **应急**（不推荐作为长期方案）：在 `Texture Sample.RGB` 与 `Material.Normal` 之间插入 `ConstantBiasScale`（`Bias = -0.5`，`Scale = 2.0`），等价于手动 `N = RGB * 2 - 1`；但此法会绕过 UE 对法线贴图专用的 BC5 双通道压缩优化，仅供临时验证归因用。

##### 沉淀到 Agent 自查清单

Agent 在设计"参数化 PBR 父材质"型玩法（本项目 TerraSphericalTileGenerator、未来任何 ambientCG 材质集成）时，**主稿 / 详稿必须明确要求**：

- 父材质中的 Normal `TextureSampleParameter2D` 节点，`Sampler Type` 必须显式设为 `Normal`（不能沿用新建节点的默认 `Color`）。
- 文档"父材质接线"一节必须以**红字 / 加粗形式**提示这一点，并在"常见问题排查"表中给出"World Normal 整片黄色 → 检查 Sampler Type"这一行。
- 代码侧无法自动纠正此项（Sampler Type 属于材质图节点属性，非纹理资产属性），因此只能通过文档与验收清单兜底。

### 3.6 ? 顶点法线写法通则（UE5 左手系 + CCW 约定，经 R8 实测修订）

> **核心结论**（已修订）：UE5 是左手坐标系 + CCW frontface（[`D3D12State.cpp:356`](../../Program%20Files/Epic%20Games/UE_5.8/Engine/Source/Runtime/D3D12RHI/Private/D3D12State.cpp) `FrontCounterClockwise = true`）+ 漫反射用 `saturate(dot(N, L))`（[`ForwardLightingCommon.ush:387-392`](../../Program%20Files/Epic%20Games/UE_5.8/Engine/Shaders/Private/ForwardLightingCommon.ush)）。**对球外渲染的球面 mesh，顶点法线应朝外**（`+UnitCenter`）——与几何直觉完全一致。
>
> ? 早期版本（R7 期）曾误推为"应朝球心"，该结论已于 R8 期被实测证伪（详见 [SphereTopologyReference.md §11.4](SphereTopologyReference.md#114-关联踩坑历史与文档修订)）。

#### 3.6.1 硬性要求

| # | 要求 | 实施 |
| --- | --- | --- |
| ① | **球面 mesh 顶点法线首选 `+UnitCenter`（朝外）**——与几何直觉、SLW 反射、Lit 漫反射公式全部同向 | 例：`Normal = Cell.UnitCenter`。Tangents 留空（Default Lit 不消费切线空间；如需法线贴图再单独算 World→Tangent basis）|
| ② | **不要滥用 KismetTangents 自动法线** | 仅在"顶点跨三角形共享"的 mesh 上产出平滑法线；本项目"每 Corner 展开 3 独立顶点"拓扑下该工具等价 flat shading，会让阴影 / 光照边界出三角棱面锯齿（§3.13 / §3.14）|
| ③ | **禁止写 `-UnitCenter`（朝球心）** | 会让 `dot(N, L) ≤ 0` 全像素 → 整球漆黑。早期§3.6 这条写成"必须取负"，现已证伪并删除 |

#### 3.6.2 判别法

- View Mode 切 Lit ? Unlit：**Unlit 正常 + Lit 漆黑**是法线方向反向的指纹（不是材质 fallback、不是绕序错误、不是相机位置）；在本项目中最常见的原因是**误写 `-UnitCenter`**，改回 `+UnitCenter` 即恢复
- Buffer Visualization → World Normal viewmode：**朝光源一侧的半球应在 lit 后呈亮**；若反过来 → 法线方向反了

#### 3.6.3 错误归因路径（已避免重蹈，R7→R8 复盘）

R7 期曾报告一次 Lit 漆黑并被误归因为"需要朝内法线"，其实是多个问题被 KismetTangents 同时掩盖。R8 阶段用同一段代码独立 sign 翻转复现证伪了这个误结论。详见 [SphereTopologyReference.md §11.4](SphereTopologyReference.md#114-关联踩坑历史与文档修订)。

**详见**：[SphereTopologyReference.md §11](SphereTopologyReference.md#11-顶点法线与-ue5-光照约定重要结论--经验沉淀)（权威参考，已修订） / [SphericalSDFTerrainDesign.md §11.2](SphericalSDFTerrainDesign.md#112-pmcptg-渲染契约顶点法线与-ue5-光照约定)（PMC?PTG 跨期契约） / [R7_TerrainTriplanar.md §6](R7_TerrainTriplanar.md) 排错表"整球 Lit 模式漆黑"行。

### 3.7 ? UE5 左手系 + Z up 下的"自西向东"方向（W3 经典踩坑）

> **核心结论**：UE5 是**左手坐标系** + **Z up**。从 +Z 俯视，+X→+Y 是**顺时针**——这与地球北极俯视自转方向（**逆时针**，即"自西向东"）相反。所以在 UE5 中：**自西向东**（地理意义上的"东向"、即经度递增方向）= **+X → ?Y**，而不是凭"2D 逆时针旋转 90°"直觉的 +X → +Y。

#### 3.7.1 推导

> 球面以 +Z 为北极、本初子午线穿过 +X 方向。赤道上点 P = (cos λ, **?**sin λ, 0)（λ 是经度，λ=0 时 P=+X，向东转 → ?Y 方向）。
>
> 东向切向量 = dP/dλ = (?sin λ, **?**cos λ, 0)。
>
> 在 P = (x, y, 0) 时：sin λ = ?y、cos λ = x，故：
>
> **East(P) = (y, ?x, 0)**——也即代码里 `FVector East(U.Y, -U.X, 0.0f);`

#### 3.7.2 错误归因路径（W3 实际踩坑，2026-06-28 复盘）

W3 第一版落地的 `Step_SimulateMoisture` 凭"2D 逆时针旋转 90°"直觉写出 `FVector East(-U.Y, U.X, 0.0f);`（那是西向）→ 上风 SSSP 实际从海岸往**东**传播 → **大陆西岸湿润、东岸干燥**（与设计的"东风模型，东岸湿、西岸干"完全相反）。修复后改为 `(U.Y, -U.X, 0)`，恢复正确东西岸梯度。

| 反模式 | 表象 | 修复 |
| --- | --- | --- |
| 把"绕 +Z 逆时针 90° 旋转 (x,y,0)" 当成东向 → `(-y, x, 0)` | 在右手系 + Z up 下没问题，但 **UE5 是左手系**，"绕 +Z 逆时针" 在 UE5 里是 +X→?Y 而非 +X→+Y | `(y, -x, 0)`（**先验证手系再写公式**） |
| 想当然认为 +Y 是"东" | UE5 默认惯例确实把 +Y 视作"屏幕右"，但**地图上的"东"= 经度递增方向 = 从 +X 出发的旋转方向**，与"屏幕右"无关 | 永远从"经度公式 P(λ) → dP/dλ"推导，不靠几何直觉 |
| 忘记先验证：调试时不分东西、误以为是设计问题 | "湿度梯度反"被误归为参数过弱、SSSP 未收敛、风向选错 | **任何与"方位"挂钩的代码都先打 DrawDebug 红箭头沿 East 画一笔**，目视核对再继续 |

#### 3.7.3 通则（§3.6 同构性补充）

| # | 要求 | 实施 |
| --- | --- | --- |
| ① | **任何"东/南/西/北"语义的轴向公式都先验证手系** | UE5 = 左手系 + Z up；"自西向东" = +X→?Y；"自南向北" = 沿 +Z（北极）方向 |
| ② | **不要把 2D 旋转矩阵直觉迁移到 3D 球面** | 2D 课本默认右手系 + 逆时针正向；UE5 是左手系，旋转方向相反 |
| ③ | **先打 DrawDebug 箭头目视验证再写大段算法** | `DrawDebugDirectionalArrow(World, Cell.Center, Cell.Center + East*100, 5, FColor::Red, ...)`，看是否真的指向地球图上"东"那一侧 |

#### 3.7.4 高频轴向公式速查

| 球面位置（单位向量 U=(x,y,z)）| 切向量 | UE5 公式 |
| --- | --- | --- |
| 东向（经度递增）| dP/dλ | `FVector(U.Y, -U.X, 0).GetSafeNormal()` |
| 北向（纬度递增）| dP/dφ（与 East 正交、与 U 正交）| `FVector::CrossProduct(U, East).GetSafeNormal()`（左手系 cross 已对应"指向北"，即 +Z 半球）|
| 极区退化 | `\|East\| < ε` | 一律 `bPolar = true` 走各向同性分支，避免数值病态 |

> **教训**：本通则的存在是因为 W3.5 第一次落地时凭直觉写错了东向公式，导致大陆东西岸湿度梯度反。任何"自西向东"/"东风"/"上风"等方位语义的代码，**都必须先核对左手系下的公式再写**。

### 3.8 ? GameplayTag 注册：手放 `Config/Tags/*.ini` 重启**无效**——必须经编辑器"Add New Tag Source"动作（W4 经典踩坑）

> **核心结论**（2026-06-28 W4 用户实测纠正）：把 `Terrain.ini` 文件直接放到 `Config/Tags/` 目录下、然后重启编辑器**完全无效**——下拉框依旧为空。引擎在启动期虽然会 `AddTagIniSearchPath(FPaths::ProjectConfigDir()/"Tags")`（[`GameplayTagsManager.cpp:683`](../../Program%20Files/Epic%20Games/UE_5.8/Engine/Source/Runtime/GameplayTags/Private/GameplayTagsManager.cpp)），但实际 ini 的"被注册"还需要 GameplayTagsSettings/`UGameplayTagsList` 子对象在引擎启动期把它认作合法 Source。**必须经过编辑器内 "Add New Tag Source" 一次显式动作**，让引擎写盘并把该 ini 注册进 Source 列表，之后该 ini 才会被纳入扫描；否则即使文件已存在、内容格式正确、重启编辑器，依然不被加载。

#### 3.8.1 必须做（2026-06-28 修正）

| # | 要求 | 实施 |
| --- | --- | --- |
| ① | **不要直接手动创建 `Config/Tags/Foo.ini` 然后期望重启生效**——这是 W4 第一版的错误做法 | 哪怕文件已存在、UTF-8 BOM 正确、Section 正确、重启了编辑器，下拉框依旧为空 |
| ② | **必须经编辑器内 "GameplayTags → Manage Gameplay Tags → Add New Gameplay Tag → Source 下拉中输入 Foo.ini" 一次** | 编辑器会**创建 / 覆盖** `Config/Tags/Foo.ini`、把它注册成合法 Source；此时该 ini 才被加入 GameplayTagsSettings 的扫描列表 |
| ③ | **由编辑器自动生成 ini 后，再把详稿给的完整 `+GameplayTagList=` 行覆盖进去** | 编辑器只会写一个空 Source（仅含初始那个 Tag）；要把所有目标 Tag 一次写入，复制详稿提供的完整 ini 内容覆盖文件 → 再次重启编辑器，Tag 树就能完整加载 |
| ④ | **关键 / 启动期就需要的 Tag 可直接放 `Config/DefaultGameplayTags.ini` 内联** | 该文件由 [`GameplayTagsManager.cpp:278`](../../Program%20Files/Epic%20Games/UE_5.8/Engine/Source/Runtime/GameplayTags/Private/GameplayTagsManager.cpp) 硬编码强制读取（`static const FName NAME_DefaultGameplayTagsIni`），无需任何注册动作；适合 cpp 端有 `FNativeGameplayTag` 引用的 Tag，但与 W4 数据驱动思路不同——本项目 W4 采用方案②③ |
| ⑤ | **不要重复注册同一 Tag** | 同一 Tag 只能在一处 `+GameplayTagList=`；多处出现会触发 `WarnOnInvalidTags` 告警；grep `Config/Tags/*.ini` + `DefaultGameplayTags.ini` 互斥检查 |
| ⑥ | **`UPROPERTY meta=(Categories="X.Y")` 限制下拉框时一定要写对前缀** | 例：`meta=(Categories="Terrain")` 限定下拉只显示 `Terrain.*`；前缀写错 → 下拉框为空（现象与 ini 没加载完全一样，需排除）|

#### 3.8.2 错误归因路径（W4 实际踩坑 + 修正 ，2026-06-28 完整复盘）

| 阶段 | 操作 | 结果 |
| --- | --- | --- |
| 第一版 | Agent 在 `Config/Tags/Terrain.ini` 写了 17 个 `+GameplayTagList=`；不进编辑器，期望它会被自动扫描 | DA 资产 `TerrainTag` 下拉框为空 |
| 第二版 | 进入 Project Settings → GameplayTags → 点了"导入"按钮 | **仍为空**（"导入"按钮只重读已注册 Source 列表中已纳入的 ini，不重扫文件系统） |
| 第三版 | 完全关闭并重启 UnrealEditor.exe | **仍为空**（即便引擎启动期 SearchPath 注册的是 `Config/Tags/` 目录，该目录下的 ini 也不会自动成为 GameplayTagsSettings 认可的 Source） |
| 第四版（实测有效） | 在编辑器内 "Add New Gameplay Tag Source" 输入 `Terrain.ini` → 编辑器自动创建/覆盖 `Config/Tags/Terrain.ini` 并把它注册为合法 Source → 把详稿给的完整 ini 内容粘贴覆盖 → 重启编辑器 | ? 17 个 Tag 全部出现 |

> **错误根源**：作者前几版误读了 `AddTagIniSearchPath` 在第 683 行的字面行为，以为 SearchPath 注册了就等于自动扫描；实际上**目录被 SearchPath 注册 ≠ 该目录下的新文件会被自动当作 Source**——必须经编辑器 UI 动作或 `FindOrAddTagSource(...)` 显式登记。

#### 3.8.3 推荐流程（W4 / 后续任何新增 GameplayTag ini 的标准流程）

```
┌────────────────────────────────────────────────────────────────┐
│ Step 1: cpp / Build.cs / .uproject 该改的先改完，编译通过       │
└────────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌────────────────────────────────────────────────────────────────┐
│ Step 2: 启动编辑器（此时 ini 不必预先存在）                      │
│   ? Project Settings → GameplayTags                            │
│   ? Manage Gameplay Tags → 顶部 "Add New Gameplay Tag"         │
│   ? Source 下拉框中输入新 ini 文件名（如 Terrain.ini）          │
│   ? Tag 字段填随便一个占位 Tag（之后会被覆盖）                   │
│   ? 点 Add New Tag                                              │
│   → 编辑器自动创建/覆盖 Config/Tags/Terrain.ini，并把它注册     │
└────────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌────────────────────────────────────────────────────────────────┐
│ Step 3: 关闭编辑器 → 用文本编辑器把详稿提供的完整 ini 内容粘贴   │
│         覆盖 Config/Tags/Terrain.ini                            │
│         （编辑器创建出来的版本只含一个占位 Tag）                  │
└────────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌────────────────────────────────────────────────────────────────┐
│ Step 4: 重启编辑器 → 17 个 Tag 全部出现在 DA 字段下拉框         │
└────────────────────────────────────────────────────────────────┘
```

#### 3.8.4 三段诊断法（下拉框为空时按顺序排查）

```
┌──────────────────────────────────────────────────────────┐
│ Layer 1: ini 是否被引擎注册为 Source（最常见根因）         │
│   ? 进 Project Settings → GameplayTags → Manage           │
│     Gameplay Tags → 看 "Sources" 列表是否包含该 ini      │
│   ? 不在列表中 → 走 §3.8.3 Step 2 用编辑器 Add New Source │
│   ? 已在列表中 → 跳到 Layer 2                              │
└──────────────────────────────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────────┐
│ Layer 2: ini 文件内容                                     │
│   ? 路径 Config/Tags/*.ini（不是 Config/*.ini）             │
│   ? Section [/Script/GameplayTags.GameplayTagsList]       │
│   ? 行格式 +GameplayTagList=(Tag="Foo.Bar",DevComment="…") │
│   ? 编码 UTF-8 / UTF-8 BOM 都可                            │
└──────────────────────────────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────────┐
│ Layer 3: UPROPERTY 元数据 Categories                     │
│   ? 字段是否加了 meta=(Categories="...")？                  │
│   ? 如有，前缀必须与 ini 中 Tag 命名空间一致                │
│   ? 拿掉 Categories 测一次；立即出现 → 元数据写错了        │
└──────────────────────────────────────────────────────────┘
```

#### 3.8.5 通则（§3.6 / §3.7 同构性补充）

| # | 要求 |
| --- | --- |
| ① | 任何 GameplayTag ini **不要靠手放文件**——一定走编辑器 "Add New Gameplay Tag Source" 动作让它被注册 |
| ② | 数据驱动方案（如 W4 的 `Terrain.*` Tag）的验收清单第一项是 **"在编辑器内创建 Tag Source 并粘贴详稿 ini 内容"**，*不是* "重启编辑器"——重启对未注册的 ini 完全无效 |
| ③ | 凡读引擎源码得到行为推论时，必须**写最小复现**实测验证，*不要*基于"看起来 SearchPath 注册了就该自动扫描"的字面理解发布通则；W4 第一/二/三版就是凭源码字面推论给了错误指引（[详见 §3.8.2 复盘表](#382-错误归因路径w4-实际踩坑--修正-2026-06-28-完整复盘)）|
| ④ | 在详稿 §6 排错表为每个新增 ini 配置预先写一行"下拉框为空"，给后人留档（[W4_BiomeClassification.md §6](W4_BiomeClassification.md) 已落地）|

---

### 3.9 ? `TUniquePtr<前向声明类型>` 的析构契约（R8 水面 Actor 经典踩坑）

#### 3.9.1 现象

新增 cpp Actor `APlanetWaterShell` 持有 `TUniquePtr<FSphereTopology> Topology`；在 .h 中按"最小依赖"原则只前向声明 `class FSphereTopology;`，未在 .h 显式声明析构函数。编译报错：

```
error C2027: use of undefined type 'FSphereTopology'
error C2338: static_assert failed: 'cannot delete an incomplete type'
note: while compiling class template member function 'TDefaultDelete<...>::operator()'
note: instantiated in Module.TerraCivilization.gen.cpp
```

错误指向 `.gen.cpp`——而 `.gen.cpp` 是 UHT 自动生成的，那里**只 include 了 .h（前向声明）而没 include `FSphereTopology.h`**，因此 `delete Topology.Get()` 无法看到完整类型。

#### 3.9.2 根因（UE 5.8 [UniquePtr.h:55-60](../../../Program%20Files/Epic%20Games/UE_5.8/Engine/Source/Runtime/Core/Public/Templates/UniquePtr.h) 官方说明）

`TDefaultDelete<T>::operator()` 上方的注释明确给出修复模板：

> If you get an error here when trying to use a `TUniquePtr<FForwardDeclaredType>` inside a UObject then:
> - Declare all your UObject's constructors and destructor in the .h file.
> - Define all of them in the .cpp file. You can use `UMyObject::UMyObject() = default;` to auto-generate the default constructor and destructor so that they don't have to be manually maintained.
> - Define a `UMyObject(FVTableHelper& Helper)` constructor too, otherwise it will be defined in the .gen.cpp file where your pimpl type doesn't exist. It cannot be defaulted, but it need not contain any particular implementation.

**为什么 VTableHelper 也要显式定义**：UHT 在 `.gen.cpp` 里默认生成 `Class(FVTableHelper&) = default`，同样会触发"看不到完整类型"问题。把它显式定义在 .cpp（已 include 了完整类型的地方），就把析构链路全部限制在 .cpp 里了。

#### 3.9.3 修复模板（强制三件套）

**.h**：声明三个特殊成员（注意 `virtual` + `= default` 都不放在 .h）：

```cpp
public:
    APlanetWaterShell();

    /** 显式声明析构 + VTableHelper 构造，原因：持有 TUniquePtr<前向声明类型>。 */
    virtual ~APlanetWaterShell();
    APlanetWaterShell(FVTableHelper& Helper);
```

**.cpp**：定义三个成员，include 完整类型（如 `#include "FSphereTopology.h"`）：

```cpp
APlanetWaterShell::APlanetWaterShell() { /* 正常初始化 */ }
APlanetWaterShell::~APlanetWaterShell() = default;             // 必须 .cpp，可 = default
APlanetWaterShell::APlanetWaterShell(FVTableHelper& Helper) : Super(Helper) {}  // 不能 = default
```

#### 3.9.4 三条不要犯的子错

| # | 错误写法 | 原因 |
| --- | --- | --- |
| ① | `virtual ~APlanetWaterShell() = default;`（写在 .h）| `= default` 让编译器在 .h 处实例化析构 → 仍触发 `.gen.cpp` 路径，原 bug 复现 |
| ② | `APlanetWaterShell(FVTableHelper& Helper) = default;`（.cpp）| VTableHelper 构造 *不能* `= default`——基类 `AActor::AActor(FVTableHelper&)` 不是 trivially-constructible，必须显式 `: Super(Helper) {}`（与 [APlanetBinder](../Source/TerraCivilization/Private/Interaction/PlanetBinder.cpp) / [APlanetTopologyDebugMesh](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp) 同款）|
| ③ | "为了简单，干脆在 .h `#include "FSphereTopology.h"`" | 短期能编过，长期破坏 .h 最小依赖原则——下游文件（蓝图反射头、UI 模块、Editor 模块）都会被强制拉入 Grid 模块，编译时间 / 头文件膨胀指数级上升 |

#### 3.9.5 通用规则

> 凡 `UObject` 子类持有 `TUniquePtr<T>` 且 T 在 .h 仅前向声明 → **必须执行 §3.9.3 三件套**。
>
> 不仅适用于 `TUniquePtr`，也适用于 `TPimplPtr` / `TSharedPtr<T, ESPMode::NotThreadSafe>` 等其他 RAII 包装。
>
> 项目内已落地的样板：
> - [APlanetBinder](../Source/TerraCivilization/Public/Interaction/PlanetBinder.h)（PTG 早期，最早踩坑）
> - [APlanetTopologyDebugMesh](../Source/TerraCivilization/Public/Render/PlanetTopologyDebugMesh.h)（R1 拓扑可视化，持有 `TUniquePtr<FSphereTopology>` 与 `TUniquePtr<FWorldGenerator>`）
>
> R8 水面层早期版本 `APlanetWaterShell` 也踩过同一坑并按本模板修复，后因独立 Actor 生命周期问题（详见 §3.10）整个被删除，水面 mesh 改为 `APlanetTopologyDebugMesh` 的子组件 `WaterMeshComp`。
>
> 三处的修复手法**完全一致**——下次再写新 Actor 持有前向声明 pimpl 时，直接按 §3.9.3 模板抄即可。

---

### 3.10 ? `OnConstruction` 中 `SpawnActor` 的 Editor / PIE 生命周期错位（R8 水面层经典踩坑）

#### 3.10.1 现象

R8 水面层早期实现：在 `APlanetTopologyDebugMesh::Rebuild()`（由 `OnConstruction` 调用）末尾用 `World->SpawnActor<APlanetWaterShell>(WaterShellClass, ...)` 创建一个独立水面 Actor，再把它 `AttachToActor` 到本 Actor。Editor 中放置正常——能看到一颗贴着水纹的半透明球。但**点 PIE 后**：

1. 水面 Actor 在视觉上**变成不透明的默认棋盘格球**（材质丢失）；
2. 退 PIE 时水面 Actor 整个消失，但 Details 里的 `WaterShellClass` 字段仍在。

也就是 **Editor 视觉与 PIE 视觉不一致**，无法用 PIE 验收水面材质效果。

#### 3.10.2 根因

PIE 启动时 UE 会把整个 Editor World **深拷贝**到 PIE World：

1. 在 `OnConstruction` 中 `SpawnActor` 出来的子 Actor 是**未持久化到 .umap 的临时 Editor Actor**（不在 outliner 选中链路）。深拷贝时它会被一起复制过去，但其中的 `TObjectPtr<UMaterialInterface> WaterMaterial` 引用**没有写进 .umap**——拷贝后变成 None。
2. PIE 启动后 `OnConstruction` **会再跑一次**，又触发一次 `SpawnActor` + 销毁旧的扫描循环；但旧的 attach 关系在深拷贝时被破坏，扫描循环可能误判，最终留下材质空槽的 Actor。
3. 退 PIE 时整个 PIE World 被回收，那个 SpawnActor 出来的 Editor 临时实例一并消失。

**核心：`OnConstruction` 中 SpawnActor 的子 Actor 处于"既不属于 Editor 持久层、又会被 PIE 拷贝"的灰色生命周期带，UPROPERTY 引用在拷贝过程中无保障**。

#### 3.10.3 修复模板：把子 Actor 改为 Owner Actor 的 SubObject Component

`APlanetTopologyDebugMesh` 不再持有 `TSubclassOf<AActor> WaterShellClass`，而是**直接拥有一个子组件**：

```cpp
// .h
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "...|R8")
bool bEnableWaterShell = false;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "...|R8")
TObjectPtr<UMaterialInterface> WaterMaterial;

UPROPERTY(VisibleAnywhere, Category = "...|R8")
TObjectPtr<UProceduralMeshComponent> WaterMeshComp;
```

```cpp
// .cpp 构造函数（注意：CreateDefaultSubobject 必须在构造函数中调用，
// 不能在 OnConstruction / Rebuild 中创建——否则又落入"非持久 SubObject"陷阱）
WaterMeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterMeshComp"));
WaterMeshComp->SetupAttachment(MeshComp);   // attach 到 Root 的兄弟链
WaterMeshComp->SetVisibility(false);        // 默认隐形
// ...其它 PMC 配置（Movable / NoCollision / NoNavigation / AsyncCooking）
```

```cpp
// .cpp Rebuild() 末尾
RebuildWaterMesh_();   // 内部按 bEnableWaterShell + WaterMaterial 重建 mesh + SetMaterial
```

`Component` 是 Owner Actor 的 **SubObject**——PIE 深拷贝跟着 Owner 整体走，UPROPERTY 引用**一起序列化/拷贝**，无任何额外管理代码、无任何生命周期窗口。

#### 3.10.4 三条不要犯的子错

| # | 错误写法 | 原因 |
| --- | --- | --- |
| ① | 在 `OnConstruction` 中 `NewObject<UProceduralMeshComponent>` 而不是构造函数中 `CreateDefaultSubobject` | OnConstruction 中创建的 Component 也属于"非持久 SubObject"，PIE 拷贝时同样会丢字段 |
| ② | 改用 `Components Hierarchy` 蓝图节点动态 attach 一个 PMC | 蓝图侧动态 Component 的 UPROPERTY 引用同样不进 .umap，PIE 拷贝丢失 |
| ③ | 给 SpawnActor 加 `Params.ObjectFlags = RF_Transactional \| RF_DefaultSubObject` 试图骗持久层 | UE 不允许这样标记非构造期 SpawnActor 出来的 Actor，会触发 ensure；而且即便骗过去，`RF_DefaultSubObject` 在跨 World 拷贝时不被尊重 |

#### 3.10.5 通用规则

> **凡需要"被 Owner Actor 联动构建 / 销毁"的视觉子对象（mesh、贴图、材质实例、灯光等）→ 必须用 Component（SubObject），不要用 SpawnActor 出来的子 Actor。**
>
> 唯一可以用子 Actor 的场景：该子 Actor 自身需要拥有独立的 Replication / GameplayLogic / AbilitySystem，是一个"游戏实体"而非"视觉装饰"。
>
> 项目内已落地的样板：
> - [APlanetTopologyDebugMesh::WaterMeshComp](../Source/TerraCivilization/Public/Render/PlanetTopologyDebugMesh.h)（R8 水面层，本节复盘对象）
> - [APlanetTopologyDebugMesh::MeshComp](../Source/TerraCivilization/Public/Render/PlanetTopologyDebugMesh.h)（R1 主 mesh，从一开始就走 Component 路径，未踩坑）

---

### 3.11 ? PIE 退出后 Editor 中 MID/Material 引用失效（R8 反复踩坑）

#### 3.11.1 现象

Editor 中放置 `APlanetTopologyDebugMesh` 后 Rebuild 显示正常；进入 PIE 一切正常；**退出 PIE 回到 Editor 后**，主 mesh 与水面 mesh 都变成不透明的默认白材质 / 棋盘格——**但 Material Stats 仍然正常**（说明材质本身没坏），且**再次 Rebuild 或再进 PIE 立即恢复**。

> 关键诊断特征："PIE 退出后才丢、Rebuild 又有了" → 一定是 MID/材质引用层失效，而**不是几何/数据层问题**。

#### 3.11.2 根因（UE 5.x DuplicateWorld + GC 行为）

PIE 启动时 UE 把整个 Editor World **深拷贝**（`UEditorEngine::DuplicateWorldForPIE`）成 PIE World：

| 阶段 | Editor World | PIE World |
| --- | --- | --- |
| 启动前 | A_editor + MeshComp_editor + MID_editor | — |
| PIE 启动后 | 同上（保留） | A_pie + MeshComp_pie + MID_pie（duplicate）|
| PIE 期间 | MeshComp_editor.SceneProxy → MID_editor | MeshComp_pie.SceneProxy → MID_pie |
| PIE 退出（Cleanup PIE World）| MeshComp_editor.SceneProxy → **MID_pie 已被 GC，引用悬挂** | A_pie / MID_pie / MeshComp_pie 全部 GC |
| 渲染线程刷新 RenderProxy | fallback 到 `UEngine::DefaultMaterial` | — |

**触发关键**：`UMaterialInstanceDynamic::Create(Material, this)` 用本 Actor 作为 Outer。**duplicate 时 MID 跟随 Owner 一起进 PIE**，PIE 退出销毁 PIE World 时 MID_pie 被 GC，`MeshComp_editor.SceneProxy` 在某些刷新路径上拿到的 `OverrideMaterials[0]` 引用就指向了 stale 对象，最终 fallback。

类似的引用失效也会发生在：
- `Texture2D` 通过 `CreateTransient(...)` 创建（Outer = TransientPackage，但被 MID_pie 引用 root → MID_pie GC 后 LUT 也可能被一起回收）
- `UStaticMeshComponent` 持有的 Override Material
- 任何 cpp 端 `NewObject<...>(this, ...)` 创建的视觉资源

#### 3.11.3 修复模板：监听 `FWorldDelegates::OnPostWorldCleanup`

```cpp
// .h
private:
    FDelegateHandle PostWorldCleanupHandle;

    /** PIE 退出后 MID/Material 引用恢复回调。 */
    void OnPostWorldCleanup_(UWorld* World, bool bSessionEnded, bool bCleanupResources);
```

```cpp
// .cpp 构造函数末尾
#if WITH_EDITOR
    PostWorldCleanupHandle = FWorldDelegates::OnPostWorldCleanup.AddUObject(
        this, &AMyActor::OnPostWorldCleanup_);
#endif

// .cpp 析构函数（不能用 = default，必须 Remove 防止 dangling）
AMyActor::~AMyActor()
{
#if WITH_EDITOR
    if (PostWorldCleanupHandle.IsValid())
    {
        FWorldDelegates::OnPostWorldCleanup.Remove(PostWorldCleanupHandle);
        PostWorldCleanupHandle.Reset();
    }
#endif
}

// .cpp 回调实现
void AMyActor::OnPostWorldCleanup_(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
#if WITH_EDITOR
    // 防御 1：本 Actor 即将销毁 → 不能 Rebuild
    if (!IsValid(this) || HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed)) return;

    UWorld* MyWorld = GetWorld();
    // 防御 2：被 cleanup 的就是本 Actor 所在 World → 即将销毁，跳过
    if (!MyWorld || MyWorld == World) return;

    // 防御 3：仅当游戏会话真的结束（PIE/Standalone 退出）时恢复，切关卡 / 编辑器关闭跳过
    if (!bSessionEnded) return;

    // 防御 4：仅当本 Actor 在 Editor World（PIE 退出 → Editor 端材质失效）时才恢复
    if (MyWorld->WorldType != EWorldType::Editor &&
        MyWorld->WorldType != EWorldType::EditorPreview) return;

    // 安全 → 重建一次（重新创建 MID + SetMaterial）
    Rebuild();
#endif
}
```

> **`FWorldDelegates::OnPostWorldCleanup` 在 Engine 模块** —— 无需依赖 UnrealEd，Build.cs 无需新增模块依赖。Standalone 构建路径下 `WITH_EDITOR=0`，回调不注册不触发，零额外开销。

#### 3.11.4 四条防御为什么都需要

| 防御 | 不写会怎样 |
| --- | --- |
| ① `IsValid(this) + RF_BeginDestroyed/FinishDestroyed` | 本 Actor 自己被销毁过程中收到 cleanup 通知（编辑器关闭场景），直接 Rebuild 会触发悬挂访问 crash |
| ② `MyWorld == World` | 编辑器关闭 → Editor World 自己 Cleanup → 此时 `this` 的 World 即将销毁，Rebuild 中所有 SubObject 操作未定义 |
| ③ `bSessionEnded` | 切关卡（streaming level load/unload）也走 Cleanup 路径但 `bSessionEnded=false`；不过滤会导致每次切关卡都触发不必要的 Rebuild |
| ④ `WorldType != Editor` | 本 Actor 自身就在 PIE/Game 中（玩家关卡里跑游戏），此时不会发生跨 World duplicate 问题；不过滤会在游戏运行时多余触发 |

#### 3.11.5 不要用的替代方案

| 错误方案 | 失败原因 |
| --- | --- |
| 在 `UMaterialInstanceDynamic::Create(Material, GetTransientPackage())` 改 Outer | 改成 TransientPackage 后 PIE duplicate 不再 fork MID，但 PIE 期间 MID 上注入的参数会污染 Editor MID（因为是同一个对象），PIE 退出后参数变成 PIE 最后一帧的状态 |
| 在 `PostEditChangeProperty` / `PostLoad` 中重建 | 这两个钩子 PIE 退出**不会触发** —— Editor Actor 完全没收到任何属性变更或加载事件 |
| 给 MID 字段加 `RF_RootSet` 防 GC | RootSet 是全局根集合，会让 PIE GC 把 MID_pie 也一并保留——内存泄漏 + 渲染状态错乱 |
| `FEditorDelegates::EndPIE`（UnrealEd 模块）| 能用但要给 Build.cs 加 UnrealEd 模块依赖 + 包 `WITH_EDITOR`；用 `FWorldDelegates::OnPostWorldCleanup` 等价且更轻量 |
| 改 `UPROPERTY` 标志加 `DuplicateTransient` | 会让 MID 在 duplicate 时被跳过 —— PIE World 里 MID 是 nullptr，PIE 期间就显示棋盘格了，问题被前移 |

#### 3.11.6 通用规则

> 凡是 cpp 端 `NewObject<T>(this, ...)` / `UMaterialInstanceDynamic::Create(Mat, this)` / `CreateTransient(...)` 创建的"视觉资源对象"（材质实例、动态纹理、动态 mesh 数据等），且**只在 Editor 中通过 `OnConstruction` 路径填充**的——
>
> **必须**注册 `FWorldDelegates::OnPostWorldCleanup` 兜底回调，按 §3.11.3 模板实现 4 个防御 + Rebuild 调用，确保 PIE 退出后 Editor 视觉一致。
>
> 如果 Actor 同时还运行在 PIE/Game 中（实际游戏 Actor），还要在 `BeginPlay` 中再触发一次 Rebuild 或做运行态缺失检测。详见 §3.11.7。
>
> 项目内已落地的样板：
> - [APlanetTopologyDebugMesh::OnPostWorldCleanup_](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp)（本节复盘对象，R8 主 mesh + 水面层）

#### 3.11.7 Packaged GameWorld 必须重建纯 C++ 运行态（TessellatedMesh 棋子缺失踩坑）

##### 现象

`BP_PlanetTessellatedMesh` 在 PIE 中正常，打包 Development 后进入 `TessellatedMeshTestMap` 只剩一个 `PlanetTessellatedMesh`，没有任何棋子 Actor。

临时诊断日志显示：

```text
[Tess][PackageDiag] BeginPlay ... IsGameWorld=1 CellTopology=0 MeshTopology=0 Generator=0 GameplayValid=0 GameplayInitialized=0 PresentedPieces=0
```

##### 根因

`APlanetTessellatedMesh` 的 `CellTopology` / `MeshTopology` / `Generator` / `GameplayContainer` 都是纯 C++ `TUniquePtr` 运行态对象，不会序列化进 cooked map。旧实现主要依赖 `OnConstruction -> RebuildAll_()` 在 Editor/PIE 路径中把这些状态建好；打包 GameWorld 启动时这些指针可能为空，于是 `RebuildGameplay_()` 没有有效依赖，`UTerraPiecePresentationManager` 收不到棋子 snapshot，自然不会 spawn 棋子 Actor。

##### 修复

`APlanetTessellatedMesh::BeginPlay` 必须在 `World->IsGameWorld()` 下检查运行态是否缺失：

- `CellTopology` / `MeshTopology` / `Generator` 任一无效
- `GameplayContainer` 无效或未初始化

命中后直接调用 `RebuildAll_()`，让打包运行时重新建立双拓扑、WorldGen、Gameplay 容器，并继续走 `SyncP1PiecePresentation_()` 生成棋子 Actor。

##### 通用规则

凡 `OnConstruction` 中填充的对象满足以下任一条件，都不能假设它会存在于 cooked GameWorld：

- `TUniquePtr` / 普通 C++ 容器持有的拓扑、生成器、查询结构、规则容器
- `CreateTransient` / `NewObject` / `UMaterialInstanceDynamic::Create` 得到的运行态资源
- 依赖 Editor construction 路径派生出来、但没有直接序列化到 `.umap` 的表现层缓存

这类 Actor 如果会进入实际游戏，必须在 `BeginPlay` 做 GameWorld 运行态重建或缺失检测。以后遇到“PIE 正常、打包后缺对象 / 缺材质 / 缺缓存”的问题，第一步先在 `BeginPlay` 打印这些运行态指针是否有效，而不是先怀疑资产 cook。

---

### 3.12 ? Translucent 不接收 SSR / 反射探针 → 水体应使用 Single Layer Water shading model（R8 水面层经典踩坑）

#### 3.12.1 现象

R8 验收期把水面材质 `M_WaterShell` 配置为：

- Blend Mode = `Translucent`
- Shading Model = `Default Lit`
- Translucency → Lighting Mode = `Surface ForwardShading`（已按"看似最优"配置）
- Specular = 0.5~1.0、Roughness = 0.05~0.2、Normal = fbm 法线扰动

实际效果：水面**只是浅蓝色透明球，无任何反射 / 高光，PIE 期间也看不出时间流动**。

强力诊断（详见 §3.12.4 测试 A）证伪了"通路坏了"假设：把 Blend Mode **临时改为 `Opaque` + Roughness=0**，反射立刻显著、fbm 噪声波纹流动也清晰可见——说明 fbm / sparkle 通路本身完全正确。

#### 3.12.2 根因

**UE5 Translucent shading model 在不开 Project-wide Forward Shading 的前提下，物理上无法接收 ScreenSpaceReflection / ReflectionCapture / SkyLight 反射**。这是 UE5 渲染管线的硬约束——Translucent 走的是 Forward 简化路径，默认管线下：

| 反射源 | Translucent 接收？ |
| --- | --- |
| ScreenSpaceReflection | ?（除非在 Translucency 分组里勾 Screen Space Reflections，且效果有限）|
| ReflectionCapture（Box/Sphere）| ? |
| SkyLight 实时 capture | ? |
| Lumen Reflections | ? |
| 太阳 specular（DirectionalLight）| ?（但只是单方向，不算"反射"，是"高光"）|

所以 Translucent 水面只能看到一点点太阳高光（如果 Roughness 极低且光向恰好对着相机），但**永远看不到天空映在水面上**。这与"水的反射"视觉直觉强烈冲突，也是新手在 UE5 中做水体的最高频踩坑。

#### 3.12.3 修复路径

**首选：切到 `Single Layer Water` Shading Model**（UE5 官方 Water Plugin 用的同一套）

| 配置项 | 改为 |
| --- | --- |
| Blend Mode | `Opaque` ? |
| Shading Model | `Single Layer Water` ? |
| Two Sided | ? |
| Translucency Lighting Mode | 失效（Opaque 模式下不可见）|
| 主节点 Opacity 引脚 | 失效（Opaque 模式下不可见）|

SLW 本质是不透明渲染（写深度、参与 GBuffer），但内置"水下颜色透出 + SkyLight 反射 + Lumen 反射 + 太阳 specular"的整套近似——既透又反，且性能比 Translucent 还好。

新增 `Single Layer Water Material Output` 节点（材质图右键搜索添加），4 个引脚：

- Scattering Coefficients：水的散射颜色（蓝绿调）
- Absorption Coefficients：水的吸收颜色（红光衰减最快）
- Phase G：散射各向异性（先用 0）
- Color Scale Behind Water：水下颜色染色（先用 (1,1,1) 让水下原色透出）

场景前置条件**缺一不可**：

| # | 条件 | 缺失症状 |
| --- | --- | --- |
| ① | DirectionalLight 勾 `Atmosphere Sun Light` | 无太阳 specular highlight |
| ② | 场景里有 ASkyLight + 勾 `Real Time Capture` | **反射全黑** |
| ③ | Project Settings → Reflection Method ≠ None | SSR 失效 |

> **可选**：Sphere/Box ReflectionCapture 提升局部反射；Lumen Reflections 是默认推荐路径。

完整 R8 SLW 实现见 [R8_ParametricTint.md §4.5.3](R8_ParametricTint.md)。

**次选**（不推荐，仅作记录）：保留 Translucent，开启 Project Settings → Rendering → **Forward Shading** ? + 材质 Translucency 勾 Screen Space Reflections + Forward Shading 勾 High Quality Reflections。这条路会**全项目所有材质切到 Forward 渲染**——影响 Lumen / Nanite 兼容性，R8/T 阶段不要为一个测试球开此项目级开关。

**末选**（伪反射，仅作 placeholder）：保留 Translucent + Default Lit，承认无真反射，把 sparkle 通过 Emissive 显式画出来。视觉够用但不是物理反射，跟相机角度 / 光源位置无关。R8 验收期已抛弃此路径。

#### 3.12.4 二分诊断流程（强信号、不可错认）

当遇到"水面看起来不动 / 没反射"时，按下面流程二分定位根因：

**测试 A：fbm 是否在跑？**

把 Custom Node A（Wave Noise，输出 Float1）输出 → Multiply ×3 → 接 Emissive Color；Blend Mode 临时改 Opaque、Opacity 接 1。Emissive 不走光照不走 Specular，**只要 fbm 在跑就一定能看到漂移噪声**。

| 现象 | 结论 |
| --- | --- |
| 缓慢漂移的灰度噪声 | fbm OK，问题在反射通路 → 跳测试 B |
| 静态不动 | Time 没接 / fbm 跑死 |
| 纯白纯黑 | VN 宏续行符问题 |

**测试 B：反射通路是否在工作？**

把 M_WaterShell 临时改为 Blend Mode = Opaque + Roughness = 0 + Specular = 1（不改 Shading Model）。

| 现象 | 结论 |
| --- | --- |
| 反射显著 | Default Lit + Opaque 下反射正常 → 100% 是 Translucent 的 SSR/Capture 不接收问题 → 切 SLW |
| 仍然没反射 | 场景缺 SkyLight / Lumen Reflections 关闭，先修场景再谈 |

#### 3.12.5 通用规则

> 凡是"既要透光又要反射"的视觉对象（水体、薄冰、玻璃、半透明能量球）：
>
> - 水体 → **Single Layer Water**（首选）
> - 薄冰 / 玻璃 / 半透明能量球 → 评估 SLW 是否够用；如果不够再考虑 Translucent + Forward Shading 全开
>
> **绝不要**用 Translucent + Default Lit + 期待真反射 —— UE5 物理上不支持，是新手最高频陷阱。
>
> 项目内已落地的样板：
> - [R8_ParametricTint.md §4.5.3](../Docs/R8_ParametricTint.md)（M_WaterShell SLW 完整节点级实现，本节复盘对象）

---

### 3.13 ? 独立顶点 mesh + KismetTangents 自动法线 = 隐式 flat shading（R8 水面层经典踩坑）

#### 3.13.1 现象

R8 水面层 `WaterMeshComp` 用 sub=3 球面拓扑，几何路径与主 mesh 完全相同——**每个 Corner 展开 3 个独立顶点（不共享）**——共 1280 三角形 / 3840 顶点。法线交给 `UKismetProceduralMeshLibrary::CalculateTangentsForMesh` 自动计算（这是 §3.6 推荐的"避免手填法线坑"的标准做法）。

实际效果：水面 SLW 反射能看到天空，时间流动也对，**但球面表面有清晰可见的 1280 个三角小棱面**——光滑反射被棱角"切割"成片状。

#### 3.13.2 根因

`CalculateTangentsForMesh` 的算法是：

1. 对每个三角形 T，算面法线 N(T) = `cross(P1-P0, P2-P0).GetSafeNormal()`
2. **对每个顶点 V**，遍历所有"以 V 为顶点的三角形"，把它们的 N(T) 相加并归一化 → 得到顶点法线

这个算法在**顶点共享**的 mesh 上工作正常（顶点 V 被多个三角形共享 → 法线被周围多个面取平均 → 光滑插值），但在**顶点完全不共享**的 mesh 上：

- 每个顶点 V 只属于一个三角形（因为我们对每个 Corner 展开 3 独立顶点）
- 步骤 2 的"求和"只有一个面 → 顶点法线 = 该三角形的面法线
- 三角形 3 个顶点的法线**完全相同**（都等于面法线）
- 光栅化插值出来的逐像素法线 = **常量面法线**
- 视觉效果 = **flat shading**（每个三角形是一个清晰的扁平面）

简言之：**`CalculateTangentsForMesh` 在"顶点完全不共享"的几何上不能产出光滑法线，等价于隐式 flat shading**。

#### 3.13.3 为什么主 mesh 没踩到这个坑

R8 主 mesh 同样是"每 Corner 展开 3 独立顶点"——但你不会在 R7/R8 主材质里看到棱面。原因：

- 主材质 R8 走 SDF + Triplanar 路径，**法线在 PS 端从 `WorldPosition` 球面反算**：`float3 dir = normalize(WorldPosition - PlanetCenter)` → 这是数学完美球面外法
- 顶点法线（KismetTangents 算出的 flat 法线）**完全不参与材质计算**——主材质的 Custom 节点根本不读 VertexNormalWS
- → flat shading 法线在主 mesh 上是"隐藏 bug，无视觉影响"

而水面 SLW shading model **必须读顶点法线**（通过 Compose Normal 节点 + VertexNormalWS）—— flat shading 法线被 SLW 直接消费 → 棱面立刻可见。

#### 3.13.4 修复模板：根据 mesh 几何选择法线策略

| 几何类型 | 推荐法线策略 | 原因 |
| --- | --- | --- |
| 顶点共享 mesh（FRenderTri 路径、Static Mesh）| KismetTangents 自动 | 共享顶点会被多面取平均，光滑插值 |
| 顶点不共享 + 球面（如 IsoSphere primal）| **直接用 `UnitCenter` 作为顶点法线** | 数学完美光滑，零棱面 |
| 顶点不共享 + 任意几何 | 显式预计算"每顶点共享圈"再求平均 | 自己实现 smooth shading |
| 顶点不共享 + 想要 flat shading（低多边形美术风格）| KismetTangents 自动 | flat shading 是想要的效果，刚好对上 |

**水面 mesh 的修复代码**（已落地于 [PlanetTopologyDebugMesh.cpp `RebuildWaterMesh_`](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp)）：

```cpp
// ? 错误：顶点不共享 mesh + KismetTangents = flat shading
// const FVector NZero = FVector::ZeroVector;
// Vertices.Add(PA); Normals.Add(NZero);   // 占位
// ...
// UKismetProceduralMeshLibrary::CalculateTangentsForMesh(
//     Vertices, Triangles, UV0, AutoNormals, AutoTangents);
// Normals = MoveTemp(AutoNormals);        // 实际是 flat 法线

// ? 正确：球面 mesh 直接用 UnitCenter 作为顶点法线
const FVector NA = WaterTopology->Cells[CA].UnitCenter;
const FVector NB = WaterTopology->Cells[CB].UnitCenter;
const FVector NC = WaterTopology->Cells[CC].UnitCenter;

Vertices.Add(PA); Normals.Add(NA);
Vertices.Add(PB); Normals.Add(NB);
Vertices.Add(PC); Normals.Add(NC);

// Tangents 留空——KismetTangents 一调用会先重算 Normals 把刚填的 UnitCenter 砸掉；
// 而 SLW shading model 通过 Normal 引脚直接消费 World Space 法线，不读 Tangent 空间。
Tangents.Reset();
```

#### 3.13.5 三条不要犯的子错

| # | 错误写法 | 原因 |
| --- | --- | --- |
| ① | "为了简单，先填 ZeroVector，最后调 KismetTangents 覆盖"——但 mesh 是顶点不共享 | KismetTangents 会把 ZeroVector 替换成**面法线**，但因为顶点不共享 → flat shading（本节根因）|
| ② | 既填 UnitCenter 又调 KismetTangents | KismetTangents 会**覆盖**你刚填的 UnitCenter 法线（参数是 `out OutNormals`，整组重写），白填了 |
| ③ | 试图通过提高 SubdivisionLevel（sub=4/5/6）来"让棱面变小看不出来" | 棱面数量增加但仍存在；性能上升；**根因没解决**——正确做法是改法线策略 |

#### 3.13.6 通用规则

> 凡是 ProceduralMesh 上"每三角形展开 3 独立顶点"的几何（**球面 IsoSphere mesh、SDF 软边 mesh、Cell 编码 mesh** 都是这个路径），**绝不能盲目调用 `KismetProceduralMeshLibrary::CalculateTangentsForMesh` 期待光滑法线**——它在该几何上等价于 flat shading。
>
> 球面情况下直接用 `UnitCenter` 作为顶点法线即可（Tangent 留空，由材质 Shading Model 决定是否需要——若需要 Tangent 空间反算法线，再单独算 World→Tangent basis）。
>
> 项目内已落地的样板：
> - [APlanetTopologyDebugMesh::RebuildWaterMesh_](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp)（水面 SLW 光滑反射球，§3.13 复盘对象，+UnitCenter）
> - [APlanetTopologyDebugMesh::Rebuild](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp)（主 mesh 同样用 +UnitCenter，**§3.14 复盘对象**）
>
> ? **本结论的修订史**：
> - 早期版本的 §3.13 曾说"主 mesh 仍走 KismetTangents 路径，因为材质 PS 端 `normalize(WorldPosition - PlanetCenter)` 反算法线"——但 R8 主材质 PS 的 dir 只用作 Triplanar 投影方向、**未覆盖 Normal 引脚**，Lit 走的仍是顶点法线。该说法已修正为"主 mesh 也应取 +UnitCenter、不调 KismetTangents"（§3.14）。
> - 早期版本的 §3.14 / [SphereTopologyReference.md §11](../Docs/SphereTopologyReference.md) 曾说"主 mesh 顶点法线应取 -UnitCenter（朝球心）以匹配 UE5 face_normal_LH"——该结论已于 R8 阶段被用户实测 sign 翻转复现证伪（朝内法线令整球漆黑）。现已修订为"+UnitCenter、朝外"，详见 [SphereTopologyReference.md §11.4](../Docs/SphereTopologyReference.md#114-关联踩坑历史与文档修订) 修订史。

---

### 3.14 ? Flat 顶点法线在 PS 反算法线材质上的"阴影边界尖刺锯齿"（§3.13 的兄弟坑）

#### 3.14.1 现象

主 mesh `APlanetTopologyDebugMesh::Rebuild` 走 §3.13 复盘的"每 Corner 展开 3 独立顶点 + KismetTangents 自动法线 = 隐式 flat shading"路径。早期版本认为这"无视觉影响"——因为主材质 PS 端用 `normalize(WorldPos - PlanetCenter)` 反算球面法线，**着色阶段不消费顶点法线**。

但实际 PIE 测试显示：

- **球面着色本身光滑**（PS 反算法线工作正常）?
- **昼夜分割线（光照明暗过渡边界）沿 mesh 三角形边缘呈现锯齿状阶梯**——肉眼可清晰数出 sub=3 的 1280 个三角形面 ?
- **远距离投射阴影**（CSM）边界同样呈三角棱面阶梯 ?
- 调高 `WaterSurfaceOffset` 后水面遮挡了部分锯齿 → 误判为 Z-fighting，但**真实根因不在水面**

#### 3.14.2 根因：阴影 / N·L 判定不走材质 PS

UE5 渲染管线中**消费顶点法线**的路径远多于材质 PS：

| 路径 | 是否消费 VertexNormal | 是否走材质 PS 反算 |
| --- | --- | --- |
| Base pass 着色 | ? 默认；但材质可在 PS 端覆盖 Normal 引脚 | ? |
| Shadow caster pass（DepthOnly + 三角形 face cull）| ?（VS 阶段算 dot(N, LightDir) < 0 的三角形不投影）| ? 完全不走 PS |
| CSM 接收阴影 / Lit cull | ? | ? |
| **N·L back-face self-shadow**（昼夜分割线核心）| ? | ? |
| Lumen 反射几何代理 | ? | ? |
| Distance Field 阴影 | ? | ? |

主材质 PS 反算 Normal 只能解决**第 1 项**——剩下的 5 条全部用顶点法线，**flat 顶点法线在这些路径上原汁原味呈现 1280 个三角面**。

具体到昼夜分割线：UE 在 VS 阶段对每个三角形算 `dot(face_normal, LightDir)`，背向光的三角形整体被 N·L self-shadow 抑制（置 0 或近似 0）—— flat 法线 = 三角形 3 顶点共享面法线 = **整个三角形要么受光要么不受光**，相邻三角形的"二选一"差异在分割线两侧形成棱面阶梯。这就是肉眼看到的 sub=3 三角形边缘锯齿。

#### 3.14.3 为什么 PS 反算法线无法救济

很自然的反应是："那我在 PS 里覆盖 Normal 引脚，把 flat 顶点法线替换成 PS 反算的 dir 法线不就好了吗？"——**不行**。

Material Editor 的 Normal 引脚**只影响 base pass 的反射 / 漫反射**。Shadow caster pass 在 UE 渲染管线里是**独立 pass**，由引擎在 cull stage 直接拿 vertex buffer 的 Normal 字段做几何剔除——**这个 pass 不读材质 Normal 引脚**（甚至材质实例化都不在 shadow pass 里跑）。

所以即使你写了完美的 PS 反算法线，shadow / N·L back-face culling 仍然按 vertex buffer 里的 flat 法线工作。**根治办法只有一个：在 vertex buffer 里写入光滑顶点法线**。

#### 3.14.4 修复模板：在 cpp 端直接写 +UnitCenter

按 [SphereTopologyReference.md §11](../Docs/SphereTopologyReference.md)（已修订）约定：

| 材质 Shading Model | 顶点法线方向 | 理由 |
| --- | --- | --- |
| Default Lit / 主材质（球外渲染）| **`+UnitCenter`**（朝外）| `saturate(dot(+UnitCenter, LightDir))` 在朝光半球 > 0 → Lit 正确受光；顶点间法线插值平滑 |
| Single Layer Water（球外渲染）| **`+UnitCenter`**（朝外）| SLW 期待"朝外法线 = 入射光反射方向"，与 Default Lit **同向** |
| Unlit | 任意 / 留空 | 不消费法线 |

? **早期版本的错误说法**：该表曾写"Default Lit 用 -UnitCenter（朝球心）"——该结论被 R8 阶段用户实测 sign 翻转复现证伪（-UnitCenter 令整球漆黑）。详见 [SphereTopologyReference.md §11.4](../Docs/SphereTopologyReference.md#114-关联踩坑历史与文档修订)。**主 mesh 和水面 mesh 均取 +UnitCenter**。

**主 mesh 修复代码**（已落地于 [PlanetTopologyDebugMesh.cpp `Rebuild`](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp)）：

```cpp
// ? 错误：占位 + KismetTangents 自动覆盖 = flat shading
// const FVector NA = FVector::ZeroVector;
// const FVector NB = FVector::ZeroVector;
// const FVector NC = FVector::ZeroVector;
// ...
// UKismetProceduralMeshLibrary::CalculateTangentsForMesh(...);  // 覆盖成 flat 面法线

// ? 也错误：-UnitCenter（朝球心）会让 dot(N, L) ≤ 0 全像素 → 整球漆黑
// const FVector NA = -Topology->Cells[CA].UnitCenter;

// ? 正确：每个顶点（位置 = Cell 中心）的光滑顶点法线 = +UnitCenter（朝外）
const FVector NA = Topology->Cells[CA].UnitCenter;
const FVector NB = Topology->Cells[CB].UnitCenter;
const FVector NC = Topology->Cells[CC].UnitCenter;

// Tangents 留空——主材质 PS 端不消费 Tangent 空间法线
// 不调 KismetTangents（避免覆盖刚填的法线，且本路径独立顶点上等价 flat shading）
TArray<FProcMeshTangent> AutoTangents;
Tangents = MoveTemp(AutoTangents);
```

#### 3.14.5 二分诊断流程：识别"是 PS 反算 OK 但顶点法线 flat"

当遇到"主体表面平滑但**光照 / 阴影边界**呈三角阶梯"时：

| 测试 | 现象 | 结论 |
| --- | --- | --- |
| 旋转 mesh 看锯齿是否跟着转 | 跟着 mesh 几何走 | 阴影边界跟 vertex buffer 法线 → **顶点法线 flat** |
| Buffer Visualization → World Normal | 球面显示 1280 个色块（每三角形一种颜色）| 顶点法线 = 面法线 = flat |
| Buffer Visualization → World Normal | 球面显示连续色调（极少数缝隙）| 顶点法线 smooth ? |
| 关掉 DirectionalLight 锯齿是否消失 | 消失 | 锯齿来自 N·L self-shadow，根因在顶点法线 |

#### 3.14.6 三条不要犯的子错

| # | 错误想法 | 实际后果 |
| --- | --- | --- |
| ① | "主材质走 PS 反算法线，顶点法线 flat 不影响视觉" | 阴影边界 / shadow caster cull / Lumen 反射代理全部消费 vertex buffer 法线，PS 反算救不了它们 |
| ② | "降低 SubdivisionLevel 阴影锯齿就看不出来了" | 三角形数量减少但单个三角形面积变大，锯齿反而更显眼 |
| ③ | "在 Material 里勾掉 Cast Shadows / 关掉 DirLight CSM 来回避问题" | 治标——视觉上的"球体真实感"依赖光照细节，关掉 self-shadow 后球面变成"贴图 + 环境光"风格，跟 SLW 反射效果不匹配 |

#### 3.14.7 通用规则

> **凡是 mesh 几何的"阴影边界 / 昼夜分割线 / 接触面"上看到沿三角形棱面的锯齿** —— 直接定位到顶点法线为 flat（无论是 KismetTangents 在独立顶点上的 flat shading，还是手填面法线的 low-poly 风格）。
>
> 主材质 PS 端的法线反算只解决 base pass 渲染，**不解决任何 self-shadow / N·L cull / Lumen 几何代理路径**——这些路径只看 vertex buffer。
>
> **球面 mesh 的修复模板**：顶点法线直接写 `+UnitCenter`（朝外）——Default Lit 和 SLW 同向，不调 KismetTangents。
>
> 项目内已落地的样板：
> - [APlanetTopologyDebugMesh::Rebuild](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp)（主 mesh，本节复盘对象，+UnitCenter）
> - [APlanetTopologyDebugMesh::RebuildWaterMesh_](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp)（水面 SLW，§3.13 复盘对象，+UnitCenter）

### 3.15 ? SLW 水体光程随 1/cos(θ) 离散化导致的水面三角锯齿（§3.14 之后的子坑）

#### 3.15.1 现象

§3.14 修复主 mesh 顶点法线为 `+UnitCenter`（光滑朝外）后，主球昼夜分割线本身已经光滑、无三角棱面锯齿。**但水面 SLW 表面在昼夜分割线附近仍呈现与 sub=3 球面 1280 个三角形精确对应的色块状锯齿**——色调过渡按三角面分段而非连续。

排查时首先排除的（不是这些）：
1. ? 水面 mesh 顶点法线 flat → 已实测水面 mesh 顶点法线也是 `+UnitCenter` 光滑
2. ? Z-fighting → 调高 `WaterSurfaceOffset` 锯齿不消失，甚至在更小 σ 下不调 Offset 也无锯齿
3. ? 绕序错误 / KismetTangents → 水面 mesh 跟主 mesh 同样的法线工作流，均无问题
4. ? 主 mesh 法线（已经是 `+UnitCenter`）

实际根因是 **SLW 物理参数在球面拓扑上的尺度问题**——与几何法线无关。

#### 3.15.2 根因：Beer-Lambert 衰减下的非线性光程敏感度

SLW 用 Beer-Lambert 衰减计算水下颜色：

```
T(λ) = exp(-(σ_a + σ_s)(λ) · L)
       L ≈ d / cos(θ)
       d = WaterSurfaceOffset（几何厚度，恒定）
       θ = 入射角，cos(θ) = dot(N_water, LightDir)
```

在昼夜分割线附近 `cos(θ) → 0` → `L → ∞`。Beer-Lambert 是 `exp(-σ·L)`，在 `σ·L ≈ 1` 数量级处颜色梯度变化最剧烈（指数函数对自变量最敏感的区段）。

**虽然水面 mesh 几何法线光滑（`+UnitCenter` 在每顶点处朝外，三角面间法线光滑插值），但 `θ(x)` 经过 `1/cos` 这个奇异点附近的非线性映射后，肉眼对斜率的感知会沿三角形面分段离散化**：几个像素之内颜色就跨越多个数量级，三角形边界两侧的细微差异被放大成可见色块。**这个锯齿来自 SLW 物理着色函数本身，不来自几何法线**。

#### 3.15.3 解法：等比例同时缩小 σ、放大 d，让 σ·d 不变但单位光程斜率变缓

| 参数 | 原值（物理标准）| R8 修订默认值 | 倍率 |
| --- | --- | --- | --- |
| `WaterSurfaceOffset`（cm）| 0~1 | **100** | ×100 |
| `Scattering Coefficients` | `(0.05, 0.18, 0.25)` | **`(0.0005, 0.0018, 0.0025)`** | ÷100 |
| `Absorption Coefficients` | `(0.3, 0.08, 0.04)` | **`(0.003, 0.0008, 0.0004)`** | ÷100 |

**等比例缩放为什么有效**：
- 光学厚度积分 `(σ_a + σ_s) · d` 值近似不变 → 远处水体仍呈"清澈海蓝"色调，不破坏 SLW 物理观感
- 单位光程上的衰减 `exp(-σ·L)` 对 `L` 的局部斜率 `≈ -σ · exp(-σ·L)`，σ 缩小 100× → 斜率绝对值缩小 100×
- 当 `cos(θ)` 在三角形面间因法线插值产生小阶跃时，`L = d/cos(θ)` 的相对变化乘以缩小后的 σ → 颜色变化幅度也缩小到肉眼不可分辨级别

**单独调一个参数都不行**：
- 单独 Offset ×100 而不缩小 σ → 水体不透明、看不见海床（破坏物理观感）
- 单独 σ ÷ 100 而不放大 Offset → 水体过于稀薄、看不到水色（丢失视觉质感）
- **必须同向等比例缩放**

#### 3.15.4 排查诊断流程：识别"光程锯齿 vs 法线锯齿"

| 测试 | 现象 | 结论 |
| --- | --- | --- |
| 把水面 mesh Visibility 关掉 | 主球昼夜分割线**仍有锯齿** | 法线锯齿（§3.14） |
| 把水面 mesh Visibility 关掉 | 主球昼夜分割线**光滑** | 锯齿来自水面层 |
| 锯齿仅在水面层、且仅在昼夜分割线附近 | cos(θ) → 0 区域 | 光程锯齿（**本节**）|
| 调小 σ（×0.01）+ 大幅调高 Offset（×100）锯齿消失 | 等比例缩放成功 | 锯齿确认是光程问题 |
| 调小 σ 但不调 Offset → 锯齿消失但水体太稀 | 物理观感破坏 | 必须**同时**调 Offset |

#### 3.15.5 与 §3.14 主 mesh 锯齿的并列关系

| 现象 | 根因 | 修复 | 已落地位置 |
| --- | --- | --- | --- |
| 主 mesh 阴影边界三角棱面锯齿（§3.14） | 顶点法线 flat（KismetTangents 在独立顶点上等价 flat shading）| 顶点法线手填 `+UnitCenter`、Tangents 留空、不调 KismetTangents | [APlanetTopologyDebugMesh::Rebuild](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp) |
| 水面 SLW 色块三角锯齿（**§3.15**）| 光程 `L = d/cos(θ)` 在 σ·L 较大时非线性敏感 | Offset × 100 + σ ÷ 100 = 光学厚度不变、衰减梯度变缓 | [PlanetTopologyDebugMesh.h `WaterSurfaceOffset` 默认 100](../Source/TerraCivilization/Public/Render/PlanetTopologyDebugMesh.h) + 用户在 `M_WaterShell` 材质中手填新 σ 值 |

**两个坑独立**：前者是 vertex buffer 法线问题（修复后水面色块仍在），后者是 SLW 物理参数尺度问题（修复后主 mesh 锯齿仍在）。R8 阶段两者都已落地默认参数化解。

#### 3.15.6 通用规则

> **凡是 mesh 几何法线已经光滑（已修复 §3.14），但材质着色仍在阴影 / 光照 / 散射边界上呈三角面锯齿** —— 锯齿来自材质内部的非线性着色函数（如 Beer-Lambert `exp(-σ·d/cos(θ))`、SSS、Lumen IS），与 mesh 几何无关。
>
> **修法是参数缩放，不是几何细分**——把非线性函数中的关键尺度参数缩小，同时把另一个反向尺度参数放大同样倍数，保持物理观感的同时把斜率压平。
>
> SLW 专属修法：**Offset × N + (Scattering, Absorption) ÷ N** 等比例缩放，N = 100 在 sub=3 球面上验证有效；如未来主 mesh 提升细分级别（sub=4/5），N 可适当下调。

---

### 3.16 ? 不要在 `BeginDestroy` 里手动 `Reset()` TUniquePtr 字段（T2 关闭编辑器经典踩坑）

#### 3.16.1 现象

关闭 Editor 时随机崩溃（PIE 不一定触发，纯 Editor 退出更明显），调用栈：

```
APlanetTessellatedMesh::BeginDestroy()
  → FMeshDisplacementBuilder::~FMeshDisplacementBuilder()
    → ~TMap<uint64, int32>
    → ~TSparseArray< TTuple<uint64, int> >
    → ReallocTo  ← EXCEPTION_ACCESS_VIOLATION
```

#### 3.16.2 根因

`UObject::BeginDestroy()` 是 GC 流程的**早期回调**——对象在引擎逻辑层已被标记销毁、callback 已派发，但 **C++ 内存还活着、UPROPERTY 引用仍有效、后续阶段（FinishDestroy / 真正的 dtor）尚未执行**。

我在 `BeginDestroy` 中调用 `Displacement.Reset() / MeshTopology.Reset() / CellTopology.Reset()`，等于**在 GC 早期阶段提前释放非 UObject 字段**。Editor 关闭路径会触发：

1. 非常多的 actor `BeginDestroy` 在同一帧链式触发；
2. 期间 GC 内部数据结构（用 `TSparseArray<TTuple<...>>` 实现的 `TSet` / `TMap` 池）会做 `ReallocTo` 重定位；
3. 我已经手动析构过的 `TMap<uint64, int32> CoarseTriIdByCellTriple` 内存被释放了一次；
4. 之后编译器生成的 `~APlanetTessellatedMesh()` 又试图析构同一份 `TUniquePtr` —— 此时 `TUniquePtr` 内部 ptr 已是 nullptr 没事，但 builder 内的 TMap 元数据已被破坏 → `~TSparseArray::ReallocTo` 试图 free 一块脏内存 → AV。

#### 3.16.3 正确做法

> **TUniquePtr 等非 UObject 字段交给编译器生成的 actor dtor 自动释放，不要 override `BeginDestroy` 来手动 Reset。**

```cpp
// 头文件：根本不声明 BeginDestroy。
class TERRACIVILIZATION_API APlanetTessellatedMesh : public AActor {
    // 只显式声明析构和 FVTableHelper 构造（前向声明类型 + UHT 联调所需）。
    APlanetTessellatedMesh();
    APlanetTessellatedMesh(FVTableHelper& Helper);
    virtual ~APlanetTessellatedMesh();
};

// .cpp 中：
APlanetTessellatedMesh::~APlanetTessellatedMesh() = default;        // ★ 自动展开 TUniquePtr 析构
APlanetTessellatedMesh::APlanetTessellatedMesh(FVTableHelper& H) : Super(H) {}
// 完全不要 override BeginDestroy()。
```

参考样板：[`PlanetTopologyDebugMesh.cpp`](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp) 中 R8 阶段的 actor 也是这种写法——它持有 `TUniquePtr<FSphereTopology> / TUniquePtr<FWorldGenerator>` 等多个非 UObject 字段，但**整个文件没有 override `BeginDestroy`**，从未崩溃。

#### 3.16.4 例外：什么时候才需要 override `BeginDestroy`？

仅当 actor 持有的资源**必须比真正的 dtor 更早释放**时，才考虑 override：

| 场景 | 是否要 override BeginDestroy |
| --- | --- |
| `TUniquePtr<非 UObject 类>`（如 `FSphereTopology` / `FMeshDisplacementBuilder`）| ? 不要——编译器生成的 dtor 自动释放，时机正确 |
| 取消 `FWorldDelegates::OnPostWorldCleanup` 等订阅 | ? 可以——但更推荐放在 `~Actor()` 中（`PlanetTopologyDebugMesh` 写法）|
| 释放 RHI 资源 / GPU buffer | ? 必须——`BeginDestroy` 是 GPU 资源解绑的官方时机 |
| 通知其他 UObject 解除引用 | ? 可以——此时其他 UObject 仍有效 |

#### 3.16.5 验证清单

- [ ] **grep 检查**：项目中所有 `override.*BeginDestroy` 都不能 `Reset()` TUniquePtr（应当是 RHI / 委托取消订阅）。
- [ ] **关闭 Editor 测试**：放入 actor → 编辑器中拖动属性 → 关闭 Editor，观察是否有 ACCESS_VIOLATION 弹窗。
- [ ] **PIE 进出测试**：与 §3.10 / §3.11 配合，确认进出 PIE 不丢材质、不崩溃。

#### 3.16.6 一句话速记

> **BeginDestroy ≠ C++ 析构函数**——前者是 GC 早期回调（对象逻辑死、内存还活），后者是真正释放内存。**TUniquePtr 字段要交给 C++ 析构函数处理，不要在 GC 早期阶段抢着 Reset。**

---

### 3.17 ? R8 PS 端的 c0/c1/c2 是**三角形级常量**——T3 共享顶点路径会破坏这一假设导致整球破碎（T3 经典踩坑）

> **修订历史**：2026-06-30 第一版诊断把根因归为"UV0 漏写 → c2 默认为 cell 0 → PS 错选"。修复后视觉症状不变 → 重新推演发现真正根因是**共享顶点路径下，邻接 mesh 三角形对同一个共享顶点写入了不同的 (c0, c1, c2) 代表组**，光栅化插值后 PS 端解出毫无意义的 cell ID。"UV0 漏写"是早期版本的附属症状之一，不是根因。

#### 3.17.1 现象

T3 拖入 [APlanetTessellatedMesh](../Source/TerraCivilization/Public/Render/PlanetTessellatedMesh.h) actor、挂上同一份 R8 合规材质 `M_TopologyDebug_R8`，与右侧 R8 的 [APlanetTopologyDebugMesh](../Source/TerraCivilization/Public/Render/PlanetTopologyDebugMesh.h) 并排：

- **R8 actor**：每个 hex/pent 内部颜色一致，cell 边界整齐（17 配方 placeholder 视觉）
- **T3 actor**：依稀可见 hex 形状（颜色块尺寸与 R8 接近），但**每个 hex 内部按 mesh 三角形粒度破碎**——sub=4 的每个三角形都呈现一块独立颜色，相邻三角形之间颜色完全不同；Output Log `? R8 compliance (all 17 inputs present & connected)`，无 Error

#### 3.17.2 根因（订正版）

R8 PS 端的 17-input Custom 节点（详见 [SphericalSDFTerrainDesign.md §3.2.2](SphericalSDFTerrainDesign.md) / [PlanetTopologyDebugMesh.cpp L488~L545](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp)）的 **3 个候选 cell 编码布局**是：

```
UV0.xy = (HiC, LoC)   ← Cell C 的 CellId 拆分（R8 把 UV0 借给 c2）
UV1.xy = (HiA, LoA)   ← Cell A 的 CellId 拆分
UV2.xy = (HiB, LoB)   ← Cell B 的 CellId 拆分
UV3.xy = (OneHot.x, OneHot.y) — R3 重心权重，R4+ 由 PS 自算，仅作 hint
```

**R8 PS 端的核心隐含假设（从未明文写出但事实存在）**：

> **每个渲染三角形内部，光栅化插值出的 (c0, c1, c2) 必须是常量**——也就是说三角形的 3 个顶点必须**写入完全相同的一组 (HiA,LoA, HiB,LoB, HiC,LoC)**，光栅化后 UV0/UV1/UV2 在三角形内部是常量，PS 端解出的 c0/c1/c2 仍然是同一组（A, B, C），与该三角形对应的 primal 三角形 1:1 对应。

R8 通过"独立顶点 + 三个顶点写同一组"自然满足这一假设：

```cpp
// R8：每个 Corner（primal 三角形）展开 3 独立顶点，3 个顶点写同一组
Vertices.Add(PA); UV0=(HiC,LoC); UV1=(HiA,LoA); UV2=(HiB,LoB); UV3=(1,0);  // Role 0
Vertices.Add(PB); UV0=(HiC,LoC); UV1=(HiA,LoA); UV2=(HiB,LoB); UV3=(0,1);  // Role 1
Vertices.Add(PC); UV0=(HiC,LoC); UV1=(HiA,LoA); UV2=(HiB,LoB); UV3=(0,0);  // Role 2
```

T3 第一版基于"共享顶点节省内存 / 自然光滑"的朴素优化，把渲染顶点缓冲设为 `MeshTopology->PrimalVertsUnit`（共享顶点）。**但共享顶点路径在物理上不可能满足 R8 PS 端的 c 三角形级常量假设**：

- 一个共享 mesh 顶点 V 同时被 5/6 个邻接 mesh 三角形使用
- 每个邻接三角形需要 V 写入"该三角形所属粗 Cell 三角形的 (cA, cB, cC)"——但**5/6 个邻接三角形对应的粗 Cell 三角形通常不同**，需要的 (cA, cB, cC) 也不同
- V 只能保留**一组** (c0, c1, c2)，T3 第一版用 `FindRepresentativeCoarseTri_` 挑"权重最大的代表组"——这只对 V 自身位置正确，**对它周围的三角形则是错的**
- 当一个三角形 T 的 3 个顶点 V0/V1/V2 各自挑了不同的代表组时，光栅化器把 3 组 (HiA,LoA) 做线性插值——**插值后的 (Hi, Lo) 在 PS 端 round 出来是个完全无意义的 cell ID**
- PS 端球面 Voronoi `argmax(dot(dir, V_i))` 在 3 个**乱七八糟的 cell 候选**间仲裁，最终选到的 cell 不属于该位置 → 该 fragment 显示错误的 layer 颜色 → 整球按 mesh 三角形粒度破碎

#### 3.17.3 诊断流程

| 步骤 | 工具 | 看什么 | 期望 |
| --- | --- | --- | --- |
| 1 | grep cpp | 顶点缓冲长度 = `NumMeshVerts` 还是 `NumMeshTris*3` | **必须是 `NumMeshTris*3`**（独立顶点） |
| 2 | grep cpp | `Triangles.Add(Tri.X/.Y/.Z)`（共享索引）vs `Triangles.Add(BaseIdx+0/1/2)`（独立索引）| 必须是 `BaseIdx+i` |
| 3 | 对照 R8 cpp | `for CornerIdx ... Vertices.Add(PA/PB/PC)` 的"3 独立顶点"模式 | T3 必须采用同款"每三角形 3 顶点"模式 |
| 4 | 顶点缓冲 dump | 同一三角形 3 个顶点的 UV1 值 | **必须完全相等**（HiA, LoA 三顶点同值） |

#### 3.17.4 修复

T3 [PlanetTessellatedMesh.cpp::RebuildTerrainMesh_](../Source/TerraCivilization/Private/Render/PlanetTessellatedMesh.cpp) 必须切换到"独立顶点 + 三角形级 c 一致"路径：

1. 顶点缓冲长度 = `NumMeshTris * 3`
2. 对每个 mesh 渲染三角形 T：
   - 通过 `MeshTopology->PrimalTriTreeNodes[T]` 沿 `Father` 爬升 `MeshSub - CellSub` 步，拿到所属粗 Cell 三角形的 `(cA, cB, cC)`
   - 把 `(cA, cB, cC)` 作为该三角形 3 个独立顶点的**共同**写入值
3. UV3 用 R8 同款 OneHot `(1,0)/(0,1)/(0,0)` 三角色循环
4. 顶点位置 / 法线仍按 mesh 顶点查 `PrimalVertsUnit + VertexElevCM` 取值——保证邻接三角形在共享 mesh 顶点处的位置 / 法线**完全相等** → 视觉上仍是光滑共享顶点 mesh，无 flat shading 锯齿

> "顶点位置共享 + 顶点 UV 不共享"是 R8 一贯的设计——T 阶段沿用即可。共享顶点带来的 "8KB → 3KB 内存节省" 在球面渲染量级下完全不重要，但 PS 端的 c 三角形级常量假设必须满足。

#### 3.17.5 通用规则

> **任何复用 R8 材质的渲染路径，渲染三角形必须采用"独立顶点 + 同一组 (c0, c1, c2) 写入 3 个顶点"模式。**
> **共享顶点（一个 mesh 顶点被多个三角形共享同一组 UV）会破坏 R8 PS 端的 c 三角形级常量假设，导致 PS 仲裁出错乱的 cell ID。**

附属规则：

- UV1 / UV2 / UV0 的 (Hi, Lo) 拆分编码必须 **3 个候选 cell 全部写**——任何一个漏写 / 全 0 都会让 PS 的球面 Voronoi 仲裁默认把该位置当成 cell 0
- 顶点位置 / 法线**应当**仍按"共享 mesh 顶点位置"取值（即三角形 T 的 3 个独立 UV 顶点的 Position 仍取 `PrimalVertsUnit[Tri.X/Y/Z] * (R + ElevCM)`），保证视觉光滑
- 三角索引必须用 `BaseIdx + 0/1/2` 顺序模式（与 R8 一致），不能用 `MeshTopology->PrimalTris[T].X/Y/Z`（那是共享索引）

#### 3.17.6 一句话速记

> **R8 PS 端 c0/c1/c2 = 三角形级常量**——三角形 3 顶点必须写同一组 c，光栅化插值后才解得回正确的 cell。共享顶点必然破坏这一假设，必须用"独立顶点 + 同 c 三写"。位置 / 法线仍可共享 mesh 顶点取值，所以视觉无锯齿。

---

## 4. 文档维护规范

### 4.1 跨阶段同步项检查表

每次推进版本号（R<N-1> → R<N>）必须同步：

- [ ] 主稿 §0 摘要里的"当前阶段"
- [ ] 主稿 §11 Roadmap 表的状态列、描述列
- [ ] 主稿 §14.6 末尾的"跳转入口"
- [ ] 主稿 §14.7（如涉及球面重心坐标）
- [ ] R<N-1> 详稿 §7「路径预告表」修正为 R<N> 实际方案
- [ ] §12 风险表（如有新风险）
- [ ] §14 GPU 资源表（如有新 LUT/纹理）

### 4.2 grep 全文扫描验证（强制）

每次主稿改完，**必须** grep 关键术语扫描：

```
# 例子：R3 → R4 切换后必须扫
argmax(λ)            # R3 旧路径
CellCenterLUT        # 早期 LUT 命名
Sharpen(λ)           # R5 旧路径 A
per-cell noise on δ  # R6 旧路径
```

确认所有过时表述都已修正或在合适的"历史叙述语境"中保留。

### 4.3 命名一致性

| 项 | cpp | 文档 | 错误用法 |
| --- | --- | --- | --- |
| LUT 命名 | `CellAttrLUT` / `CellDirLUT` | 同 cpp | ~~CellCenterLUT~~ |
| 像素格式 | `PF_A32B32G32R32F` | `R32G32B32A32_FLOAT` | — |
| 参数名 | `TEXT("EdgeWidth")` | `EdgeWidth` | 大小写不一致 |
| 阶段编号 | R<N> | R<N> | 不要用 v<N> / step <N> |

### 4.4 用户踩坑立即沉淀

凡用户实际踩过的坑：
1. 当期详稿 §6 排错表追加一行（症状 / 根因 / 修复）
2. 主稿 §11.x 加 1~2 行警示（如 R3 阶段把 PMC 槽位陷阱写入 §11.2.2）
3. 必要时加红框警示：`> ? 关键陷阱（必读）：...`

---

## 5. 调试方法论

### 5.1 三层诊断法

```
┌──────────────────────────┐
│ Layer 1: cpp 层           │
│   ? 反射诊断打印是否齐全？  │
│   ? LUT 自检日志正常？     │
│   ? Rebuild 总结行 OK？   │
└──────────────────────────┘
              │
              ▼
┌──────────────────────────┐
│ Layer 2: 材质层           │
│   ? Stats 面板有红错？     │
│   ? Custom 节点 Code OK？ │
│   ? Texture Object 位置 A 槽位有值？ │
└──────────────────────────┘
              │
              ▼
┌──────────────────────────┐
│ Layer 3: 运行时层          │
│   ? Lit / Unlit 模式分别看 │
│   ? Unlit 直接显示 BaseColor，是排查纹理 fallback 的金标准 │
└──────────────────────────┘
```

### 5.2 "假绿灯"陷阱识别

**警钟**：R7-compliance ? + Stats ? + Output Log ? ≠ 视觉效果正确。

当所有日志都通过但视觉不对：
1. 第一怀疑：材质槽位挂错（PMC 通用槽 vs cpp UPROPERTY 槽）
2. 第二怀疑：Texture Object Parameter 位置 A 空槽 fallback
3. 第三怀疑：Sampler Type 与纹理实际格式不匹配
4. 第四怀疑：HLSL 在 RT 阶段不合法（如 `Sample` 在 ClosestHit）

### 5.3 用户截图必读

用户提供日志截图 / 渲染截图时，**必须** `read_image`：
- R6 接缝问题靠截图诊断
- R7 纯白问题靠 Lit + Unlit 双模式截图诊断

**禁止**只读文字日志就臆断结论。

### 5.4 着色器编译错误的查找通道

| 错误类型 | 日志 channel | 备注 |
| --- | --- | --- |
| cpp 反射诊断 | `LogPlanetTopologyDebugMesh` | 自定义 channel |
| HLSL 编译错误 | `LogShaderCompilers` / `LogShaders` / `LogMaterial` | 必须在 Output Log 顶栏 Filters → All Categories |
| 材质静默 fallback | 无日志 | 用 Unlit 模式视觉判别 |

---

## 6. 跨期同构性约束（贯穿 R1~R11，最重要的隐性约束）

> 当前 PMC 调试 mesh（R1~R8）与 T 阶段+ 自研球面网格生产路线必须共用同一套着色公式。

实现守则：
- 所有几何 / 着色公式同时适配 PMC（顺点流 UV 还原 c0/c1/c2）与自研网格（cpp 端预计算"权重"灬顶点）
- LUT 纹理（CellAttrLUT / CellDirLUT 与后续 4 通道材质 LUT）在 T 阶段及以后仍生效、表示语义一致
- 所有 HLSL Custom Code 在 R8 → T 阶段 mesh 切换时**应当零改动复用**（PS 收到的 3 个权重的具体语义会随 D3 决策演进而变化——T 阶段拍板为 dot 线性插值；如未来升级到 acos 软权重或高次方插值，需同步在 [TessellatedMeshDesign.md](TessellatedMeshDesign.md) §1.3 D3 行更新）
- 详稿每章 §7 必须明确"与上下游阶段的关系"（包括不再引用的 R11 PTG 路线需明确标记为"已废弃"）

具体到当前阶段：

| 阶段 | mesh | 三个权重来源 | 区别 |
| --- | --- | --- | --- |
| R3~R8 | IsoSphere primal | 顶点流 UV 还原 c0/c1/c2 + 硬件重心坐标 | 无 Elevation 位移 |
| T 阶段+ | 自研 `MeshTopology = FSphereTopology(4)` 独立实例 | cpp 端按 mesh 顶点 → 父 CellTopology 三角形 → 三 Cell `dot` 线性插值 + 多重三角形算术平均；详见 [TessellatedMeshDesign.md §3](TessellatedMeshDesign.md) | 含径向位移；顶点法线直取 `+UnitCenter`、Tangent 留空、不调 KismetTangents |
| ~~R11 PTG~~ | ~~PTG 高细分球皮~~ | ~~GPU FindNearestCell + acos~~ | ? 已废弃（T 阶段自研网格取代）|
---

## 7. 协作守则速查卡

| 场景 | 行为 |
| --- | --- |
| 用户提需求 | 先澄清、列方案、等拍板 |
| 主稿 vs 详稿 | 主稿简述 + 跳转，详稿独立成文 |
| 写跨模块字段引用 | **先 grep / 读 .h 确认真实字段名**，禁止凭直觉/摘要写（详见 §1.4.0） |
| 改 cpp | 立即编译验证、立即更新 Roadmap 状态 |
| 验证材质 | 反射诊断打印 Inputs / Code / 合规性 |
| 处理 Texture | 位置 A 必填、SampleLevel、Sampler Type 对齐 |
| 写 HLSL | 不嵌套函数、scoped block 内联 |
| 用户踩坑 | 立即沉淀到详稿 §6 排错表 |
| 编辑后验证 | grep 而非 codebase_search |
| 读 uasset | 靠 cpp 反射、不靠二进制读取 |
| 用户截图 | 必须 read_image |
| 多文件读取 | 并行 |
| 多处改动同文件 | multi_replace |
| **删除文件 / 资产 / 过时脚本** | **禁止主动调用 `Remove-Item`/`rm`/`del`**；改写为废弃 stub 或保留原状，回复**末尾**统一列出"待用户手动清理清单"（详见 §2.5）|

---

## 8. 应做 / 不可做 全表

### 8.1 ? 应做

- 列候选方案让用户拍板
- 主稿简述 + 详稿落地
- **写跨模块字段引用前先打开真实头文件 / grep 字段定义核对拼写**（详见 §1.4.0）
- 反射诊断升级到当期 Inputs 数
- LUT 自检日志（前 16 cell）
- Roadmap 状态联动
- 详稿 §6 排错表立即沉淀踩坑
- 多文件 / 多关键词并行调用工具
- HLSL 内联 scoped block + `SampleLevel(..., 0)`
- Texture Object Parameter 位置 A 必填
- 改完用 grep 验证
- 用户截图必 read_image

### 8.2 ? 不可做

- 用户没拍板就动手
- **凭直觉 / 设计稿摘要 / 记忆写字段名（必须以源码 .h 为准；高频坑：`NeighborCellIds` 误写为 `Neighbors`、`EdgeIds` 误写为 `Edges`）**
- 主稿塞详细 HLSL / cpp（应放详稿）
- 直接 read_file 二进制 .uasset
- `replace_in_file` 把 `\t` 转义成 `\\t`
- 在 UE Custom Code 内定义函数（`float Foo() {...}`）
- 在 UE Custom Code 内用 `.Sample(...)`（应 `SampleLevel`）
- 仅在主材质 Parameter Defaults 面板挂 Texture2DArray（应在材质图节点本身的 Details > Texture 槽）
- 把 Material 挂到 PMC 通用槽 Element 0（应挂 `PlanetTopology > Material`）
- cpp 编辑后用 codebase_search / view_code_item 验证（有索引延迟）
- 漏改 §11 Roadmap 状态
- 擅自重命名 LUT / 参数（必须配套全文 grep 替换）
- 顺序调用本可并行的工具
- **凭"2D 逆时针旋转 90°"直觉写球面东向公式（UE5 是左手系 + Z up，"自西向东" = +X→?Y，故 East = `(U.Y, -U.X, 0)` 而非 `(-U.Y, U.X, 0)`；详见 §3.7）**
- **新增 GameplayTag ini 时直接手放 `Config/Tags/Foo.ini` 期望重启即可（无效——必须经编辑器内 "GameplayTags → Manage Gameplay Tags → Add New Gameplay Tag → Source 输入 Foo.ini" 一次让它被注册为合法 Source；详见 §3.8）**

---

## 9. 阶段性能预算参考

为 R8+ 提供性能直觉（基于 sub=3, 642 cells, 1080p 视口）：

| 阶段 | 增量 ms | 主要开销 |
| --- | --- | --- |
| R3 | ~0.05 | 3 次 LUT.Load |
| R4 | ~0.1 | + 3 次 dot + argmax |
| R5 | ~0.3 | + 3 次 acos + 3 次 smoothstep + 2 次额外 LUT.Load + 2 次额外 hash |
| R6 | ~0.5 | + 3 次 8-corner value noise |
| R7 | ~1.5 | + 9 次 Texture2DArray.SampleLevel |

R8+ 要预算下一期的 ms 增量、显存占用、Sampler 资源占用。

---

## 10. 后续阶段（R8～R11）的常驻提醒

### R8（参数化 Tint + 水面层）
- 将单一 `TerrainAlbedoArray` 19-slice 路径升级为 3 张基础 PBR 套件（Soil/Rock/Forest Canopy）+ 4 通道材质 LUT（Index/Tint/HSV_Rough/NSpec）
- 17 种地形配方依靠在同一组基础贴图上调 Tint/HSV/Roughness/Triplanar Scale 变出
- 新增独立水面层——sub=3 简易球皮 mesh + 噪声扰动反光透光材质（R8 阶段单独验收，与地形重合不能同时看）
- 反射诊断 Inputs 从 R7 的 14 项升级到 ≈18–20 项（新增 4 LUT 与 Triplanar 参数）
- **不依赖 W4**：BaseTexIdx 仍走 R3 Knuth 哈希 placeholder

### T 阶段（TessellatedMesh、自研球面网格无 LOD，详见 [TessellatedMeshDesign.md](TessellatedMeshDesign.md)）
- 子里程碑 T1~T6 逐文件验收（与 R4 / W1 验收风格对齐；T6 = LOD，详见 [TessellatedMeshDesign.md §5.4](TessellatedMeshDesign.md)）
- 主验收 actor `APlanetTessellatedMesh` 同时持有两个独立 `FSphereTopology` 实例：逻辑层 `CellTopology`（默认 sub=3）+ 渲染层 `MeshTopology`（默认 sub=4，独立可调）
- cpp 端顺 mesh 顶点预计算：`Cells[v].CornerIds` 拿 5/6 个粗 mesh 三角形 → `FTriTreeNode->Father` 上爬 `MeshSub - CellSub` 次 → 得到粗 CellTopology 三角形 → 三 Cell `dot` 权重线性插值 → 多重三角形取算术平均
- 径向位移：`Pos = Dir·(R + H_macro)`，不进 GPU WPO
- **顶点法线直取 `+UnitCenter`、Tangent 留空、不调 KismetTangents**（径向位移不改变法线方向）
- fbm 高频细节（`H_micro`）接口预留但 T 阶段主验收不吻合，留后续子里程碑
- 拾取路线：`Hit.ImpactPoint` 径向归一化 → `FSphereTopologyQuery::FindNearestCell`（使用 `CellTopology`、不是 `MeshTopology`）

### W4（联调验收）
- R8 + T 阶段联调通过后，把 R3 Knuth 哈希 placeholder 换为 `Def->LayerIndex` + `Def->FTerrainMaterialParams` 真实查表
- SDF 端仅一行 cpp 改动 + 反射诊断参数名更新

### R9（多套 LUT）
- 加 Decor / Owner / Fog 三套独立 LUT（在 R8 4 通道 LUT 基础上扩展）
- 反射诊断 Inputs 升级到 ≈20–23 项

### T6（自研网格 LOD，原 R10 已迁至 [TessellatedMeshDesign.md §5.4](TessellatedMeshDesign.md)）
- 基于二十面体递归细分（边递归，非面递归）；LOD 0 = MeshSub = CellSub（硬约束、不可反）
- 按需 new/delete `FTriTreeNode` 子树，不维护全量高 sub MeshTopology；LOD 切换仅局部 patch不重建整 mesh
- 高 LOD 顶点位置拍板倾向 CPU 端 fbm（与 T2/T3 已有路径一致），与 R8 材质 Height 采样二选一的决策留 T6 落地期
- 仍走 [T3 §3.6](T3_TerrainMeshRender.md) 的"独立顶点 + 三角形 3 顶点写同一组 (cA, cB, cC)"模式，R8 PS 端零改动

### R10（已迁出）
- 原计划的 "R10 自研网格 LOD" 已于 2026-06-30 拍板迁至 T 阶段主稿为 T6。概念上 LOD 是几何域职责不是 SDF 著色职责。

### R11（高亮描边）
- 接入 §15 高亮描边带 + 选中 / 鼠标悬停的 LUT 联动
- hex 边发光描边、选中即时反馈

### 废弃路线提醒
- 原计划的 "R11 PTG 路线"（渲染从 IsoSphere 切到 PTG 高细分球皮 + GPU FindNearestCell）**已废弃**——T 阶段自研球面网格已取代该路线的全部职责。SDF 主稿 §14 仅作历史档案保留（其中 §14.7 球面重心坐标证明仍有效，被 §16.3 复用）。在 T 阶段以后，可以一次性删除 `ProceduralTerrainGenerator` 插件依赖与 `PlanetBinder` 中的 PTG 桥接代码。

---

## 附录 A：本文档引用的核心文件

- 主稿：[SphericalSDFTerrainDesign.md](SphericalSDFTerrainDesign.md)
- 阶段详稿：
  - [R2_TopologyDebugMaterial.md](R2_TopologyDebugMaterial.md)
  - [R3_CellAttrLUTMaterial.md](R3_CellAttrLUTMaterial.md)
  - [R4_VoronoiBoundary.md](R4_VoronoiBoundary.md)
  - [R5_SharpenSoftEdge.md](R5_SharpenSoftEdge.md)
  - [R6_BoundaryNoise.md](R6_BoundaryNoise.md)
  - [R7_TerrainTriplanar.md](R7_TerrainTriplanar.md)
- cpp 端：
  - [PlanetTopologyDebugMesh.h](../Source/TerraCivilization/Public/Render/PlanetTopologyDebugMesh.h)
  - [PlanetTopologyDebugMesh.cpp](../Source/TerraCivilization/Private/Render/PlanetTopologyDebugMesh.cpp)

## 附录 B：本文档归档时间线

- 创建于：R7 完成后（19 个真实地表 layer 的 Triplanar 通路验收通过）
- 下次更新：T 阶段完成后（自研球面网格与 PTG 路线废弃需補充新章节）
- 长期目标：作为 T6/R11 阶段的 "AI 协作 baseline"



---

### 3.18 Native default subobject refactor can break old Blueprints and PropertyEditor

#### Symptom

After `APlanetTessellatedMesh` was split into camera, gameplay, HISM interaction, and piece-presentation components, the failure did not appear immediately:

- The Editor remained usable after the refactor, and several PIE or standalone runs completed successfully.
- A later standalone launch crashed while loading the map. The crash stack ended in `SerializeUnversionedProperties` and `UBlueprintGeneratedClass::SerializeDefaultObject`.
- Opening the old `BP_PlanetTessellatedMesh` then crashed the Editor. A newly created Blueprint that directly inherited `APlanetTessellatedMesh` also crashed. The Editor crash was `Unhandled Exception: EXCEPTION_STACK_OVERFLOW`, with repeated `UnrealEditor_PropertyEditor` frames while the Blueprint SCS/Details view was being built.

These are two related compatibility failures, not a gameplay or input-loop recursion.

#### Root cause and delayed appearance

1. The old Blueprint asset had serialized component-template exports for removed native default subobjects, including `TerrainMeshComp`, `WaterMeshComp`, and the old `PiecePresentationManager`. After the C++ refactor removed those names from `Default__PlanetTessellatedMesh`, a fresh standalone process could no longer resolve the templates while asynchronously loading the Blueprint/map package. The subsequent null template path caused the serialization access violation.

2. The original piece-presentation split created `UTerraPiecePresentationManager` (itself a `UActorComponent`) as a visible default subobject of `UPlanetPiecePresentationComponent`. PropertyEditor recursively expanded this nested native component object graph. Host-side `VisibleAnywhere` references without `NoEditInline` also made extracted component objects available for inline actor Details expansion. This can exhaust the stack while constructing the Blueprint editor even for a new Blueprint, because the issue belongs to the native class reflection tree rather than to the old asset alone.

The delayed symptom is expected in UE:

- Existing editor sessions and some PIE paths can operate on already loaded/duplicated class defaults and may not re-read the stale Blueprint exports from disk.
- A standalone process starts with a fresh package load, so it exercises the missing-template path deterministically when the affected map is loaded.
- Opening a Blueprint immediately constructs its CDO/SCS preview and Details tree, so the PropertyEditor recursion becomes visible independently of whether gameplay has begun.

#### Fix

1. Do not create an `UActorComponent` as a visible default subobject of another `UActorComponent`. `UPlanetPiecePresentationComponent` now creates `UTerraPiecePresentationManager` lazily with `NewObject` only when runtime presentation is required, and keeps the pointer transient and hidden from Details.

2. Keep actor-owned native components as `VisibleAnywhere` references so the Blueprint Components panel can display their Details, but add `meta = (NoEditInline)` to prevent recursive inline expansion from the actor Details panel. Retain Blueprint read access and edit camera/gameplay/HISM/piece-presentation settings by selecting the corresponding native component in the Blueprint Components panel.

3. Do not populate runtime HISM/Gameplay state in a Blueprint `EditorPreview` world. `APlanetTessellatedMesh::OnConstruction` returns for templates and `EWorldType::EditorPreview`; normal level-editor construction and `BeginPlay` retain their rebuild paths.

4. Treat the old `BP_PlanetTessellatedMesh` as an incompatible asset after removed default-subobject names. Create a clean replacement Blueprint from the repaired C++ class, reapply only supported HISM and component settings, replace level instances, reconnect `PlanetBinder.TessellatedMeshRef`, save the affected maps/external actors, and only then retire the old asset. Do not restore ProceduralMesh/SDF components merely to keep obsolete data alive.

#### Verification checklist

- Build with the Editor closed and require `Result: Succeeded`.
- Reset `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` only as a diagnostic control; it removes saved Details expansion state but cannot repair either C++ reflection recursion or stale Blueprint exports.
- Create and open a fresh Blueprint directly derived from `APlanetTessellatedMesh`; it must open without `EXCEPTION_STACK_OVERFLOW`.
- Confirm the Components panel still contains Camera, Gameplay, HISM Interaction, and Piece Presentation components, and that their settings remain editable there.
- Launch the migrated map in a fresh standalone process and verify there are no `Could not find template object` messages for removed subobject names.

#### Rule

Native default-subobject names are serialized compatibility identifiers for Blueprint assets. Removing or relocating one is an asset migration, not a source-only refactor. Keep runtime-only nested helpers transient/lazy, and use `NoEditInline` when a native component reference must be visible in the Components panel but must not be recursively expanded in actor Details.

## Pitfall: Native component constructor-time owner cache can point to Blueprint CDO

### Symptom

After moving `APlanetTessellatedMesh` camera behavior into `UPlanetCameraComponent`, runtime `BP_PlanetTessellatedMesh` instances existed, `PlanetBinder.TessellatedMeshRef` was set, and `APlanetInteractionController` could initialize C3, but WSAD camera control did not move the camera.

Temporary diagnostics showed:

```text
ApplyFocusCameraState failed: World is null Host=Default__BP_PlanetTessellatedMesh_C CameraComp=PlanetCameraComponent
```

### Cause

`PlanetCameraComponent` is a native default subobject. The broken version called this from the actor constructor:

```cpp
PlanetCameraComponent = CreateDefaultSubobject<UPlanetCameraComponent>(TEXT("PlanetCameraComponent"));
PlanetCameraComponent->Initialize(this);
```

For Blueprint-derived actors, constructor-time `this` can be the class default object (`Default__BP_...`) during CDO construction/reinstancing. Caching that pointer in the component as long-lived state made runtime instances resolve their host to the CDO. `Host->GetWorld()` was then null, so `ApplyFocusCameraState` returned false.

### Fix

- Remove `UPlanetCameraComponent::Initialize(APlanetTessellatedMesh*)`.
- Remove cached host state such as `HostOverride`.
- Implement `UPlanetCameraComponent::GetHost()` as `Cast<APlanetTessellatedMesh>(GetOwner())`.
- Let `APlanetTessellatedMesh` constructor only create the default subobject; do not write an owner pointer into the component.

### Rule

A native `UActorComponent` that needs its owning actor should resolve it from `GetOwner()` at runtime. Do not cache actor-constructor `this` inside a component as persistent state, especially for Blueprint-derived actors where the constructor also runs for the CDO.
## Gameplay 编排组件化（实现更新）

- 当前 `FTerraGameplayContainer` 已不再由 `APlanetTessellatedMesh` 直接持有。
- 新结构为：
  - `APlanetTessellatedMesh` 挂载 `UPlanetGameplayComponent`
  - `UPlanetGameplayComponent` 内部持有 `TUniquePtr<FTerraGameplayContainer>`
  - `APlanetTessellatedMesh` 上旧的 Gameplay 入口仅保留兼容桥接
- 受影响的真实实现位置：
  - `Source/TerraCivilization/Public/Render/PlanetGameplayComponent.h`
  - `Source/TerraCivilization/Private/Render/PlanetGameplayComponent.cpp`
- 已迁移的职责包括：
  - Gameplay rebuild
  - Cell click 编排
  - Tab 棋子循环导航
  - Undo
  - NPC MCP validated action 执行桥
  - 当前阵营/脏格高亮刷新
  - G1 调试棋子缓存与绘制

## HISM 交互与高亮组件化（实现更新）

- 当前 HISM 命中解析与高亮逻辑已不再由 `APlanetTessellatedMesh` 直接承载。
- 新结构为：
  - `APlanetTessellatedMesh` 挂载 `UPlanetHISMInteractionComponent`
  - `UPlanetHISMInteractionComponent` 负责：
    - HISM hit -> CellId 解析
    - hover / click 交互桥接
    - `PerInstanceCustomData` 高亮写入
    - capture preview cell 刷新
    - hover fade 计时驱动
- 真实实现位置：
  - `Source/TerraCivilization/Public/Render/PlanetHISMInteractionComponent.h`
  - `Source/TerraCivilization/Private/Render/PlanetHISMInteractionComponent.cpp`
- `APlanetTessellatedMesh` 上旧的 HISM 交互函数当前仅保留兼容桥接，便于旧蓝图与交互控制器继续工作。
