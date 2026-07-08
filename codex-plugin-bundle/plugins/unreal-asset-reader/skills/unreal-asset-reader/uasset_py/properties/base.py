# properties/base.py
# PropertyData 基类 + 属性 tag 读取逻辑（versioned 模式）
#
# 设计说明：
# - __slots__ 减少内存占用，属性 class 往往有大量实例
# - read_property() 是统一入口，处理 versioned/unversioned 两种路径
# - to_json() 输出兼容 UAssetAPI C# 格式的 JSON（带 $type 字段）

from __future__ import annotations
from typing import TYPE_CHECKING, Optional, Any

from ..core.versions import (
    VER_UE4_PROPERTY_GUID_IN_PROPERTY_TAG,
    PROPERTY_TAG_COMPLETE_TYPE_NAME,
    PROPERTY_TAG_EXTENSION_AND_OVERRIDABLE_SERIALIZATION,
)

if TYPE_CHECKING:
    from ..core.reader import BinaryReader
    from ..core.uasset import UAsset

# C# UAssetAPI 对应的 $type 名称映射表
CSHARP_TYPE_MAP: dict[str, str] = {
    'NormalExport': 'UAssetAPI.ExportTypes.NormalExport, UAssetAPI',
    'RawExport': 'UAssetAPI.ExportTypes.RawExport, UAssetAPI',
    'DataTableExport': 'UAssetAPI.ExportTypes.DataTableExport, UAssetAPI',
    'EnumExport': 'UAssetAPI.ExportTypes.EnumExport, UAssetAPI',
    'StringTableExport': 'UAssetAPI.ExportTypes.StringTableExport, UAssetAPI',
    'StructExport': 'UAssetAPI.ExportTypes.StructExport, UAssetAPI',
    'FunctionExport': 'UAssetAPI.ExportTypes.FunctionExport, UAssetAPI',
    'ClassExport': 'UAssetAPI.ExportTypes.ClassExport, UAssetAPI',
    'LevelExport': 'UAssetAPI.ExportTypes.LevelExport, UAssetAPI',
    # Simple properties
    'IntPropertyData': 'UAssetAPI.PropertyTypes.Objects.IntPropertyData, UAssetAPI',
    'FloatPropertyData': 'UAssetAPI.PropertyTypes.Objects.FloatPropertyData, UAssetAPI',
    'BoolPropertyData': 'UAssetAPI.PropertyTypes.Objects.BoolPropertyData, UAssetAPI',
    'StrPropertyData': 'UAssetAPI.PropertyTypes.Objects.StrPropertyData, UAssetAPI',
    'NamePropertyData': 'UAssetAPI.PropertyTypes.Objects.NamePropertyData, UAssetAPI',
    'EnumPropertyData': 'UAssetAPI.PropertyTypes.Objects.EnumPropertyData, UAssetAPI',
    'ObjectPropertyData': 'UAssetAPI.PropertyTypes.Objects.ObjectPropertyData, UAssetAPI',
    'SoftObjectPropertyData': 'UAssetAPI.PropertyTypes.Objects.SoftObjectPropertyData, UAssetAPI',
    'TextPropertyData': 'UAssetAPI.PropertyTypes.Objects.TextPropertyData, UAssetAPI',
    'ArrayPropertyData': 'UAssetAPI.PropertyTypes.Objects.ArrayPropertyData, UAssetAPI',
    'MapPropertyData': 'UAssetAPI.PropertyTypes.Objects.MapPropertyData, UAssetAPI',
    'SetPropertyData': 'UAssetAPI.PropertyTypes.Objects.SetPropertyData, UAssetAPI',
    'StructPropertyData': 'UAssetAPI.PropertyTypes.Structs.StructPropertyData, UAssetAPI',
    'BytePropertyData': 'UAssetAPI.PropertyTypes.Objects.BytePropertyData, UAssetAPI',
    'Int8PropertyData': 'UAssetAPI.PropertyTypes.Objects.Int8PropertyData, UAssetAPI',
    'Int16PropertyData': 'UAssetAPI.PropertyTypes.Objects.Int16PropertyData, UAssetAPI',
    'Int64PropertyData': 'UAssetAPI.PropertyTypes.Objects.Int64PropertyData, UAssetAPI',
    'UInt16PropertyData': 'UAssetAPI.PropertyTypes.Objects.UInt16PropertyData, UAssetAPI',
    'UInt32PropertyData': 'UAssetAPI.PropertyTypes.Objects.UInt32PropertyData, UAssetAPI',
    'UInt64PropertyData': 'UAssetAPI.PropertyTypes.Objects.UInt64PropertyData, UAssetAPI',
    'DoublePropertyData': 'UAssetAPI.PropertyTypes.Objects.DoublePropertyData, UAssetAPI',
    'WeakObjectPropertyData': 'UAssetAPI.PropertyTypes.Objects.WeakObjectPropertyData, UAssetAPI',
    'InterfacePropertyData': 'UAssetAPI.PropertyTypes.Objects.InterfacePropertyData, UAssetAPI',
    'FieldPathPropertyData': 'UAssetAPI.PropertyTypes.Objects.FieldPathPropertyData, UAssetAPI',
    'DelegatePropertyData': 'UAssetAPI.PropertyTypes.Objects.DelegatePropertyData, UAssetAPI',
    'MulticastDelegatePropertyData': 'UAssetAPI.PropertyTypes.Objects.MulticastDelegatePropertyData, UAssetAPI',
    'MulticastInlineDelegatePropertyData': 'UAssetAPI.PropertyTypes.Objects.MulticastInlineDelegatePropertyData, UAssetAPI',
    'MulticastSparseDelegatePropertyData': 'UAssetAPI.PropertyTypes.Objects.MulticastSparseDelegatePropertyData, UAssetAPI',
    'AssetObjectPropertyData': 'UAssetAPI.PropertyTypes.Objects.AssetObjectPropertyData, UAssetAPI',
    # Struct variants
    'VectorPropertyData': 'UAssetAPI.PropertyTypes.Structs.Math.VectorPropertyData, UAssetAPI',
    'RotatorPropertyData': 'UAssetAPI.PropertyTypes.Structs.Math.RotatorPropertyData, UAssetAPI',
    'QuatPropertyData': 'UAssetAPI.PropertyTypes.Structs.Math.QuatPropertyData, UAssetAPI',
    'LinearColorPropertyData': 'UAssetAPI.PropertyTypes.Structs.Math.LinearColorPropertyData, UAssetAPI',
    'ColorPropertyData': 'UAssetAPI.PropertyTypes.Structs.Core.ColorPropertyData, UAssetAPI',
    'GuidPropertyData': 'UAssetAPI.PropertyTypes.Structs.Core.GuidPropertyData, UAssetAPI',
    'DateTimePropertyData': 'UAssetAPI.PropertyTypes.Structs.Core.DateTimePropertyData, UAssetAPI',
    'TimespanPropertyData': 'UAssetAPI.PropertyTypes.Structs.Core.TimespanPropertyData, UAssetAPI',
    'IntPointPropertyData': 'UAssetAPI.PropertyTypes.Structs.Math.IntPointPropertyData, UAssetAPI',
    'BoxPropertyData': 'UAssetAPI.PropertyTypes.Structs.Math.BoxPropertyData, UAssetAPI',
    'SoftObjectPathPropertyData': 'UAssetAPI.PropertyTypes.Structs.Misc.SoftObjectPathPropertyData, UAssetAPI',
    'GameplayTagContainerPropertyData': 'UAssetAPI.PropertyTypes.Structs.Misc.GameplayTagContainerPropertyData, UAssetAPI',
    'RichCurveKeyPropertyData': 'UAssetAPI.PropertyTypes.Structs.Engine.RichCurveKeyPropertyData, UAssetAPI',
}

