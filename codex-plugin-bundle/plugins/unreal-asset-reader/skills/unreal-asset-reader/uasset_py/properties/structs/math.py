# properties/structs/math.py
# 数学相关 struct：Vector / Rotator / Quat / LinearColor / Box / IntPoint / Plane 等
#
# 设计说明：
# UE5.0+ (ObjectVersionUE5 >= LARGE_WORLD_COORDINATES=5) 后，
# Vector/Rotator/Quat 使用 double 精度（3/4 doubles）。
# UE5.3 项目中默认 use_double=True，但需要兼容旧版本。

from __future__ import annotations
from typing import TYPE_CHECKING

from ..base import PropertyData, CSHARP_TYPE_MAP
from ..registry import register

if TYPE_CHECKING:
    from ...core.reader import BinaryReader
    from ...core.uasset import UAsset

from ...core.versions import LARGE_WORLD_COORDINATES


def _use_double(asset: 'UAsset') -> bool:
    """是否使用 double 精度（UE5 LWC）"""
    return asset.object_version_ue5 >= LARGE_WORLD_COORDINATES


# ── Vector ────────────────────────────────────────────────────

@register("VectorProperty")
class VectorPropertyData(PropertyData):
    """
    FVector。UE5+ 用 double（3 doubles），否则 float（3 floats）。
    注意：这是 custom struct，不通过 NTPL（属性列表）序列化。
    """
    __slots__ = ('x', 'y', 'z')
    PROPERTY_TYPE = "VectorProperty"

    def __init__(self):
        super().__init__()
        self.x: float = 0.0
        self.y: float = 0.0
        self.z: float = 0.0

    def read_value(self, reader, include_header, tag, leng, asset):
        if _use_double(asset):
            self.x = reader.read_double()
            self.y = reader.read_double()
            self.z = reader.read_double()
        else:
            self.x = reader.read_float()
            self.y = reader.read_float()
            self.z = reader.read_float()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['$type'] = CSHARP_TYPE_MAP.get('VectorPropertyData', d['$type'])
        d['Value'] = {'X': self.x, 'Y': self.y, 'Z': self.z}
        return d


# ── Rotator ────────────────────────────────────────────────────

@register("RotatorProperty")
class RotatorPropertyData(PropertyData):
    """FRotator (Pitch, Yaw, Roll)"""
    __slots__ = ('pitch', 'yaw', 'roll')
    PROPERTY_TYPE = "RotatorProperty"

    def __init__(self):
        super().__init__()
        self.pitch: float = 0.0
        self.yaw: float = 0.0
        self.roll: float = 0.0

    def read_value(self, reader, include_header, tag, leng, asset):
        if _use_double(asset):
            self.pitch = reader.read_double()
            self.yaw = reader.read_double()
            self.roll = reader.read_double()
        else:
            self.pitch = reader.read_float()
            self.yaw = reader.read_float()
            self.roll = reader.read_float()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['$type'] = CSHARP_TYPE_MAP.get('RotatorPropertyData', d['$type'])
        d['Value'] = {'Pitch': self.pitch, 'Yaw': self.yaw, 'Roll': self.roll}
        return d


# ── Quat ──────────────────────────────────────────────────────

@register("QuatProperty")
class QuatPropertyData(PropertyData):
    """FQuat (X, Y, Z, W)"""
    __slots__ = ('x', 'y', 'z', 'w')
    PROPERTY_TYPE = "QuatProperty"

    def __init__(self):
        super().__init__()
        self.x: float = 0.0
        self.y: float = 0.0
        self.z: float = 0.0
        self.w: float = 1.0

    def read_value(self, reader, include_header, tag, leng, asset):
        if _use_double(asset):
            self.x = reader.read_double()
            self.y = reader.read_double()
            self.z = reader.read_double()
            self.w = reader.read_double()
        else:
            self.x = reader.read_float()
            self.y = reader.read_float()
            self.z = reader.read_float()
            self.w = reader.read_float()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['$type'] = CSHARP_TYPE_MAP.get('QuatPropertyData', d['$type'])
        d['Value'] = {'X': self.x, 'Y': self.y, 'Z': self.z, 'W': self.w}
        return d


# UE5 使用 Quat4f 别名（仍注册 QuatProperty）
@register("Quat4fProperty")
class Quat4fPropertyData(QuatPropertyData):
    PROPERTY_TYPE = "Quat4fProperty"

    def read_value(self, reader, include_header, tag, leng, asset):
        # Quat4f 始终用 float（不受 LWC 影响）
        self.x = reader.read_float()
        self.y = reader.read_float()
        self.z = reader.read_float()
        self.w = reader.read_float()


# ── LinearColor ────────────────────────────────────────────────

