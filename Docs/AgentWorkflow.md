# AI Agent 工作流报告（TerraCivilization R1~R7 阶段总结）

> 本文档基于 TerraCivilization 项目 R1（纯白材质）→ R7（Triplanar 真实地表）阶段中累积的协作经验，沉淀适合后续 R8+ 及自研球面网格（R8.5+）生产路线的 AI Agent 工作流规范。
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
       │   • 列 2~3 条候选方案   │       ← 不立刻动手；列权衡
       │   • 等用户拍板           │
       └─────────┬──────────────┘
                 ▼
       ┌────────────────────────┐
       │ ② 主稿简述               │       ← 在 SphericalSDFTerrainDesign.md
       │   • §6.x.1 / §11        │           只写公式 + 跳转
       │   • 不超过 30 行         │
       └─────────┬──────────────┘
                 ▼
       ┌────────────────────────┐
       │ ③ 当期独立详稿            │       ← R<N>_<Topic>.md
       │   • 8 主章节 + 附录       │           几何/HLSL/cpp/材质/验收/排错
       │   • 含完整可粘贴 HLSL    │
       └─────────┬──────────────┘
                 ▼
       ┌────────────────────────┐
       │ ④ cpp 落地 + 反射诊断    │       ← 编辑器内手工材质验收
       │   • 编译验证 0 警 0 错   │       ← Output Log 反射诊断
       │   • Roadmap 状态联动    │
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
- ❌ 用户没明确选择就直接编码或写文档。
- ❌ 用户已选 A 路径，私下用 B 路径"顺便"实现。

### 1.2 段 ② — 主稿简述

主稿是 [SphericalSDFTerrainDesign.md](C:\workspace\TerraCivilization\Docs\SphericalSDFTerrainDesign.md)。

#### 1.2.1 必须做
- 当期 §6.x.1（落地小节）写：核心公式、跨期约束、跳转链接，**不超过 12~30 行**。
- §11 Roadmap 表当期行写：动作摘要、保留链路、关键约束、可量化验收期望（一行）。
- §12 风险表新增对应风险（如"NoiseAmplitude > TriRadius 互锁伪影"）。
- §14 GPU 资源表新增对应资源（如新 LUT、新 Texture2DArray）。

#### 1.2.2 禁止做
- ❌ 把当期完整 HLSL / cpp 改动塞进主稿。
- ❌ 主稿章节超过 30 行还不拆出独立详稿（R4 早期把详细方案塞进 §11.3 144 行，后被压缩至 12 行并移出独立文档；这是教训）。

### 1.3 段 ③ — 当期独立详稿

命名：`R<N>_<Topic>.md`，放在 [Docs/](C:\workspace\TerraCivilization\Docs)。

#### 1.3.1 固定 8 章 + 附录结构（来自 R5/R6/R7 的实操沉淀）

| 章 | 内容 | 是否必须 |
| --- | --- | --- |
| §0 | 一句话目标 + 视觉对比矩阵 | ✅ |
| §1 | 几何与数学（含与上一期的差分） | ✅ |
| §2 | HLSL 实现（UE Custom 节点限制 + 完整可粘贴代码） | ✅ |
| §3 | cpp 端改动（UPROPERTY、MID 注入、反射诊断升级） | ✅ |
| §4 | 材质资产搭建（节点级 + Sampler Type + Inputs 表） | ✅ |
| §5 | 验收清单（A~J 项可勾选） | ✅ |
| §6 | 排错表（症状 → 根因 → 修复） | ✅ |
| §7 | 与上下游关系（与 R<N-1> / R<N+1> / R8.5 自研网格同构性） | ✅ |
| 附录 | 数值上限分析、参数调参参考、跨阶段同构性论证 | 推荐 |

#### 1.3.2 必须做
- §2 给出**直接可复制粘贴**的完整 HLSL Code，不要让用户"自己拼"。
- §6 排错表必须列出每个已知踩坑的完整三段式（症状 / 根因 / 修复）。
- §7 必须明确与上下游阶段的关系（关键约束：本项目 PMC 与 R8.5 自研网格共用同一套 HLSL。原 R11 PTG 同构性要求已作废）。

#### 1.3.3 禁止做
- ❌ 在详稿里写「请参考 §X.Y」却不在 §6 排错表给出操作步骤。
- ❌ 详稿落地后忘记反向更新主稿 §11 Roadmap 状态字段。

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

