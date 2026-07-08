# properties/simple.py
# 所有简单（标量/字符串/名字/枚举/对象引用）属性类型的实现
#
# 设计思路：
# - 使用 @register 装饰器自动注册，模块被导入时即注册
# - 每个类只实现 read_value() + to_json()，公共逻辑在 PropertyData 基类
# - BoolPropertyData 特殊：值存在 PropertyTag 的 flags 里（bool_val 字段）

from __future__ import annotations
from typing import TYPE_CHECKING, Optional, Any

from .base import PropertyData
from .registry import register

if TYPE_CHECKING:
    from ..core.reader import BinaryReader
    from ..core.uasset import UAsset
    from .base import PropertyTag

from ..core.versions import FSOFTOBJECTPATH_REMOVE_ASSET_PATH_FNAMES


def _read_fname(reader: 'BinaryReader', asset: 'UAsset') -> str:
    idx, num = reader.read_fname_raw()
    return asset._resolve_fname(idx, num)


# ── 整数类型 ────────────────────────────────────────────────

@register("IntProperty")
class IntPropertyData(PropertyData):
    __slots__ = ('value',)
    PROPERTY_TYPE = "IntProperty"

    def __init__(self):
        super().__init__()
        self.value: int = 0

    def read_value(self, reader, include_header, tag, leng, asset):
        self.value = reader.read_int32()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = self.value
        return d


@register("Int8Property")
class Int8PropertyData(PropertyData):
    __slots__ = ('value',)
    PROPERTY_TYPE = "Int8Property"

    def __init__(self):
        super().__init__()
        self.value: int = 0

    def read_value(self, reader, include_header, tag, leng, asset):
        self.value = reader.read_int8()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = self.value
        return d


@register("Int16Property")
class Int16PropertyData(PropertyData):
    __slots__ = ('value',)
    PROPERTY_TYPE = "Int16Property"

    def __init__(self):
        super().__init__()
        self.value: int = 0

    def read_value(self, reader, include_header, tag, leng, asset):
        self.value = reader.read_int16()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = self.value
        return d


@register("Int64Property")
class Int64PropertyData(PropertyData):
    __slots__ = ('value',)
    PROPERTY_TYPE = "Int64Property"

    def __init__(self):
        super().__init__()
        self.value: int = 0

    def read_value(self, reader, include_header, tag, leng, asset):
        self.value = reader.read_int64()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = self.value
        return d


@register("UInt16Property")
class UInt16PropertyData(PropertyData):
    __slots__ = ('value',)
    PROPERTY_TYPE = "UInt16Property"

    def __init__(self):
        super().__init__()
        self.value: int = 0

    def read_value(self, reader, include_header, tag, leng, asset):
        self.value = reader.read_uint16()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = self.value
        return d


@register("UInt32Property")
class UInt32PropertyData(PropertyData):
    __slots__ = ('value',)
    PROPERTY_TYPE = "UInt32Property"

    def __init__(self):
        super().__init__()
        self.value: int = 0

    def read_value(self, reader, include_header, tag, leng, asset):
        self.value = reader.read_uint32()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = self.value
        return d


@register("UInt64Property")
class UInt64PropertyData(PropertyData):
    __slots__ = ('value',)
    PROPERTY_TYPE = "UInt64Property"

    def __init__(self):
        super().__init__()
        self.value: int = 0

    def read_value(self, reader, include_header, tag, leng, asset):
        self.value = reader.read_uint64()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = self.value
        return d


# ── 浮点类型 ────────────────────────────────────────────────

@register("FloatProperty")
class FloatPropertyData(PropertyData):
    __slots__ = ('value',)
    PROPERTY_TYPE = "FloatProperty"

    def __init__(self):
        super().__init__()
        self.value: float = 0.0

    def read_value(self, reader, include_header, tag, leng, asset):
        self.value = reader.read_float()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = self.value
        return d