# EPropertyTagFlags
TAG_FLAG_HAS_ARRAY_INDEX         = 1 << 0
TAG_FLAG_HAS_PROPERTY_GUID       = 1 << 1
TAG_FLAG_HAS_PROPERTY_EXTENSIONS = 1 << 2
TAG_FLAG_BOOL_TRUE               = 1 << 3  # BoolProperty 的 bool 值存在 flags 里


def _read_fname(reader: 'BinaryReader', asset: 'UAsset') -> str:
    idx, num = reader.read_fname_raw()
    return asset._resolve_fname(idx, num)


class FPropertyTypeName:
    """
    UE5.0+ 新格式（ObjectVersionUE5 >= PROPERTY_TAG_COMPLETE_TYPE_NAME）的类型名结构。
    存储格式：FName(type) + uint8(num_params) + params...
    """
    __slots__ = ('name', 'params')

    def __init__(self, reader: 'BinaryReader', asset: 'UAsset'):
        self.name = _read_fname(reader, asset)
        num_params = reader.read_uint8()
        self.params: list[FPropertyTypeName] = []
        for _ in range(num_params):
            self.params.append(FPropertyTypeName(reader, asset))

    def get_name(self) -> str:
        return self.name

    def get_param(self, index: int) -> Optional['FPropertyTypeName']:
        if 0 <= index < len(self.params):
            return self.params[index]
        return None

    def __repr__(self) -> str:
        if self.params:
            return f"{self.name}<{', '.join(str(p) for p in self.params)}>"
        return self.name


