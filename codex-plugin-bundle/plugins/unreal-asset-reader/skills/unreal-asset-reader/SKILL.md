---
name: unreal-asset-reader
description: Use when listing Content paths, reading asset properties, checking references, validating assets, exporting asset data from Unreal Engine .uasset files, reading DataTable contents/row names/column values/config tables, or asking for project path, engine path, .uproject content, modules, plugins, or engine version in Unreal Engine projects.
---

# Unreal Asset Reader

Read Unreal Engine assets via three tiers (FileSystem → uasset_py → Commandlet). All tools: `.claude/skills/unreal-asset-reader/`.

> **路径占位符说明**（从项目 MEMORY.md / CLAUDE.md 读取，或运行时动态获取）：
> - `<PROJECT_ROOT_BASH>` — 工程根目录 bash 路径（盘符因机器而异）；通过 `pwd` 或 CLAUDE.md 获取
> - `<PROJECT_ROOT_WIN>` — 工程根目录 Windows 路径；通过 `cygpath -w "$PROJECT_ROOT_BASH"` 转换
> - `<GAME_DIR>` — 含 .uproject 的游戏子目录名（各工程不同，如 `CodeHG`、`Fermion` 等）；**首次使用前必须执行下方 Step 0 确认**
> - 临时文件目录：bash 中用 `$TEMP`（Git Bash 已将 `%TEMP%` 映射为 bash 路径）；Read 工具读取时用 `$(cygpath -w "$TEMP")` 转换为 Windows 路径；Python 中用 `os.environ['TEMP']`

## Step 0：项目路径定位（首次使用必做）

若 MEMORY.md / CLAUDE.md 中已有 uproject 路径，直接使用；否则自动发现：

```bash
# 在工程根目录下查找 .uproject（不进 Engine/ 子目录，避免扫描引擎本体）
find "<PROJECT_ROOT_BASH>" -maxdepth 3 -name "*.uproject" ! -path "*/Engine/*" | head -5
```

> 通常只有一个结果，即 `<PROJECT_ROOT_BASH>/<GAME_DIR>/<UPROJECT_NAME>.uproject`。
> 记录 `<GAME_DIR>`（含 .uproject 的子目录名）和 `<UPROJECT_NAME>`（不含扩展名）。

### 已知路径（直接返回）

| 项目 | 路径 |
|------|------|
| 工程根目录 | `<PROJECT_ROOT_WIN>` |
| .uproject | `<PROJECT_ROOT_WIN>\<GAME_DIR>\<UPROJECT_NAME>.uproject` |
| Editor CMD | `Engine\Binaries\Win64\UnrealEditor-Cmd.exe` |

### 项目元数据（Read .uproject）

`<PROJECT_ROOT_BASH>/<GAME_DIR>/<UPROJECT_NAME>.uproject` 是标准 JSON，用 Read 工具直接读取：

| 用户问题 | 读取的键 |
|----------|----------|
| 引擎版本 | `EngineAssociation` |
| 模块列表 | `Modules[].Name` |
| 插件列表 | `Plugins[].Name + Enabled` |
| 项目描述 | `Category`, `Description` |

## Tier Selection

| 需求 | Tier | 速度 |
|------|------|------|
| 仅需文件路径列表 | 0 (Glob/Shell) | 即时 |
| 单个/少量资产属性 | 1 (uasset_py) | 秒级 |
| 批量 >20 / AssetRegistry / UE Python | 2 (Commandlet) | 1–5 min |

**Tier 1 Fallback：** 轻量命令失败 → `tojson` → 仍失败 → Tier 2 Commandlet。

## ⚠️ 执行环境说明（重要）

Claude Code 运行在 **Git Bash** 环境，**必须**使用 bash 风格路径执行命令：

