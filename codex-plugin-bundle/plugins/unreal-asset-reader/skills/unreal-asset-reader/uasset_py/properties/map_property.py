# properties/map_property.py
# MapPropertyData - UE TMap 序列化

from __future__ import annotations
from typing import TYPE_CHECKING, Optional, List, Any

from .base import PropertyData, read_property
from .registry import register

if TYPE_CHECKING:
    from ..core.reader import BinaryReader
    from ..core.uasset import UAsset


@register("MapProperty")
class MapPropertyData(PropertyData):
    """
    TMap 属性。
    读取：(跳过删除项数) + count × (key + value) 对。
    key/value 的类型来自 tag.inner_type / tag.value_type。
    """
    __slots__ = ('entries', 'key_type', 'value_type', '_inner_type', '_value_type')
    PROPERTY_TYPE = "MapProperty"

    def __init__(self):
        super().__init__()
        self.entries: List[dict] = []
        self.key_type: str = 'None'
        self.value_type: str = 'None'
        self._inner_type: str = 'None'
        self._value_type: str = 'None'

    def read_value(self, reader, include_header, tag, leng, asset):
        self.key_type = getattr(self, '_inner_type', 'None') or 'None'
        self.value_type = getattr(self, '_value_type', 'None') or 'None'

        # 跳过已删除的条目计数（=0，占 4 bytes）
        _deleted_count = reader.read_int32()

        count = reader.read_int32()
        self.entries = []

        for _ in range(count):
            key_prop = self._read_element(reader, asset, self.key_type, 'StructProperty_Map_Key')
            val_prop = self._read_element(reader, asset, self.value_type, 'StructProperty_Map_Value')
            self.entries.append({'Key': key_prop, 'Value': val_prop})

    def _read_element(self, reader, asset, type_name: str, struct_hint: str):
        """读取单个 map 元素（key 或 value），不带 tag。"""
        if type_name in ('None', '', 'UnknownProperty'):
            return None
        if type_name == 'StructProperty':
            from .struct_property import StructPropertyData
            sp = StructPropertyData()
            sp.name = struct_hint
            sp._struct_type = 'None'  # 无法知道具体类型
            sp.read_value(reader, include_header=False, tag=None, leng=0, asset=asset)
            return sp
        return read_property(reader, asset, include_header=False, force_type=type_name)

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['KeyType'] = self.key_type
        d['ValueType'] = self.value_type

        entries_json = []
        for entry in self.entries:
            k = entry['Key']
            v = entry['Value']
            entries_json.append({
                'Key': k.to_json(asset) if hasattr(k, 'to_json') else k,
                'Value': v.to_json(asset) if hasattr(v, 'to_json') else v,
            })
        d['Value'] = entries_json
        return d
