# AI Agent 工作流报告（TerraCivilization R1~R7 阶段总结）

> 本文档基于 TerraCivilization 项目 R1（纯白材质）→ R7（Triplanar 真实地表）阶段中累积的协作经验，沉淀适合后续 R8+ 以及 PTG（R11）路线的 AI Agent 工作流规范。
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
| §7 | 与上下游关系（与 R<N-1> / R<N+1> / R11 PTG 同构性） | ✅ |
| 附录 | 数值上限分析、参数调参参考、跨阶段同构性论证 | 推荐 |

#### 1.3.2 必须做
- §2 给出**直接可复制粘贴**的完整 HLSL Code，不要让用户"自己拼"。
- §6 排错表必须列出每个已知踩坑的完整三段式（症状 / 根因 / 修复）。
- §7 必须明确与 R11 PTG 路线的同构性（关键约束：本项目 PMC 与 PTG 共用同一套 HLSL）。

#### 1.3.3 禁止做
- ❌ 在详稿里写「请参考 §X.Y」却不在 §6 排错表给出操作步骤。
- ❌ 详稿落地后忘记反向更新主稿 §11 Roadmap 状态字段。

### 1.4 段 ④ — cpp 落地 + 编辑器验收

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

> 当前 PMC 调试 mesh 与 R11+ PTG 生产路线必须共用同一套着色公式。

实现守则：
- 所有几何 / 着色公式同时适配 PMC（顶点流 UV 还原 c0/c1/c2）与 PTG（GPU `FindNearestCell`）
- LUT 纹理（CellAttrLUT / CellDirLUT）是 R11 的天然产物，PMC 阶段就已经沉淀
- 所有 HLSL Custom Code 在 R11 切换到 PTG 时**应当零改动复用**
- 详稿每章 §7 必须明确"与 R11 PTG 的关系"

具体到当前阶段：

| 阶段 | PMC 路径 | R11 PTG 路径 | 区别 |
| --- | --- | --- | --- |
| R3~R7 | 顶点流 UV 还原 c0/c1/c2 | GPU FindNearestCell | 仅 c0/c1/c2 来源不同 |
| 其余 | 一致 | 一致 | 完全一致 |

---

## 7. 协作守则速查卡

| 场景 | 行为 |
| --- | --- |
| 用户提需求 | 先澄清、列方案、等拍板 |
| 主稿 vs 详稿 | 主稿简述 + 跳转，详稿独立成文 |
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

## 10. 后续阶段（R8~R11）的常驻提醒

### R8（接 WorldGen）
- 把 R3 Knuth 哈希 placeholder 改为 `FCellGeoData → LayerIndex` 真实查询
- `RebuildCellAttrLUT_` 改为按 `TerrainTag → LayerIndex` 写入
- 反射诊断不变（Inputs 数仍是 14）

### R9（多套 LUT）
- 加 Decor / Owner / Fog 三套独立 LUT
- 反射诊断 Inputs 升级到 17~20

### R10（LOD）
- 远距离 R4（无噪声、无 Triplanar）
- 近距离 R7 全套
- 实现可能在 cpp 端用 LOD-aware MID（per-camera distance）

### R11（PTG）
- HLSL Custom Code 零改动复用
- c0/c1/c2 来源换成 GPU `FindNearestCell`
- LUT 纹理（CellAttrLUT / CellDirLUT）继续生效

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
- 下次更新：R10 完成后（LOD 与跨期同构性需要补充新章节）
- 长期目标：作为 R11 PTG 切换前的"AI 协作 baseline"
