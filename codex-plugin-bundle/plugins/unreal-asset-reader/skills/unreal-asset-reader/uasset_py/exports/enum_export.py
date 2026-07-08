# exports/enum_export.py
# EnumExport - 解析 UE UEnum 资产（UserDefinedEnum 等）
#
# 设计说明：
# UEnum export body = NormalExport属性列表 + UEnum二进制数据
#
# UEnum 二进制数据随 object_version 有三种格式：
#   A. 旧格式（< VER_UE4_TIGHTLY_PACKED_ENUMS=474）：
#      n枚举项，value 自动递增（不存储在文件中）
#   B. 中间格式（>= 474 但 FCoreObjectVersion.EnumProperties 未到达）：
#      n枚举项，每项存 FName + int8 value
#   C. 新格式（FCoreObjectVersion.EnumProperties 版本起）：
#      n枚举项，每项存 FName + int64 value
#
# CppForm 枚举：
#   Regular = 0，Namespaced = 1，EnumClass = 2
#
# Phase 2 容错策略：解析失败时存储原始字节，不影响其他 export。

from __future__ import annotations
from typing import TYPE_CHECKING, List, Optional, Tuple

from .normal_export import NormalExport
from ..core.types import FGuid

if TYPE_CHECKING:
    from ..core.reader import BinaryReader
    from ..core.uasset import UAsset
    from ..core.import_export import FObjectExport

# UE4 版本常量
VER_UE4_TIGHTLY_PACKED_ENUMS = 474
VER_UE4_ENUM_CLASS_SUPPORT = 474

# FCoreObjectVersion GUID（用于查找 custom_version）
# {9F8BF812-FC4A-8CF7-D4D0-0C837E928BDA}
_CORE_OBJECT_VERSION_GUID = FGuid(0x9F8BF812, 0xFC4A8CF7, 0xD4D00C83, 0x7E928BDA)

# FCoreObjectVersion.EnumProperties = 6
_ENUM_PROPERTIES_VERSION = 6

# CppForm 枚举
ENUM_CPP_FORM_REGULAR = 0
ENUM_CPP_FORM_NAMESPACED = 1
ENUM_CPP_FORM_ENUM_CLASS = 2


def _get_core_object_version(asset: 'UAsset') -> int:
    """查询 FCoreObjectVersion 在 asset custom_versions 中的值。"""
    for cv in asset.custom_versions:
        g = cv.key
        if (g.a == _CORE_OBJECT_VERSION_GUID.a and
                g.b == _CORE_OBJECT_VERSION_GUID.b and
                g.c == _CORE_OBJECT_VERSION_GUID.c and
                g.d == _CORE_OBJECT_VERSION_GUID.d):
            return cv.version
    return 0


class EnumExport(NormalExport):
    """
    UEnum export 解析器。

    enum_names: [(name, value), ...]
    cpp_form: int (ENUM_CPP_FORM_xxx 常量)
    """
    __slots__ = ('_header', 'properties', 'object_guid', '_parse_errors', '_raw_data',
                 'enum_names', 'cpp_form')

    def __init__(self, header: 'FObjectExport'):
        super().__init__(header)
        self.enum_names: List[Tuple[str, int]] = []
        self.cpp_form: int = ENUM_CPP_FORM_REGULAR

    def read(self, reader: 'BinaryReader', asset: 'UAsset', next_starting: int = -1):
        try:
            # Step 1: 读取 NormalExport 属性列表
            super()._do_read(reader, asset, next_starting)

            # Step 2: 读取 UEnum 二进制数据
            self._read_uenum(reader, asset)

        except Exception as e:
            self._parse_errors.append(f"EnumExport read failed: {e}")

    def _read_uenum(self, reader: 'BinaryReader', asset: 'UAsset'):
        obj_ver = asset.object_version
        core_ver = _get_core_object_version(asset)

        n = reader.read_int32()

        if obj_ver < VER_UE4_TIGHTLY_PACKED_ENUMS:
            # 旧格式：value 自动递增
            for i in range(n):
                name = asset._read_fname(reader)
                self.enum_names.append((name, i))
        else:
            if core_ver >= _ENUM_PROPERTIES_VERSION:
                # 新格式：FName + int64
                for _ in range(n):
                    name = asset._read_fname(reader)
                    value = reader.read_int64()
                    self.enum_names.append((name, value))
            else:
                # 中间格式：FName + int8
                for _ in range(n):
                    name = asset._read_fname(reader)
                    value = reader.read_int8()
                    self.enum_names.append((name, int(value)))

        # 读取 CppForm
        if obj_ver >= VER_UE4_ENUM_CLASS_SUPPORT:
            self.cpp_form = reader.read_uint8()
        else:
            # 旧版：读 int32，1 = Namespaced，其他 = Regular
            cpp_form_int = reader.read_int32()
            self.cpp_form = ENUM_CPP_FORM_NAMESPACED if cpp_form_int == 1 else ENUM_CPP_FORM_REGULAR

    def to_json(self, asset: 'UAsset') -> dict:
        d = super().to_json(asset)
        d['$type'] = 'UAssetAPI.ExportTypes.EnumExport, UAssetAPI'
        d['Enum'] = {
            'Names': [{'Key': name, 'Value': value} for name, value in self.enum_names],
            'CppForm': self.cpp_form,
        }
        return d
