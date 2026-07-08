# cli.py
# 命令行接口：toc / refs / tojson 子命令

from __future__ import annotations
import json
import sys
from pathlib import Path
from typing import List, Optional

from .core.versions import parse_engine_version
from .core.uasset import UAsset


def _dumps_json(data, indent: bool = True) -> str:
    """统一 JSON 序列化入口：使用标准库 json。"""
    return json.dumps(data, indent=2 if indent else None,
                      ensure_ascii=False, default=_json_default)


def cmd_toc(args: List[str]):
    """
    用法：toc <source.uasset> <destination.json> <EngineVersion> [mappings]

    只解析 header + name map + import/export map，不解析 export data body，
    因此性能良好，对大文件也很快。
    """
    if len(args) < 3:
        print("Usage: toc <source.uasset> <destination.json> <EngineVersion> [mappings]",
              file=sys.stderr)
        sys.exit(1)

    src_path    = args[0]
    dst_path    = args[1]
    engine_ver  = args[2]

    try:
        obj_ver, obj_ver_ue5 = parse_engine_version(engine_ver)

        data = Path(src_path).read_bytes()
        asset = UAsset(data, object_version=obj_ver, object_version_ue5=obj_ver_ue5, lazy=True)

        result = asset.toc()
        Path(dst_path).write_text(
            _dumps_json(result),
            encoding='utf-8'
        )
    except FileNotFoundError as e:
        print(f"Error: File not found: {e.filename}", file=sys.stderr)
        sys.exit(1)
    except (ValueError, EOFError, NotImplementedError) as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


def cmd_refs(args: List[str]):
    """
    用法：refs <source.uasset> <destination.json> <EngineVersion> [mappings]

    输出 import map 中所有外部依赖。
    """
    if len(args) < 3:
        print("Usage: refs <source.uasset> <destination.json> <EngineVersion> [mappings]",
              file=sys.stderr)
        sys.exit(1)

    src_path   = args[0]
    dst_path   = args[1]
    engine_ver = args[2]

    try:
        obj_ver, obj_ver_ue5 = parse_engine_version(engine_ver)

        data = Path(src_path).read_bytes()
        asset = UAsset(data, object_version=obj_ver, object_version_ue5=obj_ver_ue5, lazy=True)

        result = asset.refs()
        Path(dst_path).write_text(
            _dumps_json(result),
            encoding='utf-8'
        )
    except FileNotFoundError as e:
        print(f"Error: File not found: {e.filename}", file=sys.stderr)
        sys.exit(1)
    except (ValueError, EOFError, NotImplementedError) as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


def _json_default(obj):
    """JSON 序列化兜底：处理不可序列化类型。"""
    if isinstance(obj, bytes):
        return list(obj)
    if isinstance(obj, memoryview):
        return list(bytes(obj))
    return str(obj)


def cmd_tojson(args: List[str]):
    """
    用法：tojson <source.uasset> <destination.json> <EngineVersion> [mappings]

    完整解析（header + export body properties），输出兼容 UAssetAPI 格式的 JSON。
    单个 export 解析失败会降级为 RawExport，不影响整体输出。
    """
    if len(args) < 3:
        print("Usage: tojson <source.uasset> <destination.json> <EngineVersion> [mappings]",
              file=sys.stderr)
        sys.exit(1)

    src_path   = args[0]
    dst_path   = args[1]
    engine_ver = args[2]

    try:
        obj_ver, obj_ver_ue5 = parse_engine_version(engine_ver)

        data = Path(src_path).read_bytes()
        asset = UAsset(data, object_version=obj_ver, object_version_ue5=obj_ver_ue5, lazy=False)

        result = asset.serialize_json()
        Path(dst_path).write_text(
            _dumps_json(result),
            encoding='utf-8'
        )
        print(f"tojson: {len(asset.exports)} exports → {dst_path}", file=sys.stderr)
    except FileNotFoundError as e:
        print(f"Error: File not found: {e.filename}", file=sys.stderr)
        sys.exit(1)
    except (ValueError, EOFError, NotImplementedError) as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        import traceback
        traceback.print_exc()
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


