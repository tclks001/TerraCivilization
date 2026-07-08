# properties/structs/core.py
# 核心 struct 类型：Color / DateTime / Guid / Timespan

from __future__ import annotations
from typing import TYPE_CHECKING
import struct as _struct

from ..base import PropertyData, CSHARP_TYPE_MAP
from ..registry import register

if TYPE_CHECKING:
    from ...core.reader import BinaryReader
    from ...core.uasset import UAsset


# ── Color ─────────────────────────────────────────────────────

@register("ColorProperty")
class ColorPropertyData(PropertyData):
    """FColor (B, G, R, A) - uint8 x4，注意顺序是 B G R A"""
    __slots__ = ('b', 'g', 'r', 'a')
    PROPERTY_TYPE = "ColorProperty"

    def __init__(self):
        super().__init__()
        self.b: int = 0
        self.g: int = 0
        self.r: int = 0
        self.a: int = 255

    def read_value(self, reader, include_header, tag, leng, asset):
        self.b = reader.read_uint8()
        self.g = reader.read_uint8()
        self.r = reader.read_uint8()
        self.a = reader.read_uint8()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['$type'] = CSHARP_TYPE_MAP.get('ColorPropertyData', d['$type'])
        d['Value'] = {
            '$type': 'UAssetAPI.UnrealTypes.FColor, UAssetAPI',
            'B': self.b, 'G': self.g, 'R': self.r, 'A': self.a
        }
        return d


# ── Guid ──────────────────────────────────────────────────────

@register("GuidProperty")
class GuidPropertyData(PropertyData):
    """FGuid - 16 bytes (4 uint32 little-endian)"""
    __slots__ = ('a', 'b', 'c', 'd')
    PROPERTY_TYPE = "GuidProperty"

    def __init__(self):
        super().__init__()
        self.a: int = 0
        self.b: int = 0
        self.c: int = 0
        self.d: int = 0

    def read_value(self, reader, include_header, tag, leng, asset):
        vals = reader.read_guid()
        self.a, self.b, self.c, self.d = vals

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['$type'] = CSHARP_TYPE_MAP.get('GuidPropertyData', d['$type'])
        d['Value'] = f"{self.a:08X}{self.b:08X}{self.c:08X}{self.d:08X}"
        return d


# ── DateTime ──────────────────────────────────────────────────

@register("DateTimeProperty")
class DateTimePropertyData(PropertyData):
    """FDateTime - int64 ticks"""
    __slots__ = ('ticks',)
    PROPERTY_TYPE = "DateTimeProperty"

    def __init__(self):
        super().__init__()
        self.ticks: int = 0

    def read_value(self, reader, include_header, tag, leng, asset):
        self.ticks = reader.read_int64()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['$type'] = CSHARP_TYPE_MAP.get('DateTimePropertyData', d['$type'])
        d['Value'] = self.ticks
        return d


# ── Timespan ──────────────────────────────────────────────────

@register("TimespanProperty")
class TimespanPropertyData(PropertyData):
    """FTimespan - int64 ticks"""
    __slots__ = ('ticks',)
    PROPERTY_TYPE = "TimespanProperty"

    def __init__(self):
        super().__init__()
        self.ticks: int = 0

    def read_value(self, reader, include_header, tag, leng, asset):
        self.ticks = reader.read_int64()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['$type'] = CSHARP_TYPE_MAP.get('TimespanPropertyData', d['$type'])
        d['Value'] = self.ticks
        return d
