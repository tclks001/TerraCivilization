# properties/array_property.py
# ArrayPropertyData - UE TArray 序列化
#
# 设计说明：
# Array 的 inner type 来自 tag.inner_type（经典格式或新格式的类型参数）。
# StructProperty 内部元素需要特殊处理：
#   - 数组头有 StructType 字段（和单独的 StructPropertyData 不同）
#   - 每个元素直接以 NTPL 或 custom struct 形式读取（无 tag）

from __future__ import annotations
from typing import TYPE_CHECKING, Optional, List, Any

from .base import PropertyData, read_property
from .registry import register

if TYPE_CHECKING:
    from ..core.reader import BinaryReader
    from ..core.uasset import UAsset


def _read_fname(reader: 'BinaryReader', asset: 'UAsset') -> str:
    idx, num = reader.read_fname_raw()
    return asset._resolve_fname(idx, num)


@register("ArrayProperty")
class ArrayPropertyData(PropertyData):
    """
    TArray 属性。
    include_header 模式下先从 tag.inner_type 得到元素类型，
    然后读取 count 个元素（元素不带 tag）。
    """
    __slots__ = ('values', 'inner_type', '_inner_type')
    PROPERTY_TYPE = "ArrayProperty"

    def __init__(self):
        super().__init__()
        self.values: List[Any] = []
        self.inner_type: str = 'None'
        self._inner_type: str = 'None'  # 从 read_property 注入

    def read_value(self, reader, include_header, tag, leng, asset):
        inner_t = getattr(self, '_inner_type', None) or 'None'
        self.inner_type = inner_t

        count = reader.read_int32()

        self.values = []

        if inner_t == 'StructProperty':
            # 注意：即使 count=0，struct array header 也要被读（始终序列化）
            self._read_struct_array(reader, count, asset)
        elif count <= 0:
            self.values = []
        elif inner_t in ('None', '', 'UnknownProperty'):
            # 无法确定元素类型，作为 bytes 读取
            self.values = list(range(count))  # placeholder
        else:
            for _ in range(count):
                prop = read_property(
                    reader, asset,
                    include_header=False,
                    force_type=inner_t
                )
                if prop is not None:
                    self.values.append(prop)
                else:
                    self.values.append(None)

    def _read_struct_array(self, reader, count: int, asset: 'UAsset'):
        """
        StructProperty 数组元数据头格式（从 UE 源码分析）：
          FName PropertyName   (8 bytes)
          FName "StructProperty" (8 bytes)
          int32 ElementSize   (4 bytes)
          int32 ArrayIndex=0  (4 bytes)
          FName StructTypeName (8 bytes)
          FGuid StructGuid    (16 bytes)
          uint8 HasPropertyGuid (1 byte)
          [16 bytes PropertyGuid if HasPropertyGuid]

        然后 count 个 struct body（NTPL 或 custom struct）。
        """
        from .struct_property import StructPropertyData

        # 读 PropertyName（用于元素名称）
        struct_prop_name = _read_fname(reader, asset)
        # 读 "StructProperty" 类型名（忽略）
        _struct_prop_type = _read_fname(reader, asset)
        # int32 ElementSize
        _element_size = reader.read_int32()
        # int32 ArrayIndex
        _array_index = reader.read_int32()
        # FName StructTypeName（重要：用于 NTPL vs custom 判断）
        struct_type_name = _read_fname(reader, asset)
        # FGuid StructGuid (16 bytes)
        reader.skip(16)
        # uint8 HasPropertyGuid
        has_guid = reader.read_uint8()
        if has_guid:
            reader.skip(16)  # PropertyGuid

        # 每个元素
        for _ in range(count):
            sp = StructPropertyData()
            sp.name = struct_prop_name
            sp._struct_type = struct_type_name  # type: ignore
            # 传入 element_size 作为 NTPL 边界保护
            sp.read_value(reader, include_header=False, tag=None, leng=_element_size, asset=asset)
            self.values.append(sp)

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['ArrayType'] = self.inner_type
        vals = []
        for v in self.values:
            if hasattr(v, 'to_json'):
                vals.append(v.to_json(asset))
            else:
                vals.append(v)
        d['Value'] = vals
        return d
