# properties/structs/engine.py
# 引擎特定 struct 类型：RichCurveKey / SmartName 等

from __future__ import annotations
from typing import TYPE_CHECKING, Optional

from ..base import PropertyData, CSHARP_TYPE_MAP
from ..registry import register

if TYPE_CHECKING:
    from ...core.reader import BinaryReader
    from ...core.uasset import UAsset


def _read_fname(reader: 'BinaryReader', asset: 'UAsset') -> str:
    idx, num = reader.read_fname_raw()
    return asset._resolve_fname(idx, num)


# ERichCurveInterpMode
RCIM_Linear    = 0
RCIM_Constant  = 1
RCIM_Cubic     = 2
RCIM_None      = 255

# ERichCurveTangentMode
RCTM_Auto      = 0
RCTM_User      = 1
RCTM_Break     = 2
RCTM_None      = 255

# ERichCurveTangentWeightMode
RCTWM_WeightedNone    = 0
RCTWM_WeightedArrive  = 1
RCTWM_WeightedLeave   = 2
RCTWM_WeightedBoth    = 3


@register("RichCurveKeyProperty")
class RichCurveKeyPropertyData(PropertyData):
    """
    FRichCurveKey。
    格式：InterpMode(uint8) + TangentMode(uint8) + TangentWeightMode(uint8)
          + Time(float) + Value(float)
          + ArriveTangent(float) + ArriveTangentWeight(float)
          + LeaveTangent(float) + LeaveTangentWeight(float)
    """
    __slots__ = (
        'interp_mode', 'tangent_mode', 'tangent_weight_mode',
        'time', 'value',
        'arrive_tangent', 'arrive_tangent_weight',
        'leave_tangent', 'leave_tangent_weight',
    )
    PROPERTY_TYPE = "RichCurveKeyProperty"

    def __init__(self):
        super().__init__()
        self.interp_mode: int = RCIM_Linear
        self.tangent_mode: int = RCTM_Auto
        self.tangent_weight_mode: int = RCTWM_WeightedNone
        self.time: float = 0.0
        self.value: float = 0.0
        self.arrive_tangent: float = 0.0
        self.arrive_tangent_weight: float = 0.0
        self.leave_tangent: float = 0.0
        self.leave_tangent_weight: float = 0.0

    def read_value(self, reader, include_header, tag, leng, asset):
        self.interp_mode = reader.read_uint8()
        self.tangent_mode = reader.read_uint8()
        self.tangent_weight_mode = reader.read_uint8()
        self.time = reader.read_float()
        self.value = reader.read_float()
        self.arrive_tangent = reader.read_float()
        self.arrive_tangent_weight = reader.read_float()
        self.leave_tangent = reader.read_float()
        self.leave_tangent_weight = reader.read_float()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['$type'] = CSHARP_TYPE_MAP.get('RichCurveKeyPropertyData', d['$type'])
        d['Value'] = {
            'InterpMode': self.interp_mode,
            'TangentMode': self.tangent_mode,
            'TangentWeightMode': self.tangent_weight_mode,
            'Time': self.time,
            'Value': self.value,
            'ArriveTangent': self.arrive_tangent,
            'ArriveTangentWeight': self.arrive_tangent_weight,
            'LeaveTangent': self.leave_tangent,
            'LeaveTangentWeight': self.leave_tangent_weight,
        }
        return d


@register("SmartNameProperty")
class SmartNamePropertyData(PropertyData):
    """FSmartName：DisplayName(FName) + UID(uint16) + MappingIndex(uint16)"""
    __slots__ = ('display_name', 'uid', 'mapping_index')
    PROPERTY_TYPE = "SmartNameProperty"

    def __init__(self):
        super().__init__()
        self.display_name: str = 'None'
        self.uid: int = 0
        self.mapping_index: int = 0

    def read_value(self, reader, include_header, tag, leng, asset):
        self.display_name = _read_fname(reader, asset)
        self.uid = reader.read_uint16()
        self.mapping_index = reader.read_uint16()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = {
            'DisplayName': self.display_name,
            'UID': self.uid,
            'MappingIndex': self.mapping_index,
        }
        return d