@register("DoubleProperty")
class DoublePropertyData(PropertyData):
    __slots__ = ('value',)
    PROPERTY_TYPE = "DoubleProperty"

    def __init__(self):
        super().__init__()
        self.value: float = 0.0

    def read_value(self, reader, include_header, tag, leng, asset):
        self.value = reader.read_double()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = self.value
        return d


# ── Bool ────────────────────────────────────────────────────

@register("BoolProperty")
class BoolPropertyData(PropertyData):
    """
    BoolProperty 的值在 include_header 模式下存于 PropertyTag.bool_val；
    在 include_header=False（数组内部）时读一个 uint8。
    """
    __slots__ = ('value',)
    PROPERTY_TYPE = "BoolProperty"

    def __init__(self):
        super().__init__()
        self.value: bool = False

    def read_value(self, reader, include_header, tag, leng, asset):
        if include_header and tag is not None:
            self.value = tag.bool_val
        else:
            self.value = bool(reader.read_uint8())

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = self.value
        return d


# ── 字节 ────────────────────────────────────────────────────

@register("ByteProperty")
class BytePropertyData(PropertyData):
    """
    ByteProperty 有两种情况：
    - 枚举类型（EnumType != None/ByteProperty）：读 FName（枚举值名）
    - 原始字节：读 uint8
    """
    __slots__ = ('value', 'enum_type', '_enum_type')
    PROPERTY_TYPE = "ByteProperty"

    def __init__(self):
        super().__init__()
        self.value: Any = 0
        self.enum_type: str = 'None'
        self._enum_type: str = 'None'  # 从 tag 注入

    def read_value(self, reader, include_header, tag, leng, asset):
        enum_t = getattr(self, '_enum_type', 'None') or 'None'
        self.enum_type = enum_t

        if include_header and leng == 8:
            # 枚举 FName（8 字节 = 两个 int32）
            self.value = _read_fname(reader, asset)
        elif include_header and leng == 1:
            self.value = reader.read_uint8()
        else:
            # 不带 header，判断 enum_type
            if enum_t not in ('None', 'ByteProperty', ''):
                self.value = _read_fname(reader, asset)
            else:
                self.value = reader.read_uint8()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['EnumType'] = self.enum_type if self.enum_type not in ('None', '') else None
        d['Value'] = self.value
        return d


# ── 字符串 ──────────────────────────────────────────────────

@register("StrProperty")
class StrPropertyData(PropertyData):
    __slots__ = ('value',)
    PROPERTY_TYPE = "StrProperty"

    def __init__(self):
        super().__init__()
        self.value: Optional[str] = None

    def read_value(self, reader, include_header, tag, leng, asset):
        self.value = reader.read_fstring()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = self.value
        return d


@register("NameProperty")
class NamePropertyData(PropertyData):
    __slots__ = ('value',)
    PROPERTY_TYPE = "NameProperty"

    def __init__(self):
        super().__init__()
        self.value: str = 'None'

    def read_value(self, reader, include_header, tag, leng, asset):
        self.value = _read_fname(reader, asset)

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = self.value
        return d


# ── 枚举 ────────────────────────────────────────────────────

@register("EnumProperty")
class EnumPropertyData(PropertyData):
    """
    EnumProperty：读枚举值名（FName）。
    EnumType 来自 tag（经典格式的额外字段）。
    """
    __slots__ = ('value', 'enum_type', '_enum_type')
    PROPERTY_TYPE = "EnumProperty"

    def __init__(self):
        super().__init__()
        self.value: str = 'None'
        self.enum_type: str = 'None'
        self._enum_type: str = 'None'

    def read_value(self, reader, include_header, tag, leng, asset):
        self.enum_type = getattr(self, '_enum_type', 'None') or 'None'
        if leng == 0 and include_header:
            # 有时 EnumProperty 在 tag 长度=0 的情况下不读值
            self.value = 'None'
        else:
            self.value = _read_fname(reader, asset)

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['EnumType'] = self.enum_type if self.enum_type not in ('None', '') else None
        d['Value'] = self.value
        return d


