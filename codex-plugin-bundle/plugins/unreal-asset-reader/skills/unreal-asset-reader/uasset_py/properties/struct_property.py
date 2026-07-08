# properties/struct_property.py
# StructPropertyData - 支持 custom struct（直接读） + NTPL（属性列表）两种模式
#
# 设计说明：
# StructProperty 的序列化有两种路径：
# 1. Custom Struct Serialization：特定 struct 类型（Vector/Color/Guid等）
#    有自定义的紧凑二进制格式，直接读取固定字节。
# 2. NTPL（None-Terminated Property List）：通用 struct，用属性列表格式序列化，
#    以 "None" FName 终止，与 export data 格式相同。
#
# struct_type 来自：
# - versioned 资产的 PropertyTag.struct_type（从 tag 中读取）
# - unversioned 资产需要 .usmap schema（Phase 1 暂用 NTPL fallback）

from __future__ import annotations
from typing import TYPE_CHECKING, Optional, List, Any

from .base import PropertyData, CSHARP_TYPE_MAP, read_property
from .registry import register

if TYPE_CHECKING:
    from ..core.reader import BinaryReader
    from ..core.uasset import UAsset

# 有自定义序列化的 struct 类型名 → 对应注册的属性类型名
# 这些 struct 类型在 PropertyData 注册表中以 XxxProperty 的形式注册
CUSTOM_STRUCT_MAP: dict[str, str] = {
    'Vector':                   'VectorProperty',
    'Vector2D':                 'Vector2DProperty',
    'Vector4':                  'Vector4Property',
    'Rotator':                  'RotatorProperty',
    'Quat':                     'QuatProperty',
    'Quat4f':                   'Quat4fProperty',
    'LinearColor':              'LinearColorProperty',
    'Color':                    'ColorProperty',
    'Guid':                     'GuidProperty',
    'DateTime':                 'DateTimeProperty',
    'Timespan':                 'TimespanProperty',
    'IntPoint':                 'IntPointProperty',
    'Box':                      'BoxProperty',
    'Plane':                    'PlaneProperty',
    'Matrix':                   'MatrixProperty',
    'SoftObjectPath':           'SoftObjectPathProperty',
    'SoftClassPath':            'SoftObjectPathProperty',  # 共享实现
    'GameplayTagContainer':     'GameplayTagContainerProperty',
    'RichCurveKey':             'RichCurveKeyProperty',
    'SmartName':                'SmartNameProperty',
}


@register("StructProperty")
class StructPropertyData(PropertyData):
    """
    UE StructProperty。
    根据 struct_type 决定使用 custom 序列化还是 NTPL。
    """
    __slots__ = ('struct_type', '_struct_type', '_struct_guid', 'value', 'properties', '_parse_errors')
    PROPERTY_TYPE = "StructProperty"

    def __init__(self):
        super().__init__()
        self.struct_type: str = 'None'
        self._struct_type: str = 'None'   # 由 read_property 注入
        self._struct_guid: Optional[bytes] = None  # 由 read_property 注入
        self.value: Any = None             # custom struct 的值
        self.properties: List[PropertyData] = []  # NTPL 模式的属性列表
        self._parse_errors: List[str] = []

    def read_value(self, reader, include_header, tag, leng, asset):
        struct_t = getattr(self, '_struct_type', 'None') or 'None'
        self.struct_type = struct_t

        custom_type_name = CUSTOM_STRUCT_MAP.get(struct_t)

        if custom_type_name:
            self._read_custom(reader, custom_type_name, include_header, tag, leng, asset)
        else:
            self._read_ntpl(reader, leng, asset, include_header=include_header)

    def _read_custom(self, reader, type_name: str, include_header, tag, leng, asset):
        """委托给对应的 custom struct 类读取。"""
        from .registry import get_property_class
        cls = get_property_class(type_name)
        if cls is None:
            # 找不到 custom 类 → 回退 NTPL
            self._read_ntpl(reader, leng, asset, include_header=include_header)
            return
        prop = cls()
        prop.name = self.name
        prop.read_value(reader, include_header=False, tag=None, leng=leng, asset=asset)
        self.value = prop

    def _read_ntpl(self, reader, leng, asset, include_header: bool = False):
        """
        读取 NTPL 属性列表（None-Terminated Property List）。

        边界逻辑：
        - include_header=True 且 leng==0：空 struct，直接跳过（0 次循环）
        - include_header=True 且 leng>0：读到 None 终止符，leng 作为超出保护
        - include_header=False 且 leng>0：array 内部 struct 元素，leng=element_size 作为边界
        - leng=0 且 include_header=False：不限制（正常 NTPL）

        设计原则：
        - 优先依赖 None 终止符（NTPL 的标准语义）
        - leng 作为防御性上界，避免解析出轨时越过 export 范围
        """
        start = reader.position

        # 空 struct 特殊处理：length=0 且 include_header=True → 跳过内容
        if include_header and leng == 0:
            return

        # 确定防御性上界
        if leng > 0:
            end_pos = start + leng
        else:
            end_pos = -1

        self.properties = []
        while True:
            if end_pos > 0 and reader.position >= end_pos:
                break
            try:
                prop = read_property(reader, asset, include_header=True)
                if prop is None:
                    break
                self.properties.append(prop)
            except Exception as e:
                self._parse_errors.append(str(e))
                break

    def to_json(self, asset) -> dict:
        import struct as _struct
        d = self._base_json()
        d['StructType'] = self.struct_type

        # StructGUID：格式 "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}"
        guid_bytes = getattr(self, '_struct_guid', None)
        if guid_bytes and len(guid_bytes) == 16:
            # UE FGuid 存储顺序：A(uint32 LE), B(uint32 LE), C(uint32 LE), D(uint32 LE)
            a, b, c, d_val = _struct.unpack_from('<4I', guid_bytes)
            # 格式：{AABBCCDD-EEFF-GGHH-IIJJ-KKLLMMNNOOPP}
            # 实际按 UE 的 ToString() 方式：%08X-%04X-%04X-%04X-%012llX
            b1 = (b >> 16) & 0xFFFF
            b2 = b & 0xFFFF
            b3 = (c >> 16) & 0xFFFF
            b4 = c & 0xFFFF
            b5_hi = d_val >> 16
            b5_lo = d_val & 0xFFFF
            guid_str = f"{{{a:08X}-{b1:04X}-{b2:04X}-{b3:04X}-{b4:04X}{b5_hi:04X}{b5_lo:04X}}}"
        else:
            guid_str = "{00000000-0000-0000-0000-000000000000}"
        d['StructGUID'] = guid_str
        d['SerializeNone'] = True

        if self.value is not None:
            # Custom struct：输出 inner value 的 JSON
            if hasattr(self.value, 'to_json'):
                inner = self.value.to_json(asset)
                # 对于 custom struct，我们把 value 的内容合并进来
                # 保留 $type 为 StructPropertyData
                d['Value'] = inner
            else:
                d['Value'] = self.value
        else:
            # NTPL：输出属性数组
            d['Value'] = [p.to_json(asset) for p in self.properties]

        if self._parse_errors:
            d['_ParseErrors'] = self._parse_errors

        return d