@register("LinearColorProperty")
class LinearColorPropertyData(PropertyData):
    """FLinearColor (R, G, B, A) - 始终 float"""
    __slots__ = ('r', 'g', 'b', 'a')
    PROPERTY_TYPE = "LinearColorProperty"

    def __init__(self):
        super().__init__()
        self.r: float = 0.0
        self.g: float = 0.0
        self.b: float = 0.0
        self.a: float = 1.0

    def read_value(self, reader, include_header, tag, leng, asset):
        self.r = reader.read_float()
        self.g = reader.read_float()
        self.b = reader.read_float()
        self.a = reader.read_float()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['$type'] = CSHARP_TYPE_MAP.get('LinearColorPropertyData', d['$type'])
        d['Value'] = {'R': self.r, 'G': self.g, 'B': self.b, 'A': self.a}
        return d


# ── IntPoint ──────────────────────────────────────────────────

@register("IntPointProperty")
class IntPointPropertyData(PropertyData):
    """FIntPoint (X, Y) - int32"""
    __slots__ = ('x', 'y')
    PROPERTY_TYPE = "IntPointProperty"

    def __init__(self):
        super().__init__()
        self.x: int = 0
        self.y: int = 0

    def read_value(self, reader, include_header, tag, leng, asset):
        self.x = reader.read_int32()
        self.y = reader.read_int32()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['$type'] = CSHARP_TYPE_MAP.get('IntPointPropertyData', d['$type'])
        d['Value'] = {'X': self.x, 'Y': self.y}
        return d


# ── Box ───────────────────────────────────────────────────────

@register("BoxProperty")
class BoxPropertyData(PropertyData):
    """FBox (Min Vector + Max Vector + uint8 IsValid)"""
    __slots__ = ('min_x', 'min_y', 'min_z', 'max_x', 'max_y', 'max_z', 'is_valid')
    PROPERTY_TYPE = "BoxProperty"

    def __init__(self):
        super().__init__()
        self.min_x: float = 0.0
        self.min_y: float = 0.0
        self.min_z: float = 0.0
        self.max_x: float = 0.0
        self.max_y: float = 0.0
        self.max_z: float = 0.0
        self.is_valid: int = 0

    def read_value(self, reader, include_header, tag, leng, asset):
        use_d = _use_double(asset)
        if use_d:
            self.min_x = reader.read_double()
            self.min_y = reader.read_double()
            self.min_z = reader.read_double()
            self.max_x = reader.read_double()
            self.max_y = reader.read_double()
            self.max_z = reader.read_double()
        else:
            self.min_x = reader.read_float()
            self.min_y = reader.read_float()
            self.min_z = reader.read_float()
            self.max_x = reader.read_float()
            self.max_y = reader.read_float()
            self.max_z = reader.read_float()
        self.is_valid = reader.read_uint8()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['$type'] = CSHARP_TYPE_MAP.get('BoxPropertyData', d['$type'])
        d['Value'] = {
            'Min': {'X': self.min_x, 'Y': self.min_y, 'Z': self.min_z},
            'Max': {'X': self.max_x, 'Y': self.max_y, 'Z': self.max_z},
            'IsValid': self.is_valid,
        }
        return d


# ── Plane ─────────────────────────────────────────────────────

@register("PlaneProperty")
class PlanePropertyData(PropertyData):
    """FPlane4f (X, Y, Z, W) - float only"""
    __slots__ = ('x', 'y', 'z', 'w')
    PROPERTY_TYPE = "PlaneProperty"

    def __init__(self):
        super().__init__()
        self.x: float = 0.0
        self.y: float = 0.0
        self.z: float = 0.0
        self.w: float = 0.0

    def read_value(self, reader, include_header, tag, leng, asset):
        if _use_double(asset):
            self.x = reader.read_double()
            self.y = reader.read_double()
            self.z = reader.read_double()
            self.w = reader.read_double()
        else:
            self.x = reader.read_float()
            self.y = reader.read_float()
            self.z = reader.read_float()
            self.w = reader.read_float()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = {'X': self.x, 'Y': self.y, 'Z': self.z, 'W': self.w}
        return d


# ── Matrix ────────────────────────────────────────────────────

@register("MatrixProperty")
class MatrixPropertyData(PropertyData):
    """FMatrix - 4x4 float/double"""
    __slots__ = ('values',)
    PROPERTY_TYPE = "MatrixProperty"

    def __init__(self):
        super().__init__()
        self.values: list = [0.0] * 16

    def read_value(self, reader, include_header, tag, leng, asset):
        use_d = _use_double(asset)
        if use_d:
            self.values = [reader.read_double() for _ in range(16)]
        else:
            self.values = [reader.read_float() for _ in range(16)]

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['Value'] = self.values
        return d
