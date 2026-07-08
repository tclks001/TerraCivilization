# Unreal Asset Reader — Reference

## uasset_py (Tier 1)

**调用方式：** 必须先 `cd` 到 skill 目录，再用 `python -m uasset_py` 执行。

**Engine versions (common):** `VER_UE5_6`, `VER_UE5_5`, `VER_UE5_4`, `VER_UE5_3`, `VER_UE5_2`, `VER_UE5_1`, `VER_UE4_27`, `VER_UE4_26`. Use `VER_UE5_3` for UE 5.3.

**Mappings:** Optional. Pass as fourth argument if the project provides a `.usmap` file.

### 轻量命令（优先）

- **refs** — 仅输出本包 Imports，适合依赖/引用分析。
```bash
cd ".claude/skills/unreal-asset-reader" && python -m uasset_py refs <input.uasset> <output.json> <EngineVersion> [mappings.usmap]
```
输出结构：`{ "Imports": [ { "ObjectName", "ClassName", "ClassPackage", "OuterIndex" }, ... ] }`

- **toc** — 包摘要 + 导出索引，无 Data/Table/字节码。
```bash
cd ".claude/skills/unreal-asset-reader" && python -m uasset_py toc <input.uasset> <output.json> <EngineVersion> [mappings.usmap]
```
输出结构：`{ "NameMapCount", "ImportCount", "ExportCount", "Exports": [ { "Index", "ObjectName", "ExportType", "OuterIndex" }, ... ] }`

- **unlua** — 从蓝图字节码解析 GetModuleName（UnLua 绑定）。仅对实现 GetModuleName 的蓝图有效。
```bash
cd ".claude/skills/unreal-asset-reader" && python -m uasset_py unlua <input.uasset> <output.json> <EngineVersion> [mappings.usmap]
```
输出结构：`{ "GetModuleName": "Game.Module.Path" }` 或 `{ "GetModuleName": null }`

- **dt-rows** — DataTable 行名列表。非 DataTable 输出空数组。
```bash
cd ".claude/skills/unreal-asset-reader" && python -m uasset_py dt-rows <input.uasset> <output.json> <EngineVersion> [mappings.usmap]
```
输出结构：`{ "RowNames": [ "Row1", "Row2", ... ] }`

- **dt-schema** — DataTable 列结构（第一行的属性名与类型）。非 DataTable 输出空 Columns。
```bash
cd ".claude/skills/unreal-asset-reader" && python -m uasset_py dt-schema <input.uasset> <output.json> <EngineVersion> [mappings.usmap]
```
输出结构：`{ "Columns": [ { "Name", "PropertyType" }, ... ] }`

### tojson（托底，全量）

当需要完整属性、复杂嵌套或 refs/toc 无法满足时使用：
```bash
cd ".claude/skills/unreal-asset-reader" && python -m uasset_py tojson <input.uasset> <output.json> <EngineVersion> [mappings.usmap]
```

## JSON Output Structure (High Level)

uasset_py 导出的 JSON 遵循 .uasset 二进制布局。 Typical top-level keys:

- **NameMap** — string table used by the asset.
- **Imports** — list of external references. Each entry has:
  - `ClassPackage`, `ClassName` — type of the reference.
  - `ObjectName` — name of the referenced object.
  - Other fields (e.g. `OuterIndex`) for linking.
- **Exports** — list of objects in the asset. Each export has:
  - `ObjectName` — export name (e.g. default subobject name).
  - `ClassName` / export type — e.g. `NormalExport`, `DataTableExport`, `BlueprintExport`.
  - Type-specific data (e.g. `Table` for DataTable, `Data` for NormalExport properties).

## Finding References

- **Imports** = what this asset references (other packages/objects).
- To find “who references this asset” you need either:
  - Tier 2: use `unreal.AssetRegistryHelpers.get_asset_registry()` and reference queries, or
  - Tier 1: parse many assets and collect references from each JSON `Imports` array.

## Common Export Types

- **NormalExport** — generic UObject with a `Data` list of properties (e.g. `FloatProperty`, `ObjectProperty`, `SoftObjectProperty`).
- **DataTableExport** — `Table` with row data (list of structs; see unreal-datatable-reader skill).
- **BlueprintExport** — blueprint class; look for child exports (components, graph, default values).
- **StaticMeshExport / SkeletalMeshExport** — mesh data; LOD and section info in type-specific fields.
- **MaterialExport** — material; parameters and connections in type-specific structure.

## Property Types in JSON

Properties appear as objects with a `Name` and type-specific value:

- **FloatProperty**, **IntProperty**, **BoolProperty** — `Value`.
- **ObjectProperty**, **SoftObjectProperty** — object index or path.
- **StrProperty**, **NameProperty** — string/name value.
- **ArrayProperty** — `Value` array of elements.
- **StructProperty** — `Value` with struct type and fields.
- **EnumProperty** — enum type and value.

Use the export’s `Data` array and match by `Name` to get a specific property.

## DataTable Rows

For DataTable assets, the main export usually has a `Table` with a `Data` array. Each row is often a `StructProperty`; the row name and column values are inside.