class PropertyTag:
    """
    属性 tag（标签头信息），在 include_header=True 时读取。
    包含属性名、类型、长度、数组索引等元信息。
    """
    __slots__ = (
        'name', 'type_name', 'length', 'array_index',
        'property_guid', 'bool_val', 'tag_flags',
        # 经典格式的额外字段
        'struct_type', 'enum_type', 'inner_type', 'value_type',
        'struct_guid',  # StructProperty 的 FGuid（16 bytes，可为 None）
    )

    def __init__(self):
        self.name: str = 'None'
        self.type_name: str = 'None'
        self.length: int = 0
        self.array_index: int = 0
        self.property_guid: Optional[bytes] = None
        self.bool_val: bool = False
        self.tag_flags: int = 0
        # 经典格式
        self.struct_type: str = 'None'   # StructProperty 的内部类型
        self.enum_type: str = 'None'     # EnumProperty / ByteProperty 枚举类型
        self.inner_type: str = 'None'    # ArrayProperty 内部元素类型
        self.value_type: str = 'None'    # MapProperty value 类型（key_type = inner_type）
        self.struct_guid: Optional[bytes] = None  # StructProperty 的 StructGuid

    @staticmethod
    def read(reader: 'BinaryReader', asset: 'UAsset') -> Optional['PropertyTag']:
        """
        读取属性 tag。返回 None 表示 "None" 终止符（属性列表结束）。

        两种格式：
        A. PROPERTY_TAG_COMPLETE_TYPE_NAME (ue5 >= 13): 新格式
        B. 经典格式（大多数资产）
        """
        tag = PropertyTag()
        tag.name = _read_fname(reader, asset)

        if tag.name == 'None':
            return None

        use_new_format = (asset.object_version_ue5 >= PROPERTY_TAG_COMPLETE_TYPE_NAME)

        if use_new_format:
            # ── Mode A：新格式 ──────────────────────────────────
            type_name_struct = FPropertyTypeName(reader, asset)
            tag.type_name = type_name_struct.get_name()
            tag.length = reader.read_int32()
            tag.tag_flags = reader.read_uint8()

            # 解析 flags
            has_array_index = bool(tag.tag_flags & TAG_FLAG_HAS_ARRAY_INDEX)
            has_guid = bool(tag.tag_flags & TAG_FLAG_HAS_PROPERTY_GUID)
            has_extensions = bool(tag.tag_flags & TAG_FLAG_HAS_PROPERTY_EXTENSIONS)
            tag.bool_val = bool(tag.tag_flags & TAG_FLAG_BOOL_TRUE)

            if has_array_index:
                tag.array_index = reader.read_int32()
            if has_extensions:
                _ext_type = reader.read_uint8()  # 扩展类型（暂不解析细节）
                if asset.object_version_ue5 >= PROPERTY_TAG_EXTENSION_AND_OVERRIDABLE_SERIALIZATION:
                    pass  # 已在 extension type byte 里处理
            if has_guid:
                tag.property_guid = bytes(reader.read_bytes(16))

            # 从类型参数中提取常用子类型信息
            _extract_type_params_new(tag, type_name_struct)

        else:
            # ── Mode B：经典格式 ──────────────────────────────────
            tag.type_name = _read_fname(reader, asset)
            tag.length = reader.read_int32()
            tag.array_index = reader.read_int32()

            _read_classic_type_extras(reader, asset, tag)

        return tag