def _load_asset_for_dt(src_path: str, engine_ver: str):
    """加载资产并返回 (asset, DataTableExport)，找不到 DataTable 则抛 ValueError。"""
    from .core.versions import parse_engine_version
    from .core.uasset import UAsset
    from .exports.data_table_export import DataTableExport
    from pathlib import Path

    obj_ver, obj_ver_ue5 = parse_engine_version(engine_ver)
    data = Path(src_path).read_bytes()
    asset = UAsset(data, object_version=obj_ver, object_version_ue5=obj_ver_ue5, lazy=False)
    asset.parse_export_bodies()

    for exp_obj in asset._export_objects:
        if isinstance(exp_obj, DataTableExport):
            return asset, exp_obj

    raise ValueError(f"No DataTableExport found in {src_path}")


def cmd_dt_rows(args: list[str]):
    """
    用法：dt-rows <source.uasset> <destination.json> <EngineVersion>

    输出 DataTable 所有行名称列表：
    {"RowNames": ["Row1", "Row2", ...]}
    """
    if len(args) < 3:
        print("Usage: dt-rows <source.uasset> <destination.json> <EngineVersion>",
              file=sys.stderr)
        sys.exit(1)

    src_path   = args[0]
    dst_path   = args[1]
    engine_ver = args[2]

    try:
        asset, dt_export = _load_asset_for_dt(src_path, engine_ver)
        row_names = [row.row_name for row in dt_export.table_data]
        result = {"RowNames": row_names}
        Path(dst_path).write_text(
            _dumps_json(result),
            encoding='utf-8'
        )
        print(f"dt-rows: {len(row_names)} rows → {dst_path}", file=sys.stderr)
    except FileNotFoundError as e:
        print(f"Error: File not found: {e.filename}", file=sys.stderr)
        sys.exit(1)
    except (ValueError, EOFError, NotImplementedError) as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        import traceback
        traceback.print_exc()
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


def cmd_dt_schema(args: list[str]):
    """
    用法：dt-schema <source.uasset> <destination.json> <EngineVersion>

    输出 DataTable schema（取第一行的属性列表作为列定义）：
    {"Columns": [{"Name": "PropName", "PropertyType": "FloatPropertyData"}, ...]}
    """
    if len(args) < 3:
        print("Usage: dt-schema <source.uasset> <destination.json> <EngineVersion>",
              file=sys.stderr)
        sys.exit(1)

    src_path   = args[0]
    dst_path   = args[1]
    engine_ver = args[2]

    try:
        asset, dt_export = _load_asset_for_dt(src_path, engine_ver)

        columns = []
        if dt_export.table_data:
            first_row = dt_export.table_data[0]
            for prop in first_row.properties:
                columns.append({
                    "Name": getattr(prop, 'name', ''),
                    "PropertyType": type(prop).__name__,
                })

        result = {"Columns": columns}
        Path(dst_path).write_text(
            _dumps_json(result),
            encoding='utf-8'
        )
        print(f"dt-schema: {len(columns)} columns → {dst_path}", file=sys.stderr)
    except FileNotFoundError as e:
        print(f"Error: File not found: {e.filename}", file=sys.stderr)
        sys.exit(1)
    except (ValueError, EOFError, NotImplementedError) as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        import traceback
        traceback.print_exc()
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


