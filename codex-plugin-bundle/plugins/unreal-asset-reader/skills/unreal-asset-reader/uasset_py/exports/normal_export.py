# exports/normal_export.py
# NormalExport - 解析带属性列表的 export body
#
# 设计说明：
# NormalExport 是最常见的 export 类型（DataTable行、Blueprint组件等）。
# 主要读取流程：
# 1. 版本相关前置读取（SerializationControl 等）
# 2. 循环读取属性（read_property）直到遇到 "None" 终止符
# 3. 可选读取 object GUID（仅非 CDO 对象）
#
# 容错：任何单属性解析失败 → 跳过并记录，不影响其他属性

from __future__ import annotations
from typing import TYPE_CHECKING, List, Optional, Any

from .export import BaseExport, RawExport

if TYPE_CHECKING:
    from ..core.reader import BinaryReader
    from ..core.uasset import UAsset
    from ..core.import_export import FObjectExport

from ..properties.base import PropertyData, read_property

# ObjectFlags
RF_ClassDefaultObject = 0x10

# UE5 版本常量（直接使用数字避免循环导入风险）
# ObjectVersionUE5 序号（raw - 999）
_PROPERTY_TAG_EXTENSION_AND_OVERRIDABLE_SERIALIZATION = 12  # raw=1011

# EOverridableSerializationControlEnum
SC_OVERRIDABLE_INFORMATION = 1
SC_BEGIN_OVERRIDE = 2
SC_END_OVERRIDE = 3


class NormalExport(BaseExport):
    """
    带属性列表的 export。
    对应 C# UAssetAPI.ExportTypes.NormalExport。
    """
    __slots__ = ('_header', 'properties', 'object_guid', '_parse_errors', '_raw_data')

    def __init__(self, header: 'FObjectExport'):
        super().__init__(header)
        self.properties: List[PropertyData] = []
        self.object_guid: Optional[bytes] = None
        self._parse_errors: List[str] = []
        self._raw_data: Optional[bytes] = None

    def read(self, reader: 'BinaryReader', asset: 'UAsset', next_starting: int = -1):
        """
        解析 export data body。

        next_starting: 下一个 export 的起始偏移（用于边界检查）。
        """
        try:
            self._do_read(reader, asset, next_starting)
        except Exception as e:
            self._parse_errors.append(f"NormalExport read failed: {e}")

    def _do_read(self, reader: 'BinaryReader', asset: 'UAsset', next_starting: int):
        has_unversioned = bool(asset.package_flags & 0x2000)  # PKG_UnversionedProperties

        if not has_unversioned:
            # ── Versioned：可能有 SerializationControl 字节 ──
            if asset.object_version_ue5 >= _PROPERTY_TAG_EXTENSION_AND_OVERRIDABLE_SERIALIZATION:
                serialization_control = reader.read_uint8()
                if serialization_control & SC_OVERRIDABLE_INFORMATION:
                    _operation = reader.read_uint8()  # 暂不使用

        # ── 循环读属性直到 None ──
        self.properties = []
        while True:
            # 边界检查：防止读超 export 范围
            if next_starting > 0 and reader.position >= next_starting:
                break

            try:
                prop = read_property(reader, asset, include_header=True)
                if prop is None:
                    break
                self.properties.append(prop)
            except Exception as e:
                self._parse_errors.append(f"Property parse error at pos {reader.position}: {e}")
                break

        # ── 可选读取 ObjectGuid（非 CDO 对象）──
        # 注：UE 源码 UObject::Serialize 中 bNetGUID / ObjectGuid 用 bool32（4字节）而不是 uint8
        # C# UAssetAPI NormalExport.Read() 中对应: bool bHasGuid = reader.ReadBoolean() (4字节)
        is_cdo = bool(self._header.object_flags & RF_ClassDefaultObject)
        if not is_cdo:
            try:
                if reader.remaining() >= 4 and (next_starting <= 0 or reader.position < next_starting):
                    has_guid = reader.read_bool32()  # bool32（4字节）
                    if has_guid and reader.remaining() >= 16:
                        self.object_guid = bytes(reader.read_bytes(16))
            except Exception:
                pass

    def to_dict(self) -> dict:
        return {
            'Index': self._header.index,
            'ObjectName': self._header.object_name,
            'ExportType': 'NormalExport',
            'PropertyCount': len(self.properties),
        }

    def to_json(self, asset: 'UAsset') -> dict:
        from ..properties.base import CSHARP_TYPE_MAP

        props_json = []
        for p in self.properties:
            try:
                props_json.append(p.to_json(asset))
            except Exception as e:
                props_json.append({'_error': str(e), '_type': type(p).__name__})

        result = {
            '$type': CSHARP_TYPE_MAP['NormalExport'],
            'Data': props_json,
            'ObjectName': self._header.object_name,
            'ClassIndex': self._header.class_index,
            'SuperIndex': self._header.super_index,
            'TemplateIndex': self._header.template_index,
            'OuterIndex': self._header.outer_index,
            'ObjectFlags': self._header.object_flags,
            'SerialSize': self._header.serial_size,
            'SerialOffset': self._header.serial_offset,
            'ForcedExport': self._header.forced_export,
            'NotForClient': self._header.not_for_client,
            'NotForServer': self._header.not_for_server,
            'PackageFlags': self._header.package_flags,
            'IsAsset': getattr(self._header, 'is_asset', False),
        }
        if self.object_guid:
            import struct
            a, b, c, d = struct.unpack_from('<4I', self.object_guid)
            result['ObjectGuid'] = f"{a:08X}{b:08X}{c:08X}{d:08X}"
        else:
            result['ObjectGuid'] = None

        if self._parse_errors:
            result['_ParseErrors'] = self._parse_errors

        return result