def _extract_type_params_new(tag: PropertyTag, tn: FPropertyTypeName):
    """从新格式 FPropertyTypeName 提取子类型参数。"""
    t = tag.type_name
    p0 = tn.get_param(0)
    p1 = tn.get_param(1)
    if t == 'StructProperty' and p0:
        tag.struct_type = p0.get_name()
    elif t in ('EnumProperty', 'ByteProperty') and p0:
        tag.enum_type = p0.get_name()
    elif t == 'ArrayProperty' and p0:
        tag.inner_type = p0.get_name()
    elif t == 'SetProperty' and p0:
        tag.inner_type = p0.get_name()
    elif t == 'MapProperty':
        if p0:
            tag.inner_type = p0.get_name()
        if p1:
            tag.value_type = p1.get_name()


def _read_classic_type_extras(reader: 'BinaryReader', asset: 'UAsset', tag: PropertyTag):
    """
    经典格式下，根据属性类型读取额外的类型信息。
    每种属性类型有不同的额外字段（struct type / enum type / inner type 等）。
    """
    t = tag.type_name

    if t == 'StructProperty':
        tag.struct_type = _read_fname(reader, asset)
        # struct guid（ver >= VER_UE4_STRUCT_GUID_IN_PROPERTY_TAG=503）：
        # 注意：这里始终读 16 bytes（FGuid），不是 bool + optional！
        # UE 源码：Ar << Tag.StructGuid（无条件序列化）
        if asset.object_version >= VER_UE4_PROPERTY_GUID_IN_PROPERTY_TAG:
            tag.struct_guid = bytes(reader.read_bytes(16))  # 保存 FGuid

    elif t == 'BoolProperty':
        tag.bool_val = bool(reader.read_uint8())

    elif t == 'ByteProperty':
        tag.enum_type = _read_fname(reader, asset)

    elif t == 'EnumProperty':
        tag.enum_type = _read_fname(reader, asset)

    elif t == 'ArrayProperty':
        if asset.object_version >= VER_UE4_PROPERTY_GUID_IN_PROPERTY_TAG:  # >= 503
            tag.inner_type = _read_fname(reader, asset)

    elif t == 'SetProperty':
        if asset.object_version >= VER_UE4_PROPERTY_GUID_IN_PROPERTY_TAG:
            tag.inner_type = _read_fname(reader, asset)

    elif t == 'MapProperty':
        if asset.object_version >= VER_UE4_PROPERTY_GUID_IN_PROPERTY_TAG:
            tag.inner_type = _read_fname(reader, asset)   # key type
            tag.value_type = _read_fname(reader, asset)   # value type

    # 所有类型的 property guid（经典格式）
    if asset.object_version >= VER_UE4_PROPERTY_GUID_IN_PROPERTY_TAG:
        has_guid = reader.read_uint8()
        if has_guid:
            tag.property_guid = bytes(reader.read_bytes(16))


