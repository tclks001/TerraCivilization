# properties/set_property.py
# SetPropertyData - UE TSet 序列化（类似 Array，但有额外的 deleted count）

from __future__ import annotations
from typing import TYPE_CHECKING, List, Any

from .base import PropertyData, read_property
from .registry import register

if TYPE_CHECKING:
    from ..core.reader import BinaryReader
    from ..core.uasset import UAsset


@register("SetProperty")
class SetPropertyData(PropertyData):
    """
    TSet 属性。格式与 Map 的 key 数组部分类似：
    删除条目数(=0) + 元素数 + 元素列表。
    """
    __slots__ = ('values', 'inner_type', '_inner_type')
    PROPERTY_TYPE = "SetProperty"

    def __init__(self):
        super().__init__()
        self.values: List[Any] = []
        self.inner_type: str = 'None'
        self._inner_type: str = 'None'

    def read_value(self, reader, include_header, tag, leng, asset):
        self.inner_type = getattr(self, '_inner_type', 'None') or 'None'

        # 跳过已删除条目（固定为 0）
        _deleted = reader.read_int32()

        count = reader.read_int32()
        self.values = []

        inner_t = self.inner_type
        for _ in range(count):
            if inner_t == 'StructProperty':
                from .struct_property import StructPropertyData
                sp = StructPropertyData()
                sp.name = 'Set_Element'
                sp._struct_type = 'None'
                sp.read_value(reader, include_header=False, tag=None, leng=0, asset=asset)
                self.values.append(sp)
            else:
                prop = read_property(reader, asset, include_header=False, force_type=inner_t)
                self.values.append(prop)

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