# ── 对象引用（FPackageIndex）──────────────────────────────────

@register("ObjectProperty")
class ObjectPropertyData(PropertyData):
    __slots__ = ('value',)
    PROPERTY_TYPE = "ObjectProperty"

    def __init__(self):
        super().__init__()
        self.value: int = 0  # FPackageIndex raw int32

    def read_value(self, reader, include_header, tag, leng, asset):
        self.value = reader.read_int32()

    def _resolve(self, asset: 'UAsset') -> Optional[str]:
        idx = self.value
        if idx == 0:
            return None
        if idx > 0 and idx - 1 < len(asset.exports):
            return asset.exports[idx - 1].object_name
        if idx < 0:
            imp_idx = -idx - 1
            if imp_idx < len(asset.imports):
                return asset.imports[imp_idx].object_name
        return f'<index:{idx}>'

    def to_json(self, asset) -> dict:
        d = self._base_json()
        # C# UAssetAPI 通过 JsonConverter 将 FPackageIndex 扁平化为纯 int
        d['Value'] = self.value
        return d


@register("WeakObjectProperty")
class WeakObjectPropertyData(PropertyData):
    __slots__ = ('value',)
    PROPERTY_TYPE = "WeakObjectProperty"

    def __init__(self):
        super().__init__()
        self.value: int = 0

    def read_value(self, reader, include_header, tag, leng, asset):
        self.value = reader.read_int32()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        # C# UAssetAPI 通过 JsonConverter 将 FPackageIndex 扁平化为纯 int
        d['Value'] = self.value
        return d


@register("InterfaceProperty")
class InterfacePropertyData(PropertyData):
    __slots__ = ('value',)
    PROPERTY_TYPE = "InterfaceProperty"

    def __init__(self):
        super().__init__()
        self.value: int = 0

    def read_value(self, reader, include_header, tag, leng, asset):
        self.value = reader.read_uint32()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = self.value
        return d


# ── SoftObjectProperty ──────────────────────────────────────

@register("SoftObjectProperty")
class SoftObjectPropertyData(PropertyData):
    """
    SoftObjectProperty：软对象引用。

    版本分支：
    - ue5 >= ADD_SOFTOBJECTPATH_LIST (9)：int32 索引到 asset.soft_object_paths_list
    - ue5 >= FSOFTOBJECTPATH_REMOVE_ASSET_PATH_FNAMES (8) 且 < 9：FName×2 + FString
    - 旧格式：FString(AssetPath) + FString(SubPath)
    """
    __slots__ = ('asset_path_name', 'sub_path_string', 'package_name', 'asset_name', 'soft_path_index')
    PROPERTY_TYPE = "SoftObjectProperty"

    def __init__(self):
        super().__init__()
        self.asset_path_name: Optional[str] = None
        self.sub_path_string: Optional[str] = None
        self.package_name: Optional[str] = None
        self.asset_name: Optional[str] = None
        self.soft_path_index: int = -1

    def read_value(self, reader, include_header, tag, leng, asset):
        from ..core.versions import ADD_SOFTOBJECTPATH_LIST, FSOFTOBJECTPATH_REMOVE_ASSET_PATH_FNAMES
        if asset.object_version_ue5 >= ADD_SOFTOBJECTPATH_LIST:
            # 新格式（ue5 >= 9）：int32 索引到 soft_object_paths_list
            self.soft_path_index = reader.read_int32()
            if 0 <= self.soft_path_index < len(asset.soft_object_paths_list):
                entry = asset.soft_object_paths_list[self.soft_path_index]
                self.package_name = entry['pkg']
                self.asset_name = entry['asset']
                self.sub_path_string = entry.get('sub')
        elif asset.object_version_ue5 >= FSOFTOBJECTPATH_REMOVE_ASSET_PATH_FNAMES:
            # 中间格式（ue5 8）：FTopLevelAssetPath + SubPath
            self.package_name = _read_fname(reader, asset)
            self.asset_name = _read_fname(reader, asset)
            self.sub_path_string = reader.read_fstring()
        else:
            # 旧格式：AssetPath FString + SubPath FString
            self.asset_path_name = reader.read_fstring()
            self.sub_path_string = reader.read_fstring()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        if self.package_name is not None:
            d['Value'] = {
                '$type': 'UAssetAPI.PropertyTypes.Objects.FSoftObjectPath, UAssetAPI',
                'AssetPath': {
                    '$type': 'UAssetAPI.PropertyTypes.Objects.FTopLevelAssetPath, UAssetAPI',
                    'PackageName': self.package_name,
                    'AssetName': self.asset_name,
                },
                'SubPathString': self.sub_path_string,
            }
        else:
            d['Value'] = {
                'AssetPathName': self.asset_path_name,
                'SubPathString': self.sub_path_string,
            }
        return d