### 在 JSON 中定位表数据（tojson 输出）

- `Exports` 数组中找含 `"Table"` 键的导出项
- `exp["Table"]["Data"]` 是行数组，每个元素是一个 dict（不是字符串）
- 每行结构：`{ "Name": "<行名>", "Value": [ {字段1}, {字段2}, ... ] }`
- 每个字段结构：`{ "Name": "<字段名>", "Value": <值或嵌套结构> }`

> ⚠️ **`Exports` 中的元素可能混有字符串**（非 dict），遍历时必须先用 `isinstance(exp, dict)` 过滤，否则对字符串调用 `.get()` 会抛 `AttributeError: 'str' object has no attribute 'get'`。

### 提取行/列 Python 模板

```python
import json, os

json_path = os.path.join(os.environ['TEMP'], 'dt_export.json')

with open(json_path, encoding="utf-8") as f:
    data = json.load(f)

# ✅ 必须用 isinstance 过滤，Exports 中可能有字符串元素
exports = data.get("Exports", [])
dt_export = next(
    (exp for exp in exports if isinstance(exp, dict) and "Table" in exp),
    None
)

if not dt_export:
    print("未找到 DataTable 导出项")
else:
    rows = dt_export["Table"].get("Data", [])
    print(f"共 {len(rows)} 行")
    for row in rows:
        row_name = row.get("Name", "?")
        fields = row.get("Value", [])
        print(f"\n行名: {row_name}")
        for field in fields:
            fname = field.get("Name", "?")
            fval  = field.get("Value", None)
            print(f"  {fname} = {fval}")
```

- 嵌套 struct 列（`Value` 是 list）：递归取 `field["Value"]` 即可
- 大表（行多）：用 Tier 2 Commandlet + `unreal.DataTableFunctionLibrary` 只查询所需行

## Errors

- If tojson fails (missing file, unsupported format, serialization error), use the error message to report. Consider Tier 2 for that asset if it is critical.
- Ensure the `.uexp` file is next to the `.uasset` when present; pass only the `.uasset` path to tojson.

## Tier 2 代码模式示例（UE Python）

以下片段供编写 Tier 2 临时脚本时参考，基于 `scripts/batch_query.py` 模板扩展。

### 读取蓝图 CDO（Class Default Object）属性

CDO 存放蓝图变量的默认值，Tier 1 无法直接读取，需要 UE Python：

```python
import unreal, json, os

BLUEPRINT_PATH = "/Game/Your/Path/BP_Foo"

asset = unreal.EditorAssetLibrary.load_asset(BLUEPRINT_PATH)

# 获取生成类
gen_class = asset.generated_class
if callable(gen_class):
    gen_class = gen_class()

# 读取 CDO 属性
cdo = gen_class.class_default_object if gen_class else None
if cdo is not None:
    props = {}
    try:
        for name in cdo.get_editor_property_names():
            try:
                val = cdo.get_editor_property(name)
                # unreal 原生类型统一转字符串
                props[name] = val.get_path_name() if hasattr(val, "get_path_name") else str(val)
            except Exception as e:
                props[name] = f"<error: {e}>"
    except Exception as e:
        props["_error"] = str(e)

output = os.path.join(os.environ.get("TEMP", "."), "ue_cdo_output.json")
with open(output, "w", encoding="utf-8") as f:
    json.dump({"cdo_path": cdo.get_path_name(), "properties": props}, f, ensure_ascii=False, indent=2)
print(output)
```

**注意：** `get_editor_property_names()` 在 UE Python 中不是通用 API，部分蓝图类型可能不支持，此时 `props` 会包含 `_error` 字段。

### 枚举蓝图子组件（SubobjectDataSubsystem）

用于获取蓝图 Component 列表（等价于编辑器 Components 面板）：

```python
import unreal, json, os

BLUEPRINT_PATH = "/Game/Your/Path/BP_Foo"

asset = unreal.EditorAssetLibrary.load_asset(BLUEPRINT_PATH)
gen_class = asset.generated_class
if callable(gen_class):
    gen_class = gen_class()

components = []
try:
    subsys = unreal.get_editor_subsystem(unreal.SubobjectDataSubsystem)
    if subsys is not None:
        handles = subsys.k2_gather_subobject_data_for_blueprint(asset)
        for h in (handles or []):
            try:
                data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)
                if data is not None:
                    components.append(
                        data.get_path_name() if hasattr(data, "get_path_name") else str(data)
                    )
            except Exception:
                pass
except Exception as e:
    components = [f"<error: {e}>"]

output = os.path.join(os.environ.get("TEMP", "."), "ue_components_output.json")
with open(output, "w", encoding="utf-8") as f:
    json.dump({"blueprint": BLUEPRINT_PATH, "components": components}, f, ensure_ascii=False, indent=2)
print(output)
```

**注意：** `SubobjectDataSubsystem` 仅在 Editor 模式下可用（-run=pythonscript 满足此条件）；`k2_gather_subobject_data_for_blueprint` 传入的是蓝图资产对象，不是生成类。