| ❌ 反模式 | 为什么错 | 正确做法 |
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
            // 4. 检查与 ExpectedR<N>Inputs 交集 → ✓ R<N> compliance / ✗ missing / ✗ disconnected
        }
    }
    if (CustomFound == 0)        UE_LOG(..., Error, TEXT("NO Custom nodes!"));
    else if (!bAnyR7Compliant)   UE_LOG(..., Error, TEXT("NONE is R7-compliant"));
    else                         UE_LOG(..., Log,   TEXT("is R7-compliant ✓"));
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

cpp 编译通过后必须立即把 §11 Roadmap 当期行状态从 `⏳ 待开始` 升级为：
- `🛠 cpp 完成（待材质验收）`：cpp 已落地、等用户在 UE Editor 内手工创建材质资产
- `✅ 已完成`：用户视觉验收通过

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
| 改大文件（isBigFile=true） | `replace_in_file` / `multi_replace` | — | ❌ `edit_file` |
| 创建新文件 | `edit_file`（write 模式） | — | — |
| 读 `.uasset` 二进制资产 | ❌ 都不行 | cpp 反射诊断 | `read_file`（返回乱码） |
| 验证刚编辑过的文件 | `read_file` / `grep_search` | — | ❌ `codebase_search` / `view_code_item`（有索引延迟） |
| 用户截图诊断 | `read_image` | — | 仅读文字日志（会臆断） |
| 探索历史踩坑 | `grep_history_context` / `read_history_context` | — | — |

### 2.3 `replace_in_file` / `multi_replace` 规范

> 见 `<search_replace_spec_rule>` 顶部规范。复述要点：

- `old_string` 必须是文件中**真实字符**（含真实 `\t` 制表符、空格、换行）
- ❌ 绝不要把 `\t` 错误转义成 `\\t`
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

---

## 3. UE 材质 / 纹理领域踩坑通则

> 这是本项目最重要的"领域知识沉淀"——AI 助手在 R8+ 必须默认掌握。

### 3.1 ⚠ 三重 Material 槽位陷阱（R3 / R7 都踩过）

| 槽位 | 在 Details 哪里 | 数据流向 | cpp 反射诊断会触发吗 |
| --- | --- | --- | --- |
| ❌ 错的：Element 0 | `Rendering > Materials > Element 0`（PMC 通用槽） | 直接覆盖 PMC 材质，但 cpp 看不到 | **不会** |
| ✅ 对的：PlanetTopology > Material | 自定义分组 `PlanetTopology > Material`（cpp UPROPERTY） | cpp 读取 → 包装 MID → SetMaterial(0) | **会** |

判别法：Output Log 没有反射诊断 Warning 行 = 没进入 `if (Material)` = 挂错槽。

### 3.2 ⚠ Texture Object Parameter 的两套默认值（R7 经典踩坑）

```
┌───────────────────────────────────────────────────────────┐
│ 位置 A：材质图节点本身的 Details > Texture                  │
│ • shader 编译期资源绑定源                                   │
│ • 留空 → 1×1 白纹理 fallback（不报错！）                   │
│ • R7 必须填 T_TerrainAlbedoArray                           │
└───────────────────────────────────────────────────────────┘
┌───────────────────────────────────────────────────────────┐
│ 位置 B：主材质 / MI 的 Parameter Defaults 聚合面板          │
│ • 仅 UI 投影 / MI 覆盖                                     │
│ • 不回写位置 A、不影响 shader 编译                          │
└───────────────────────────────────────────────────────────┘
```

**典型症状**：Stats ✓ + R7-compliance ✓ + cpp 反射 ✓，但 Lit 模式米白、Unlit 纯白 → 位置 A 空槽 fallback 到 1×1 白纹理。

**强制要求**：位置 A 必须挂 Texture2DArray 资产、`Sampler Type=Color`、`Sampler Source=Shared: Wrap`、`Parameter Name=TerrainAlbedoArray`（与 cpp `SetTextureParameterValue` 名一致）。

### 3.3 ⚠ HLSL 在 UE Custom 节点的 4 项硬性限制