| 场景 | 正确做法 | 错误做法 |
|------|----------|----------|
| 执行 Python 工具 | `cd ".claude/skills/unreal-asset-reader" && python -m uasset_py ...` | `cmd.exe /c "..."` 或 `powershell -Command "..."` |
| 输出 JSON 路径 | `$TEMP/output.json`（Read 时用 `$(cygpath -w "$TEMP")\output.json`） | `%TEMP%\output.json`（bash 不解析 %VAR%） |
| 搜索文件 | `find "<PROJECT_ROOT_BASH>/<GAME_DIR>/Content" -name "*.uasset" \| grep -i "KeyWord"` | `dir /s /b` 或 `Get-ChildItem` |

> **原因：** `cmd.exe` 回显经 bash 管道后输出丢失或乱码（GBK 编码）；`powershell -Command "..."` 中 `$var`、单引号会被 bash 预处理破坏。

## Tier 0: 文件系统

**Glob（推荐）：** `<GAME_DIR>/Content/<SubPath>/**/*.uasset`

**Bash find：**
```bash
find "<PROJECT_ROOT_BASH>/<GAME_DIR>/Content" -name "*.uasset" | grep -i "KeyWord"
```

路径转换：`/Game/Characters` → `<GAME_DIR>/Content/Characters`；插件内容在 `<GAME_DIR>/Plugins/.../Content/...`。

## Tier 1: uasset_py

**调用方式：**

```bash
cd ".claude/skills/unreal-asset-reader"
python -m uasset_py <command> <source.uasset> <output.json> <EngineVersion> [mappings.usmap]
```

**EngineVersion 格式：** 字符串如 `VER_UE5_3`，或数字如 `23`（= UE4.23）。

**读取输出：** Read 工具需要 Windows 路径：`$(cygpath -w "$TEMP")\<output.json>`

| 命令 | 用途 | 输出结构 |
|------|------|----------|
| `refs` | 引用/依赖分析（轻量优先） | `{ "Imports": [...] }` |
| `toc` | 包摘要+导出索引（轻量） | `{ "ExportCount", "Exports": [{ObjectName, ExportType}] }` |
| `unlua` | 蓝图 UnLua GetModuleName（轻量） | `{ "GetModuleName": "..." \| null }` |
| `dt-rows` | DataTable 行名列表（轻量） | `{ "RowNames": [...] }` |
| `dt-schema` | DataTable 列结构（轻量） | `{ "Columns": [{Name, PropertyType}] }` |
| `tojson` | 全量属性（托底，轻量命令失败时用） | 完整 JSON（NameMap / Imports / Exports） |

执行后用 Read 工具读取输出 JSON 分析。

## Tier 2: Commandlet（UE Python 批量）

1. 编写使用 `import unreal` 的 Python 脚本，结果写入 `os.environ['TEMP'] + '/output.json'`。
2. 运行：`.claude/skills/unreal-asset-reader/scripts/run_commandlet.bat "<script.py>"`
3. Read 工具读取路径：`$(cygpath -w "$TEMP")\output.json`。模板：`scripts/batch_query.py`。超时建议 300000 ms+。

## 命令说明

| 命令 | 说明 | lazy parsing |
|------|------|--------------|
| `toc` | 资产目录（Export 列表、NameMap 数量、Import 数量） | 是（跳过 export body） |
| `refs` | 外部引用（Import 列表） | 是（跳过 export body） |
| `tojson` | 完整 JSON 序列化（含属性值） | 否（全量解析） |
| `dt-rows` | DataTable 行名列表 | 是（跳过 export body） |
| `dt-schema` | DataTable 列结构（字段名 + 类型） | 是（跳过 export body） |
| `unlua` | 提取 UnLua 模块名（GetModuleName） | 是（跳过 export body） |

> 批量处理（>100 个资产），建议用 `-m uasset_py` 批量模式或 Tier 2 Commandlet。

## P4 修改资产

若需**修改**资产，先用 P4 标记 `edit`，再修改（见项目规则）。

## 参考

- JSON 结构详解、DataTable 行解析 Python 模板（导出类型、属性类型、引用查找）: [reference.md](reference.md)
- 蓝图 + Lua 联合分析标准流程（bp-lua-analyze）: [bp-lua-analyze.md](bp-lua-analyze.md)