# ── AssetObjectProperty（类似 SoftObject）─────────────────────

@register("AssetObjectProperty")
class AssetObjectPropertyData(PropertyData):
    __slots__ = ('value',)
    PROPERTY_TYPE = "AssetObjectProperty"

    def __init__(self):
        super().__init__()
        self.value: Optional[str] = None

    def read_value(self, reader, include_header, tag, leng, asset):
        self.value = reader.read_fstring()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = self.value
        return d


# ── FieldPathProperty ────────────────────────────────────────

@register("FieldPathProperty")
class FieldPathPropertyData(PropertyData):
    __slots__ = ('path', 'owner_index')
    PROPERTY_TYPE = "FieldPathProperty"

    def __init__(self):
        super().__init__()
        self.path: list[str] = []
        self.owner_index: int = 0

    def read_value(self, reader, include_header, tag, leng, asset):
        count = reader.read_int32()
        self.path = [_read_fname(reader, asset) for _ in range(count)]
        self.owner_index = reader.read_int32()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = {
            'Path': self.path,
            'OwnerIndex': self.owner_index,
        }
        return d


# ── Delegate ─────────────────────────────────────────────────

@register("DelegateProperty")
class DelegatePropertyData(PropertyData):
    """FScriptDelegate：object index + function name"""
    __slots__ = ('object_index', 'function_name')
    PROPERTY_TYPE = "DelegateProperty"

    def __init__(self):
        super().__init__()
        self.object_index: int = 0
        self.function_name: str = 'None'

    def read_value(self, reader, include_header, tag, leng, asset):
        self.object_index = reader.read_int32()
        self.function_name = _read_fname(reader, asset)

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = {
            'Object': self.object_index,
            'FunctionName': self.function_name,
        }
        return d


@register("MulticastDelegateProperty")
class MulticastDelegatePropertyData(PropertyData):
    """FMulticastScriptDelegate：数组 FScriptDelegate"""
    __slots__ = ('delegates',)
    PROPERTY_TYPE = "MulticastDelegateProperty"

    def __init__(self):
        super().__init__()
        self.delegates: list[dict] = []

    def read_value(self, reader, include_header, tag, leng, asset):
        count = reader.read_int32()
        self.delegates = []
        for _ in range(count):
            obj_idx = reader.read_int32()
            fname = _read_fname(reader, asset)
            self.delegates.append({'Object': obj_idx, 'FunctionName': fname})

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = self.delegates
        return d


@register("MulticastInlineDelegateProperty")
class MulticastInlineDelegatePropertyData(MulticastDelegatePropertyData):
    PROPERTY_TYPE = "MulticastInlineDelegateProperty"


@register("MulticastSparseDelegateProperty")
class MulticastSparseDelegatePropertyData(MulticastDelegatePropertyData):
    PROPERTY_TYPE = "MulticastSparseDelegateProperty"