| 限制 | 错误现象 | 正确写法 |
| --- | --- | --- |
| ❌ 不允许嵌套定义函数 | `function definition is not allowed here` + `use of undeclared identifier 'Noise3D'` | scoped block 内联（`{ ... }` 包局部变量） |
| ❌ 不允许 `#include` | 无（要用节点的 `IncludeFilePaths` 字段） | inline 全部依赖 |
| ❌ RT ClosestHit 阶段不允许 `Sample(...)` | `Opcode Sample not valid in shader model lib_6_6(closesthit)` | 一律 `SampleLevel(samp, uv, 0)` |
| ⚠ Inputs 顺序必须与 HLSL 形参一一对应 | 颜色出现不连续色块 / 编译错位 | 严格按详稿 §4.3.1 顺序声明 Inputs |

> R6 函数定义错误 + R7 RT Sample 错误 = HLSL 在 UE Custom 节点的两个最经典坑，必须默认掌握。

### 3.4 ⚠ `SetTextureParameterValue` 类型一致性

> UE 5.x 隐藏约束：注入纹理类型必须与位置 A 默认纹理类型完全一致。

- 位置 A 是空 → 类型推断错误 → MID 注入静默失败
- 位置 A 是 Texture2D，注入 Texture2DArray → UE 拒绝但不报错

修复：位置 A 必须先挂正确类型的占位 Texture2DArray，MID 才能在运行时覆盖。

### 3.5 ⚠ Sampler Type 与纹理格式必须对齐

| 纹理类型 | Sampler Type | Compression Settings |
| --- | --- | --- |
| BaseColor / Albedo（sRGB） | `Color` | Default (BC1/BC3) 或 BC7 |
| Normal Map | `Normal` | Normalmap (BC5) |
| Mask / 灰度 | `LinearGrayscale` 或 `Grayscale` | Mask 或 Default |
| LUT (FP32 RGBA) | `Linear Color` | UncompressedFP32 |

不对齐：编辑器红色错误，材质 fallback 到 WorldGridMaterial。

### 3.6 ⚠ 顶点法线写法通则（UE5 左手系 + CCW 约定，R7 经典踩坑）

> **核心结论**：UE5 是左手坐标系 + CCW frontface（[`D3D12State.cpp:356`](../../Program%20Files/Epic%20Games/UE_5.8/Engine/Source/Runtime/D3D12RHI/Private/D3D12State.cpp) `FrontCounterClockwise = true`，对所有材质硬编码全局生效）+ 漫反射用 `saturate(dot(N, L))`（[`ForwardLightingCommon.ush:387-392`](../../Program%20Files/Epic%20Games/UE_5.8/Engine/Shaders/Private/ForwardLightingCommon.ush)，6 处 lit 路径全部命中同一公式）。**对球外渲染的球面 mesh，正确的顶点法线应指向球心而非球外**——这与几何直觉相反。

#### 3.6.1 硬性要求

| # | 要求 | 实施 |
| --- | --- | --- |
| ① | **任何时候不要用原始数据直接写法线** | 例如 ❌ `Normal = UnitCenter`（朝外）/ `Normal = (P - PlanetCenter).Normalize()`（朝外）—— 都是几何直觉外法线，与 UE 的 face_normal_LH 反向，Lit 漆黑 |
| ② | **法线必须自动生成或由三角形叉积计算得到** | 首选 `KismetProceduralMeshLibrary::CalculateTangentsForMesh`（自动按 `cross(P1-P0, P2-P0)` 累加）；或显式 `cross(P1-P0, P2-P0).GetSafeNormal()` 后**确认方向**再写入 |
| ③ | **如确实需要手填，必须取负** | 例如 球面 mesh 写 `Normal = -UnitCenter`（朝内）并加注释解释——这与 UE face_normal_LH 同向 |

#### 3.6.2 判别法

- View Mode 切 Lit ↔ Unlit：**Unlit 正常 + Lit 漆黑**是法线方向反向的指纹（不是材质 fallback、不是绕序错误、不是相机位置）
- Buffer Visualization → World Normal viewmode：**朝光源一侧的半球应在 lit 后呈亮**；若反过来 → 法线方向反了

#### 3.6.3 错误归因路径（已避免重蹈，R7 复盘）