class PropertyData:
    """
    所有属性数据类的基类。

    设计选择：
    - __slots__ 减少每实例内存
    - PROPERTY_TYPE class var 供注册表使用
    - read_value() 由子类重写，处理实际的二进制解码
    - to_json() 输出兼容 UAssetAPI 格式（含 $type）
    """
    __slots__ = (
        'name', 'array_index', 'property_guid', 'is_zero',
        'property_tag_flags', 'property_type_name', 'offset',
        'ancestry',
    )

    PROPERTY_TYPE: str = 'Unknown'

    def __init__(self):
        self.name: str = ''
        self.array_index: int = 0
        self.property_guid: Optional[bytes] = None
        self.is_zero: bool = False
        self.property_tag_flags: int = 0
        self.property_type_name: str = self.PROPERTY_TYPE
        self.offset: int = 0
        self.ancestry: str = ''

    def read_value(self, reader: 'BinaryReader', include_header: bool,
                   tag: Optional[PropertyTag], leng: int, asset: 'UAsset'):
        """子类重写：从 reader 读取实际属性值。"""
        raise NotImplementedError(f"{self.__class__.__name__}.read_value()")

    def _csharp_type(self) -> str:
        cls_name = self.__class__.__name__
        return CSHARP_TYPE_MAP.get(cls_name, f'UAssetAPI.PropertyTypes.Objects.{cls_name}, UAssetAPI')

    def _base_json(self) -> dict:
        """构建基础 JSON 字典（所有属性共有字段）。"""
        guid_str = None
        if self.property_guid:
            import struct
            a, b, c, d = struct.unpack_from('<4I', self.property_guid)
            guid_str = f"{a:08X}{b:08X}{c:08X}{d:08X}"
        return {
            '$type': self._csharp_type(),
            'Name': self.name,
            'ArrayIndex': self.array_index,
            'IsZero': self.is_zero,
            'PropertyGuid': guid_str,
            'Ancestry': {'$type': 'UAssetAPI.PropertyTypes.Objects.AncestryInfo, UAssetAPI',
                         'Lineage': []},
        }

    def to_json(self, asset: 'UAsset') -> dict:
        """输出 JSON（子类应 super() 后添加 Value 字段）。"""
        return self._base_json()


def read_property(reader: 'BinaryReader', asset: 'UAsset',
                  include_header: bool = True,
                  force_type: Optional[str] = None,
                  force_struct_type: Optional[str] = None) -> Optional[PropertyData]:
    """
    统一属性读取入口。

    include_header=True（默认）：读取完整 tag（属性名+类型+长度）后读值
    include_header=False：直接读值（用于 Array/Map 内部元素）

    force_type: 强制类型（用于 Array/Map 内部元素，绕过 tag 读取）
    force_struct_type: StructProperty 的 struct 类型（Array<Struct> 场景）
    """
    from .registry import get_property_class

    tag: Optional[PropertyTag] = None

    if include_header:
        start_pos = reader.position
        tag = PropertyTag.read(reader, asset)
        if tag is None:
            return None  # "None" terminator
        type_name = tag.type_name
        leng = tag.length
    else:
        type_name = force_type or 'UnknownProperty'
        leng = 0

    cls = get_property_class(type_name)
    if cls is None:
        # 未知类型：尝试跳过（有 length 信息时）
        if include_header and leng > 0:
            reader.skip(leng)
            return None
        raise ValueError(f"Unknown property type: {type_name}")

    prop = cls()
    prop.name = tag.name if tag else ''
    prop.array_index = tag.array_index if tag else 0
    prop.property_guid = tag.property_guid if tag else None
    prop.is_zero = False
    prop.offset = reader.position

    # 注入额外信息（tag 扩展字段）
    if tag and type_name == 'StructProperty':
        if force_struct_type:
            prop._struct_type = force_struct_type  # type: ignore
        else:
            prop._struct_type = tag.struct_type    # type: ignore
        prop._struct_guid = getattr(tag, 'struct_guid', None)  # type: ignore
    elif tag and type_name in ('EnumProperty', 'ByteProperty'):
        prop._enum_type = tag.enum_type  # type: ignore
    elif tag and type_name == 'ArrayProperty':
        prop._inner_type = tag.inner_type  # type: ignore
    elif tag and type_name == 'SetProperty':
        prop._inner_type = tag.inner_type  # type: ignore
    elif tag and type_name == 'MapProperty':
        prop._inner_type = tag.inner_type  # type: ignore
        prop._value_type = tag.value_type  # type: ignore

    # BoolProperty 特殊处理：值存在 tag flags 里（不需要读 body）
    if type_name == 'BoolProperty' and include_header and tag is not None:
        prop.read_value(reader, include_header=True, tag=tag, leng=leng, asset=asset)
    else:
        prop.read_value(reader, include_header=include_header, tag=tag, leng=leng, asset=asset)

    return prop


def read_property_tag_only(reader: 'BinaryReader', asset: 'UAsset') -> Optional[PropertyTag]:
    """只读 tag 不读值（用于 unversioned 属性解析的预处理）。"""
    return PropertyTag.read(reader, asset)
