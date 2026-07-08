# kismet/expressions.py
# 所有 EExprToken handler 的实现（函数分派表）
#
# 设计说明：
# - 每种 token 对应一个 _read_<name>(reader, asset) -> dict 函数
# - 最终通过 _HANDLERS 字典注册，key = EExprToken int 值
# - 辅助函数：xferstring, xferunicodestring, read_xfer_prop_pointer
#
# FReleaseObjectVersion：
#   GUID = 9DFFBCD6-494F-0158-E221-9C0D6F069F7A
#   FFieldPathOwnerSerialization = 45
#   版本 >= 45 时用 path-based FFieldPath，否则 old-style FPackageIndex
#
# 字节码中 FPackageIndex 序列化为：
#   {"$type": "UAssetAPI.UnrealTypes.FPackageIndex, UAssetAPI", "Index": N}
# （区别于属性系统中的 ObjectProperty 格式）
#
# VER_UE4_SERIALIZE_BLUEPRINT_EVENTGRAPH_FASTCALLS_IN_UFUNCTION = 508
# context 类在 object_version > 508 时不读 PropertyType byte

from __future__ import annotations
from typing import TYPE_CHECKING

from .tokens import EExprToken
from ..core.versions import LARGE_WORLD_COORDINATES

if TYPE_CHECKING:
    from ..core.reader import BinaryReader
    from ..core.uasset import UAsset

# ──────────────────────────────────────────────────────────────────────────────
# 版本常量
# ──────────────────────────────────────────────────────────────────────────────

# FReleaseObjectVersion GUID：9DFFBCD6-494F-0158-E221-9C0D6F069F7A
# 存储为 4 个 uint32 LE
_RELEASE_OBJECT_VERSION_KEY = (0x9DFFBCD6, 0x494F0158, 0xE2219C0D, 0x6F069F7A)
_FFIELD_PATH_OWNER_SERIALIZATION = 45

# object_version > 508 时，Context 不序列化 PropertyType
_VER_UE4_FASTCALLS = 508


def _get_custom_version(asset: 'UAsset', key_tuple: tuple) -> int:
    a0, a1, a2, a3 = key_tuple
    for cv in asset.custom_versions:
        g = cv.key
        if g.a == a0 and g.b == a1 and g.c == a2 and g.d == a3:
            return cv.version
    return 0


# ──────────────────────────────────────────────────────────────────────────────
# 辅助序列化函数
# ──────────────────────────────────────────────────────────────────────────────

def _pkg_index(n: int) -> dict:
    """将 FPackageIndex int 包装为 UAssetAPI JSON 格式。"""
    return {
        '$type': 'UAssetAPI.UnrealTypes.FPackageIndex, UAssetAPI',
        'Index': n,
    }


def _fname_str(name: str) -> dict:
    """FName 以简单字符串形式输出（字节码中 FName 无需复杂结构）。"""
    return name


# ──────────────────────────────────────────────────────────────────────────────
# 字符串辅助
# ──────────────────────────────────────────────────────────────────────────────

def _xferstring(reader: 'BinaryReader') -> str:
    """读取 null-terminated ASCII 字符串。"""
    chars = []
    while True:
        c = reader.read_uint8()
        if c == 0:
            break
        chars.append(chr(c))
    return ''.join(chars)


def _xferunicodestring(reader: 'BinaryReader') -> str:
    """读取 null-terminated UTF-16LE 字符串（每次读 2 字节）。"""
    chars = []
    while True:
        b1 = reader.read_uint8()
        b2 = reader.read_uint8()
        if b1 == 0 and b2 == 0:
            break
        chars.append(chr(b1 | (b2 << 8)))
    return ''.join(chars)


# ──────────────────────────────────────────────────────────────────────────────
# XFER_PROP_POINTER（FFieldPath / old FPackageIndex）
# ──────────────────────────────────────────────────────────────────────────────