R7 Lit 漆黑曾被多次误归因，不要再走这些弯路：
1. ❌ 怀疑材质槽位挂错 → 实际不是
2. ❌ 怀疑 Texture Object Parameter 位置 A 空槽 → 实际不是
3. ❌ 怀疑相机在球内 / Actor 负 Scale / 材质 PDO → 全部排除
4. ❌ **怀疑绕序错误** → 一度修改 `Triangles.Add` 引入"朝外校正"，反而出现"看到内壁 + 相机移动方向反"等更严重的视觉错误（绕序原本就是对的，CCW from outside）
5. ✅ 用 `KismetTangents` 自动法线 + 用户实测三角形 CCW from outside + 源码验证 `FrontCounterClockwise=true` + `saturate(dot(N, L))` → 闭环

**详见**：[SphereTopologyReference.md §11](SphereTopologyReference.md#11-顶点法线与-ue5-光照约定重要结论--经验沉淀)（权威参考）/ [SphericalSDFTerrainDesign.md §11.2](SphericalSDFTerrainDesign.md#112-pmcptg-渲染契约顶点法线与-ue5-光照约定)（PMC↔PTG 跨期契约）/ [R7_TerrainTriplanar.md §6](R7_TerrainTriplanar.md) 排错表"整球 Lit 模式漆黑"行。

### 3.7 ⚠ UE5 左手系 + Z up 下的"自西向东"方向（W3 经典踩坑）

> **核心结论**：UE5 是**左手坐标系** + **Z up**。从 +Z 俯视，+X→+Y 是**顺时针**——这与地球北极俯视自转方向（**逆时针**，即"自西向东"）相反。所以在 UE5 中：**自西向东**（地理意义上的"东向"、即经度递增方向）= **+X → −Y**，而不是凭"2D 逆时针旋转 90°"直觉的 +X → +Y。

#### 3.7.1 推导

> 球面以 +Z 为北极、本初子午线穿过 +X 方向。赤道上点 P = (cos λ, **−**sin λ, 0)（λ 是经度，λ=0 时 P=+X，向东转 → −Y 方向）。
>
> 东向切向量 = dP/dλ = (−sin λ, **−**cos λ, 0)。
>
> 在 P = (x, y, 0) 时：sin λ = −y、cos λ = x，故：
>
> **East(P) = (y, −x, 0)**——也即代码里 `FVector East(U.Y, -U.X, 0.0f);`

#### 3.7.2 错误归因路径（W3 实际踩坑，2026-06-28 复盘）

W3 第一版落地的 `Step_SimulateMoisture` 凭"2D 逆时针旋转 90°"直觉写出 `FVector East(-U.Y, U.X, 0.0f);`（那是西向）→ 上风 SSSP 实际从海岸往**东**传播 → **大陆西岸湿润、东岸干燥**（与设计的"东风模型，东岸湿、西岸干"完全相反）。修复后改为 `(U.Y, -U.X, 0)`，恢复正确东西岸梯度。

| 反模式 | 表象 | 修复 |
| --- | --- | --- |
| 把"绕 +Z 逆时针 90° 旋转 (x,y,0)" 当成东向 → `(-y, x, 0)` | 在右手系 + Z up 下没问题，但 **UE5 是左手系**，"绕 +Z 逆时针" 在 UE5 里是 +X→−Y 而非 +X→+Y | `(y, -x, 0)`（**先验证手系再写公式**） |
| 想当然认为 +Y 是"东" | UE5 默认惯例确实把 +Y 视作"屏幕右"，但**地图上的"东"= 经度递增方向 = 从 +X 出发的旋转方向**，与"屏幕右"无关 | 永远从"经度公式 P(λ) → dP/dλ"推导，不靠几何直觉 |
| 忘记先验证：调试时不分东西、误以为是设计问题 | "湿度梯度反"被误归为参数过弱、SSSP 未收敛、风向选错 | **任何与"方位"挂钩的代码都先打 DrawDebug 红箭头沿 East 画一笔**，目视核对再继续 |

#### 3.7.3 通则（§3.6 同构性补充）

| # | 要求 | 实施 |
| --- | --- | --- |
| ① | **任何"东/南/西/北"语义的轴向公式都先验证手系** | UE5 = 左手系 + Z up；"自西向东" = +X→−Y；"自南向北" = 沿 +Z（北极）方向 |
| ② | **不要把 2D 旋转矩阵直觉迁移到 3D 球面** | 2D 课本默认右手系 + 逆时针正向；UE5 是左手系，旋转方向相反 |
| ③ | **先打 DrawDebug 箭头目视验证再写大段算法** | `DrawDebugDirectionalArrow(World, Cell.Center, Cell.Center + East*100, 5, FColor::Red, ...)`，看是否真的指向地球图上"东"那一侧 |

#### 3.7.4 高频轴向公式速查

| 球面位置（单位向量 U=(x,y,z)）| 切向量 | UE5 公式 |
| --- | --- | --- |
| 东向（经度递增）| dP/dλ | `FVector(U.Y, -U.X, 0).GetSafeNormal()` |
| 北向（纬度递增）| dP/dφ（与 East 正交、与 U 正交）| `FVector::CrossProduct(U, East).GetSafeNormal()`（左手系 cross 已对应"指向北"，即 +Z 半球）|
| 极区退化 | `\|East\| < ε` | 一律 `bPolar = true` 走各向同性分支，避免数值病态 |

> **教训**：本通则的存在是因为 W3.5 第一次落地时凭直觉写错了东向公式，导致大陆东西岸湿度梯度反。任何"自西向东"/"东风"/"上风"等方位语义的代码，**都必须先核对左手系下的公式再写**。

### 3.8 ⚠ GameplayTag 注册：手放 `Config/Tags/*.ini` 重启**无效**——必须经编辑器"Add New Tag Source"动作（W4 经典踩坑）

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
| 第四版（实测有效） | 在编辑器内 "Add New Gameplay Tag Source" 输入 `Terrain.ini` → 编辑器自动创建/覆盖 `Config/Tags/Terrain.ini` 并把它注册为合法 Source → 把详稿给的完整 ini 内容粘贴覆盖 → 重启编辑器 | ✅ 17 个 Tag 全部出现 |

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
│   • Project Settings → GameplayTags                            │
│   • Manage Gameplay Tags → 顶部 "Add New Gameplay Tag"         │
│   • Source 下拉框中输入新 ini 文件名（如 Terrain.ini）          │
│   • Tag 字段填随便一个占位 Tag（之后会被覆盖）                   │
│   • 点 Add New Tag                                              │
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
│   • 进 Project Settings → GameplayTags → Manage           │
│     Gameplay Tags → 看 "Sources" 列表是否包含该 ini      │
│   • 不在列表中 → 走 §3.8.3 Step 2 用编辑器 Add New Source │
│   • 已在列表中 → 跳到 Layer 2                              │
└──────────────────────────────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────────┐
│ Layer 2: ini 文件内容                                     │
│   • 路径 Config/Tags/*.ini（不是 Config/*.ini）             │
│   • Section [/Script/GameplayTags.GameplayTagsList]       │
│   • 行格式 +GameplayTagList=(Tag="Foo.Bar",DevComment="…") │
│   • 编码 UTF-8 / UTF-8 BOM 都可                            │
└──────────────────────────────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────────┐
│ Layer 3: UPROPERTY 元数据 Categories                     │
│   • 字段是否加了 meta=(Categories="...")？                  │
│   • 如有，前缀必须与 ini 中 Tag 命名空间一致                │
│   • 拿掉 Categories 测一次；立即出现 → 元数据写错了        │
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
3. 必要时加红框警示：`> ⚠ 关键陷阱（必读）：...`

---

## 5. 调试方法论

### 5.1 三层诊断法

```
┌──────────────────────────┐
│ Layer 1: cpp 层           │
│   • 反射诊断打印是否齐全？  │
│   • LUT 自检日志正常？     │
│   • Rebuild 总结行 OK？   │
└──────────────────────────┘
              │
              ▼
┌──────────────────────────┐
│ Layer 2: 材质层           │
│   • Stats 面板有红错？     │
│   • Custom 节点 Code OK？ │
│   • Texture Object 位置 A 槽位有值？ │
└──────────────────────────┘
              │
              ▼
┌──────────────────────────┐
│ Layer 3: 运行时层          │
│   • Lit / Unlit 模式分别看 │
│   • Unlit 直接显示 BaseColor，是排查纹理 fallback 的金标准 │
└──────────────────────────┘
```

### 5.2 "假绿灯"陷阱识别

**警钟**：R7-compliance ✓ + Stats ✓ + Output Log ✓ ≠ 视觉效果正确。

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

> 当前 PMC 调试 mesh（R1~R8）与 R8.5+ 自研球面网格生产路线必须共用同一套着色公式。

实现守则：
- 所有几何 / 着色公式同时适配 PMC（顺点流 UV 还原 c0/c1/c2）与自研网格（cpp 端预计算 acos 权重灬顶点）
- LUT 纹理（CellAttrLUT / CellDirLUT 与后续 4 通道材质 LUT）在 R8.5 及以后仍生效、表示语义一致
- 所有 HLSL Custom Code 在 R8 → R8.5 mesh 切换时**应当零改动复用**（PS 收到的 3 个权重语义从"重心坐标"改为"acos 软权重"，但后续混合公式完全一致）
- 详稿每章 §7 必须明确"与上下游阶段的关系"（包括不再引用的 R11 PTG 路线需明确标记为"已废弃"）

具体到当前阶段：

| 阶段 | mesh | 三个权重来源 | 区别 |
| --- | --- | --- | --- |
| R3~R8 | IsoSphere primal | 顶点流 UV 还原 c0/c1/c2 + 硬件重心坐标 | 无 Elevation 位移 |
| R8.5+ | 自研 sub+2 | cpp 预计算 "最近 3 Cell + acos 三方权重" 灬进顶点 UV1/UV2/UV3 | 含径向位移 + 法线重算 |
| ~~R11 PTG~~ | ~~PTG 高细分球皮~~ | ~~GPU FindNearestCell + acos~~ | ❌ 已废弃（R8.5 自研网格取代）|
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

---

## 8. 应做 / 不可做 全表

### 8.1 ✅ 应做

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

### 8.2 ❌ 不可做

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
- **凭"2D 逆时针旋转 90°"直觉写球面东向公式（UE5 是左手系 + Z up，"自西向东" = +X→−Y，故 East = `(U.Y, -U.X, 0)` 而非 `(-U.Y, U.X, 0)`；详见 §3.7）**
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

### R8.5（自研球面网格无 LOD）
- 渲染 mesh 切到 `FSphereTopology(SubdivisionLevel + 2)` 的 primal mesh
- cpp 端预计算每个细顶点的（13 Cell + acos 三方权重 w[3]）
- 径向位移：`Pos = Dir·(R + Σ w_i · Elev_i · HeightScale)`
- **强制使用 `KismetTangents`** 重算法线（不手填，避免 Lit 漆黑）
- 拾取路线从"PTG RuntimeMesh 碰撞"改为"自研 ProcMesh 碰撞 + Hit.ImpactPoint 径向归一化"

### W4（联调验收）
- R8 + R8.5 联调通过后，把 R3 Knuth 哈希 placeholder 换为 `Def->LayerIndex` + `Def->FTerrainMaterialParams` 真实查表
- SDF 端仅一行 cpp 改动 + 反射诊断参数名更新

### R9（多套 LUT）
- 加 Decor / Owner / Fog 三套独立 LUT（在 R8 4 通道 LUT 基础上扩展）
- 反射诊断 Inputs 升级到 ≈20–23 项

### R10（自研网格 LOD）
- 远 sub+0、中 sub+1、近 sub+2、超近 sub+3
- 拼缝（T-junction）用 skirt 法解决；可选 morph 过渡避免 LOD 跳变

### R11（高亮描边）
- 接入 §15 高亮描边带 + 选中 / 鼠标悬停的 LUT 联动
- hex 边发光描边、选中即时反馈

### 废弃路线提醒
- 原计划的 "R11 PTG 路线"（渲染从 IsoSphere 切到 PTG 高细分球皮 + GPU FindNearestCell）**已废弃**——R8.5 自研球面网格已取代该路线的全部职责。SDF 主稿 §14 仅作历史档案保留（其中 §14.7 球面重心坐标证明仍有效，被 §16.3 复用）。在 R8.5 阶段以后，可以一次性删除 `ProceduralTerrainGenerator` 插件依赖与 `PlanetBinder` 中的 PTG 桥接代码。

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
- 下次更新：R8.5 完成后（自研球面网格与 PTG 路线废弃需補充新章节）
- 长期目标：作为 R10/R11 阶段的 "AI 协作 baseline"