def _collect_string_consts(expr, results: list):
    """
    递归遍历 KismetExpression 树，收集所有 EX_StringConst 的值。

    设计说明：
    - EX_StringConst 的 fields['Value'] 就是字符串
    - 其他 token 的 fields 可能包含嵌套的 KismetExpression（Parameters、ReturnExpression 等）
    - 递归遍历所有 fields 中的 KismetExpression 对象和列表
    """
    from .kismet.tokens import EExprToken
    from .kismet.expression import KismetExpression

    if not isinstance(expr, KismetExpression):
        return

    if expr.token == EExprToken.EX_StringConst:
        val = expr.fields.get('Value')
        if val is not None:
            results.append(val)
        return  # StringConst 无嵌套子表达式

    # 递归遍历 fields 中的子表达式
    for v in expr.fields.values():
        if isinstance(v, KismetExpression):
            _collect_string_consts(v, results)
        elif isinstance(v, list):
            for item in v:
                if isinstance(item, KismetExpression):
                    _collect_string_consts(item, results)
                elif isinstance(item, dict):
                    # SwitchValue cases 等复杂结构可能有嵌套 dict
                    for dv in item.values():
                        if isinstance(dv, KismetExpression):
                            _collect_string_consts(dv, results)


def cmd_unlua(args: list):
    """
    用法：unlua <source.uasset> <destination.json> <EngineVersion>

    搜索 FunctionExport name="GetModuleName" 中的 EX_StringConst，
    返回第一个字符串值作为 Lua 模块名。

    输出格式：
    {"GetModuleName": "Lua.Module.Name"}

    UnLua 框架约定：
    UE 蓝图中挂 UnLua 的 Blueprint 会有一个名为 GetModuleName 的函数，
    其函数体只有 StringConst + Return 两条指令，StringConst 的值就是
    对应的 Lua 模块路径（如 "UI.BP_MainMenu"）。
    """
    if len(args) < 3:
        print("Usage: unlua <source.uasset> <destination.json> <EngineVersion>",
              file=sys.stderr)
        sys.exit(1)

    src_path   = args[0]
    dst_path   = args[1]
    engine_ver = args[2]

    try:
        obj_ver, obj_ver_ue5 = parse_engine_version(engine_ver)
        data = Path(src_path).read_bytes()
        asset = UAsset(data, object_version=obj_ver, object_version_ue5=obj_ver_ue5, lazy=False)
        asset.parse_export_bodies()

        from .exports.function_export import FunctionExport

        get_module_name: Optional[str] = None

        for exp_obj in asset._export_objects:
            if not isinstance(exp_obj, FunctionExport):
                continue
            # 匹配函数名 GetModuleName（忽略 UE FName number 后缀）
            obj_name = getattr(exp_obj, '_header', None)
            if obj_name is None:
                continue
            func_name = exp_obj._header.object_name
            if func_name != 'GetModuleName':
                continue

            # 遍历字节码，找第一个 EX_StringConst
            bytecode = exp_obj.script_bytecode
            if not bytecode:
                break
            strings: list = []
            for expr in bytecode:
                _collect_string_consts(expr, strings)
                if strings:
                    break
            if strings:
                get_module_name = strings[0]
            break

        result = {'GetModuleName': get_module_name}
        Path(dst_path).write_text(
            _dumps_json(result),
            encoding='utf-8',
        )
        print(f"unlua: GetModuleName={get_module_name!r} → {dst_path}", file=sys.stderr)

    except FileNotFoundError as e:
        print(f"Error: File not found: {e.filename}", file=sys.stderr)
        sys.exit(1)
    except (ValueError, EOFError, NotImplementedError) as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        import traceback
        traceback.print_exc()
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


COMMANDS = {
    'toc':       cmd_toc,
    'refs':      cmd_refs,
    'tojson':    cmd_tojson,
    'dt-rows':   cmd_dt_rows,
    'dt-schema': cmd_dt_schema,
    'unlua':     cmd_unlua,
}


def main(argv: Optional[List[str]] = None):
    if argv is None:
        argv = sys.argv[1:]

    if not argv:
        print("Usage: python -m uasset_py <command> [args...]", file=sys.stderr)
        print("Commands:", ', '.join(COMMANDS), file=sys.stderr)
        sys.exit(1)

    cmd = argv[0].lower()
    if cmd not in COMMANDS:
        print(f"Unknown command: {cmd}", file=sys.stderr)
        print("Commands:", ', '.join(COMMANDS), file=sys.stderr)
        sys.exit(1)

    COMMANDS[cmd](argv[1:])