def _read_xfer_prop_pointer(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    """
    读取属性指针（FFieldPath 或旧式 FPackageIndex）。

    格式选择策略（优先级从高到低）：
    1. UE5 资产 (object_version_ue5 > 0)：始终使用新格式 FFieldPath
       （UE5 内 FFieldPath 新格式是默认的，无需 custom version 声明）
    2. UE4 资产 (object_version_ue5 == 0)：
       FReleaseObjectVersion >= FFieldPathOwnerSerialization(45) → 新格式
       否则 → 旧式 FPackageIndex

    新格式：
      - int32 num
      - num 个 FName 作为 path
      - int32 ResolvedOwner（FPackageIndex）

    旧格式：
      - int32（FPackageIndex）
    """
    use_new_format = (
        asset.object_version_ue5 > 0
        or _get_custom_version(asset, _RELEASE_OBJECT_VERSION_KEY) >= _FFIELD_PATH_OWNER_SERIALIZATION
    )
    if use_new_format:
        num = reader.read_int32()
        names = [asset._read_fname(reader) for _ in range(num)]
        owner = reader.read_int32()
        return {
            '$type': 'UAssetAPI.Kismet.Bytecode.KismetPropertyPointer, UAssetAPI',
            'New': {
                '$type': 'UAssetAPI.UnrealTypes.FFieldPath, UAssetAPI',
                'Path': names,
                'ResolvedOwner': owner,
            }
        }
    else:
        idx = reader.read_int32()
        return {
            '$type': 'UAssetAPI.Kismet.Bytecode.KismetPropertyPointer, UAssetAPI',
            'Old': _pkg_index(idx),
        }


# ──────────────────────────────────────────────────────────────────────────────
# 无 payload 的 token handler
# ──────────────────────────────────────────────────────────────────────────────

def _noop(reader, asset):
    return {}


def _read_nothing_int32(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    # EX_NothingInt32 需要消耗 4 字节
    reader.read_int32()
    return {}


# ──────────────────────────────────────────────────────────────────────────────
# 变量类 token（读 FName）
# ──────────────────────────────────────────────────────────────────────────────

def _read_variable(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    return {'VariableName': _read_xfer_prop_pointer(reader, asset)}


def _read_instance_delegate(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    return {'FunctionName': asset._read_fname(reader)}


# ──────────────────────────────────────────────────────────────────────────────
# 跳转 / 流程控制
# ──────────────────────────────────────────────────────────────────────────────

def _read_jump(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    return {'CodeOffset': reader.read_uint32()}


def _read_jump_if_not(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    return {
        'CodeOffset': reader.read_uint32(),
        'BooleanExpression': read_expression(reader, asset),
    }


def _read_assert(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    return {
        'LineNumber': reader.read_uint16(),
        'DebugMode': reader.read_uint8(),
        'AssertExpression': read_expression(reader, asset),
    }


def _read_skip(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    return {
        'CodeOffset': reader.read_uint32(),
        'SkipExpression': read_expression(reader, asset),
    }


def _read_push_execution_flow(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    return {'PushingAddress': reader.read_uint32()}


def _read_computed_jump(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    return {'OffsetExpression': read_expression(reader, asset)}


def _read_pop_execution_flow_if_not(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    return {'BooleanExpression': read_expression(reader, asset)}


def _read_skip_offset_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    return {'Value': reader.read_uint32()}


# ──────────────────────────────────────────────────────────────────────────────
# 常量类 token
# ──────────────────────────────────────────────────────────────────────────────

def _read_int_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    return {'Value': reader.read_int32()}


def _read_float_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    return {'Value': reader.read_float()}


def _read_double_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    return {'Value': reader.read_double()}


def _read_string_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    return {'Value': _xferstring(reader)}


def _read_unicode_string_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    return {'Value': _xferunicodestring(reader)}


def _read_byte_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    return {'Value': reader.read_uint8()}


def _read_int64_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    return {'Value': reader.read_int64()}


def _read_uint64_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    return {'Value': reader.read_uint64()}


def _read_name_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    return {'Value': asset._read_fname(reader)}


def _read_object_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    # XFER_FUNC_POINTER = XFERPTR = ReadInt32 for FPackageIndex
    return {'Value': _pkg_index(reader.read_int32())}


def _read_soft_object_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    return {'Value': read_expression(reader, asset)}


def _read_vector_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    if asset.object_version_ue5 >= LARGE_WORLD_COORDINATES:
        return {'Value': {
            'X': reader.read_double(),
            'Y': reader.read_double(),
            'Z': reader.read_double(),
        }}
    else:
        return {'Value': {
            'X': reader.read_float(),
            'Y': reader.read_float(),
            'Z': reader.read_float(),
        }}


def _read_vector3f_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    return {'Value': {
        'X': reader.read_float(),
        'Y': reader.read_float(),
        'Z': reader.read_float(),
    }}


def _read_rotation_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    if asset.object_version_ue5 >= LARGE_WORLD_COORDINATES:
        return {'Value': {
            'Pitch': reader.read_double(),
            'Yaw': reader.read_double(),
            'Roll': reader.read_double(),
        }}
    else:
        return {'Value': {
            'Pitch': reader.read_float(),
            'Yaw': reader.read_float(),
            'Roll': reader.read_float(),
        }}


def _read_transform_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    """
    读取 FTransform：Rotation(Quat) + Translation + Scale3D
    LWC 版本用 double，否则用 float
    """
    if asset.object_version_ue5 >= LARGE_WORLD_COORDINATES:
        rot = {
            'X': reader.read_double(), 'Y': reader.read_double(),
            'Z': reader.read_double(), 'W': reader.read_double(),
        }
        trans = {
            'X': reader.read_double(), 'Y': reader.read_double(), 'Z': reader.read_double(),
        }
        scale = {
            'X': reader.read_double(), 'Y': reader.read_double(), 'Z': reader.read_double(),
        }
    else:
        rot = {
            'X': reader.read_float(), 'Y': reader.read_float(),
            'Z': reader.read_float(), 'W': reader.read_float(),
        }
        trans = {
            'X': reader.read_float(), 'Y': reader.read_float(), 'Z': reader.read_float(),
        }
        scale = {
            'X': reader.read_float(), 'Y': reader.read_float(), 'Z': reader.read_float(),
        }
    return {'Value': {'Rotation': rot, 'Translation': trans, 'Scale3D': scale}}


# ──────────────────────────────────────────────────────────────────────────────
# 函数调用类
# ──────────────────────────────────────────────────────────────────────────────

def _read_virtual_function(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression_array
    name = asset._read_fname(reader)
    params = read_expression_array(reader, asset, EExprToken.EX_EndFunctionParms)
    return {'VirtualFunctionName': name, 'Parameters': params}


def _read_final_function(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression_array
    stack_node = _pkg_index(reader.read_int32())
    params = read_expression_array(reader, asset, EExprToken.EX_EndFunctionParms)
    return {'StackNode': stack_node, 'Parameters': params}


def _read_local_virtual_function(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression_array
    name = asset._read_fname(reader)
    params = read_expression_array(reader, asset, EExprToken.EX_EndFunctionParms)
    return {'VirtualFunctionName': name, 'Parameters': params}


def _read_local_final_function(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression_array
    stack_node = _pkg_index(reader.read_int32())
    params = read_expression_array(reader, asset, EExprToken.EX_EndFunctionParms)
    return {'StackNode': stack_node, 'Parameters': params}


def _read_call_math(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression_array
    stack_node = _pkg_index(reader.read_int32())
    params = read_expression_array(reader, asset, EExprToken.EX_EndFunctionParms)
    return {'StackNode': stack_node, 'Parameters': params}


# ──────────────────────────────────────────────────────────────────────────────
# Context 类
# ──────────────────────────────────────────────────────────────────────────────

def _read_context_common(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    obj_expr = read_expression(reader, asset)
    offset = reader.read_uint32()
    r_value_ptr = _read_xfer_prop_pointer(reader, asset)
    # VER_UE4_SERIALIZE_BLUEPRINT_EVENTGRAPH_FASTCALLS_IN_UFUNCTION = 508
    # object_version > 508 时不序列化 PropertyType
    if asset.object_version <= _VER_UE4_FASTCALLS:
        _prop_type = reader.read_uint8()  # 消耗但不输出（旧版本字段）
    ctx_expr = read_expression(reader, asset)
    return {
        'ObjectExpression': obj_expr,
        'Offset': offset,
        'RValuePointer': r_value_ptr,
        'ContextExpression': ctx_expr,
    }


def _read_context(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    return _read_context_common(reader, asset)


def _read_context_fail_silent(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    return _read_context_common(reader, asset)


def _read_class_context(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    return _read_context_common(reader, asset)


def _read_struct_member_context(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    prop_ptr = _read_xfer_prop_pointer(reader, asset)
    struct_expr = read_expression(reader, asset)
    return {
        'StructMemberExpression': prop_ptr,
        'StructExpression': struct_expr,
    }


# ──────────────────────────────────────────────────────────────────────────────
# 赋值类
# ──────────────────────────────────────────────────────────────────────────────

def _read_let(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    value_ptr = _read_xfer_prop_pointer(reader, asset)
    var_expr = read_expression(reader, asset)
    assign_expr = read_expression(reader, asset)
    return {
        'Value': value_ptr,
        'Variable': var_expr,
        'Expression': assign_expr,
    }


def _read_let_bool(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    var_expr = read_expression(reader, asset)
    assign_expr = read_expression(reader, asset)
    return {
        'VariableExpression': var_expr,
        'AssignmentExpression': assign_expr,
    }


def _read_let_two_exprs(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    var_expr = read_expression(reader, asset)
    assign_expr = read_expression(reader, asset)
    return {
        'VariableExpression': var_expr,
        'AssignmentExpression': assign_expr,
    }


def _read_let_value_on_persistent_frame(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    dest_prop = _read_xfer_prop_pointer(reader, asset)
    assign_expr = read_expression(reader, asset)
    return {
        'DestinationProperty': dest_prop,
        'AssignmentExpression': assign_expr,
    }


# ──────────────────────────────────────────────────────────────────────────────
# Return
# ──────────────────────────────────────────────────────────────────────────────

def _read_return(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    return {'ReturnExpression': read_expression(reader, asset)}


# ──────────────────────────────────────────────────────────────────────────────
# Cast 类
# ──────────────────────────────────────────────────────────────────────────────

def _read_primitive_cast(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    conv = reader.read_uint8()
    target = read_expression(reader, asset)
    return {'ConversionType': conv, 'Target': target}


def _read_cast_with_class(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    class_ptr = _pkg_index(reader.read_int32())
    target = read_expression(reader, asset)
    return {'ClassPtr': class_ptr, 'Target': target}


def _read_interface_context(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    return {'InterfaceValue': read_expression(reader, asset)}


# ──────────────────────────────────────────────────────────────────────────────
# Delegate 类
# ──────────────────────────────────────────────────────────────────────────────

def _read_bind_delegate(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    func_name = asset._read_fname(reader)
    delegate = read_expression(reader, asset)
    obj_term = read_expression(reader, asset)
    return {
        'FunctionName': func_name,
        'Delegate': delegate,
        'ObjectTerm': obj_term,
    }


def _read_multicast_delegate_two_exprs(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    delegate = read_expression(reader, asset)
    delegate2 = read_expression(reader, asset)
    return {'Delegate': delegate, 'DelegateToAdd': delegate2}


def _read_clear_multicast_delegate(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    delegate = read_expression(reader, asset)
    return {'Delegate': delegate}


def _read_call_multicast_delegate(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression_array, read_expression
    stack_node = _pkg_index(reader.read_int32())
    delegate = read_expression(reader, asset)
    params = read_expression_array(reader, asset, EExprToken.EX_EndFunctionParms)
    return {'StackNode': stack_node, 'Delegate': delegate, 'Parameters': params}


# ──────────────────────────────────────────────────────────────────────────────
# Struct / Array / Set / Map Const
# ──────────────────────────────────────────────────────────────────────────────

def _read_struct_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression_array
    struct_ptr = _pkg_index(reader.read_int32())
    struct_size = reader.read_int32()
    value = read_expression_array(reader, asset, EExprToken.EX_EndStructConst)
    return {
        'Struct': struct_ptr,
        'StructSize': struct_size,
        'Value': value,
    }


def _read_array_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    inner_prop = _read_xfer_prop_pointer(reader, asset)
    count = reader.read_int32()
    elements = [read_expression(reader, asset) for _ in range(count)]
    return {
        'InnerProperty': inner_prop,
        'Count': count,
        'Elements': elements,
    }


def _read_set_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    set_prop = _read_xfer_prop_pointer(reader, asset)
    count = reader.read_int32()
    elements = [read_expression(reader, asset) for _ in range(count)]
    return {
        'SetProperty': set_prop,
        'Count': count,
        'Elements': elements,
    }


def _read_map_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    key_prop = _read_xfer_prop_pointer(reader, asset)
    val_prop = _read_xfer_prop_pointer(reader, asset)
    count = reader.read_int32()
    elements = [read_expression(reader, asset) for _ in range(count * 2)]
    return {
        'KeyProperty': key_prop,
        'ValueProperty': val_prop,
        'Count': count,
        'Elements': elements,
    }


def _read_set_array(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression_array
    assigning_prop = _read_xfer_prop_pointer(reader, asset)
    elements = read_expression_array(reader, asset, EExprToken.EX_EndArray)
    return {'AssigningProperty': assigning_prop, 'Elements': elements}


def _read_set_set(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression_array
    assigning_prop = _read_xfer_prop_pointer(reader, asset)
    elements = read_expression_array(reader, asset, EExprToken.EX_EndSet)
    return {'AssigningProperty': assigning_prop, 'Elements': elements}


def _read_set_map(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression_array
    assigning_prop = _read_xfer_prop_pointer(reader, asset)
    elements = read_expression_array(reader, asset, EExprToken.EX_EndMap)
    return {'AssigningProperty': assigning_prop, 'Elements': elements}


# ──────────────────────────────────────────────────────────────────────────────
# SwitchValue
# ──────────────────────────────────────────────────────────────────────────────

def _read_switch_value(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    num_cases = reader.read_uint16()
    end_goto = reader.read_uint32()
    index_term = read_expression(reader, asset)
    cases = []
    for _ in range(num_cases):
        case_value = read_expression(reader, asset)
        next_offset = reader.read_uint32()
        case_term = read_expression(reader, asset)
        cases.append({
            'CaseValue': case_value,
            'NextOffset': next_offset,
            'CaseTerm': case_term,
        })
    default_term = read_expression(reader, asset)
    return {
        'NumCases': num_cases,
        'EndGotoOffset': end_goto,
        'IndexTerm': index_term,
        'Cases': cases,
        'DefaultTerm': default_term,
    }


# ──────────────────────────────────────────────────────────────────────────────
# TextConst（FScriptText）
# ──────────────────────────────────────────────────────────────────────────────

# EBlueprintTextLiteralType
_TEXT_LITERAL_EMPTY           = 0
_TEXT_LITERAL_LOCALIZED       = 1
_TEXT_LITERAL_INVARIANT       = 2
_TEXT_LITERAL_LITERAL_STRING  = 3
_TEXT_LITERAL_STRING_TABLE    = 4
_TEXT_LITERAL_IS_EMPTY        = 5


def _read_text_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    lit_type = reader.read_uint8()

    if lit_type == _TEXT_LITERAL_EMPTY:
        return {'TextLiteralType': 'Empty'}
    elif lit_type == _TEXT_LITERAL_LOCALIZED:
        return {
            'TextLiteralType': 'LocalizedText',
            'LocalizedSource': read_expression(reader, asset),
            'LocalizedKey': read_expression(reader, asset),
            'LocalizedNamespace': read_expression(reader, asset),
        }
    elif lit_type == _TEXT_LITERAL_INVARIANT:
        return {
            'TextLiteralType': 'InvariantText',
            'InvariantLiteralString': read_expression(reader, asset),
        }
    elif lit_type == _TEXT_LITERAL_LITERAL_STRING:
        return {
            'TextLiteralType': 'LiteralString',
            'LiteralString': read_expression(reader, asset),
        }
    elif lit_type == _TEXT_LITERAL_STRING_TABLE:
        return {
            'TextLiteralType': 'StringTableEntry',
            'StringTableAsset': read_expression(reader, asset),
            'StringTableId': asset._read_fname(reader),
            'StringTableKey': read_expression(reader, asset),
        }
    elif lit_type == _TEXT_LITERAL_IS_EMPTY:
        return {'TextLiteralType': 'IsEmptyText'}
    else:
        return {'TextLiteralType': lit_type}


# ──────────────────────────────────────────────────────────────────────────────
# 其他
# ──────────────────────────────────────────────────────────────────────────────

def _read_bit_field_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    prop_ptr = _read_xfer_prop_pointer(reader, asset)
    zero = reader.read_uint8()
    return {'FProperty': prop_ptr, 'Zero': zero}


def _read_property_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    return {'Property': _read_xfer_prop_pointer(reader, asset)}


def _read_field_path_const(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    return {'Value': read_expression(reader, asset)}


def _read_array_get_by_ref(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    arr_var = read_expression(reader, asset)
    arr_idx = read_expression(reader, asset)
    return {'ArrayVariable': arr_var, 'ArrayIndex': arr_idx}


def _read_instrumentation_event(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    event_type = reader.read_uint8()
    result: dict = {'EventType': event_type}
    # EventType 4 = FunctionCall, 5 = FunctionTailCall
    if event_type in (4, 5):
        result['FunctionName'] = asset._read_fname(reader)
    return result


def _read_auto_rtfm_abort_if_not(reader: 'BinaryReader', asset: 'UAsset') -> dict:
    from .expression import read_expression
    return {'Condition': read_expression(reader, asset)}


# ──────────────────────────────────────────────────────────────────────────────
# 分派表 _HANDLERS
# 全部 EExprToken → handler 函数
# ──────────────────────────────────────────────────────────────────────────────

_T = EExprToken

_HANDLERS: dict = {
    # ── 变量 ──────────────────────────────────────────
    _T.EX_LocalVariable:              _read_variable,
    _T.EX_InstanceVariable:           _read_variable,
    _T.EX_DefaultVariable:            _read_variable,
    _T.EX_LocalOutVariable:           _read_variable,
    _T.EX_ClassSparseDataVariable:    _read_variable,

    # ── 无 payload ────────────────────────────────────
    _T.EX_Nothing:                    _noop,
    _T.EX_NothingInt32:               _read_nothing_int32,
    _T.EX_EndFunctionParms:           _noop,
    _T.EX_Self:                       _noop,
    _T.EX_EndParmValue:               _noop,
    _T.EX_EndStructConst:             _noop,
    _T.EX_EndArray:                   _noop,
    _T.EX_EndSet:                     _noop,
    _T.EX_EndMap:                     _noop,
    _T.EX_EndSetConst:                _noop,
    _T.EX_EndMapConst:                _noop,
    _T.EX_EndArrayConst:              _noop,
    _T.EX_EndOfScript:                _noop,
    _T.EX_NoObject:                   _noop,
    _T.EX_NoInterface:                _noop,
    _T.EX_IntZero:                    _noop,
    _T.EX_IntOne:                     _noop,
    _T.EX_True:                       _noop,
    _T.EX_False:                      _noop,
    _T.EX_Tracepoint:                 _noop,
    _T.EX_WireTracepoint:             _noop,
    _T.EX_Breakpoint:                 _noop,
    _T.EX_PopExecutionFlow:           _noop,
    _T.EX_DeprecatedOp4A:             _noop,
    _T.EX_AutoRtfmTransact:           _noop,
    _T.EX_AutoRtfmStopTransact:       _noop,

    # ── 跳转/流程 ─────────────────────────────────────
    _T.EX_Jump:                       _read_jump,
    _T.EX_JumpIfNot:                  _read_jump_if_not,
    _T.EX_Assert:                     _read_assert,
    _T.EX_Skip:                       _read_skip,
    _T.EX_PushExecutionFlow:          _read_push_execution_flow,
    _T.EX_ComputedJump:               _read_computed_jump,
    _T.EX_PopExecutionFlowIfNot:      _read_pop_execution_flow_if_not,
    _T.EX_SkipOffsetConst:            _read_skip_offset_const,

    # ── 常量 ──────────────────────────────────────────
    _T.EX_IntConst:                   _read_int_const,
    _T.EX_IntConstByte:               _read_byte_const,
    _T.EX_FloatConst:                 _read_float_const,
    _T.EX_DoubleConst:                _read_double_const,
    _T.EX_StringConst:                _read_string_const,
    _T.EX_UnicodeStringConst:         _read_unicode_string_const,
    _T.EX_ByteConst:                  _read_byte_const,
    _T.EX_Int64Const:                 _read_int64_const,
    _T.EX_UInt64Const:                _read_uint64_const,
    _T.EX_NameConst:                  _read_name_const,
    _T.EX_ObjectConst:                _read_object_const,
    _T.EX_SoftObjectConst:            _read_soft_object_const,
    _T.EX_VectorConst:                _read_vector_const,
    _T.EX_Vector3fConst:              _read_vector3f_const,
    _T.EX_RotationConst:              _read_rotation_const,
    _T.EX_TransformConst:             _read_transform_const,

    # ── 函数调用 ──────────────────────────────────────
    _T.EX_VirtualFunction:            _read_virtual_function,
    _T.EX_FinalFunction:              _read_final_function,
    _T.EX_LocalVirtualFunction:       _read_local_virtual_function,
    _T.EX_LocalFinalFunction:         _read_local_final_function,
    _T.EX_CallMath:                   _read_call_math,

    # ── Context ───────────────────────────────────────
    _T.EX_Context:                    _read_context,
    _T.EX_Context_FailSilent:         _read_context_fail_silent,
    _T.EX_ClassContext:               _read_class_context,
    _T.EX_StructMemberContext:        _read_struct_member_context,

    # ── 赋值 ──────────────────────────────────────────
    _T.EX_Let:                        _read_let,
    _T.EX_LetBool:                    _read_let_bool,
    _T.EX_LetObj:                     _read_let_two_exprs,
    _T.EX_LetWeakObjPtr:              _read_let_two_exprs,
    _T.EX_LetDelegate:                _read_let_two_exprs,
    _T.EX_LetMulticastDelegate:       _read_let_two_exprs,
    _T.EX_LetValueOnPersistentFrame:  _read_let_value_on_persistent_frame,

    # ── Return ────────────────────────────────────────
    _T.EX_Return:                     _read_return,

    # ── Cast ──────────────────────────────────────────
    _T.EX_PrimitiveCast:              _read_primitive_cast,
    _T.EX_ObjToInterfaceCast:         _read_cast_with_class,
    _T.EX_CrossInterfaceCast:         _read_cast_with_class,
    _T.EX_InterfaceToObjCast:         _read_cast_with_class,
    _T.EX_DynamicCast:                _read_cast_with_class,
    _T.EX_MetaCast:                   _read_cast_with_class,
    _T.EX_InterfaceContext:           _read_interface_context,

    # ── Delegate ──────────────────────────────────────
    _T.EX_InstanceDelegate:           _read_instance_delegate,
    _T.EX_BindDelegate:               _read_bind_delegate,
    _T.EX_AddMulticastDelegate:       _read_multicast_delegate_two_exprs,
    _T.EX_RemoveMulticastDelegate:    _read_multicast_delegate_two_exprs,
    _T.EX_ClearMulticastDelegate:     _read_clear_multicast_delegate,
    _T.EX_CallMulticastDelegate:      _read_call_multicast_delegate,

    # ── Struct/Array/Set/Map Const ────────────────────
    _T.EX_StructConst:                _read_struct_const,
    _T.EX_ArrayConst:                 _read_array_const,
    _T.EX_SetConst:                   _read_set_const,
    _T.EX_MapConst:                   _read_map_const,
    _T.EX_SetArray:                   _read_set_array,
    _T.EX_SetSet:                     _read_set_set,
    _T.EX_SetMap:                     _read_set_map,

    # ── SwitchValue ───────────────────────────────────
    _T.EX_SwitchValue:                _read_switch_value,

    # ── TextConst ─────────────────────────────────────
    _T.EX_TextConst:                  _read_text_const,

    # ── 其他 ──────────────────────────────────────────
    _T.EX_BitFieldConst:              _read_bit_field_const,
    _T.EX_PropertyConst:              _read_property_const,
    _T.EX_FieldPathConst:             _read_field_path_const,
    _T.EX_ArrayGetByRef:              _read_array_get_by_ref,
    _T.EX_InstrumentationEvent:       _read_instrumentation_event,
    _T.EX_AutoRtfmAbortIfNot:         _read_auto_rtfm_abort_if_not,
}
