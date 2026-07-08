# properties/text_property.py
# TextPropertyData - UE FText 序列化，含多种 HistoryType 分支
#
# 设计说明：
# FText 的序列化格式在 UE4_FTEXT_HISTORY (479) 后发生根本变化。
# 主要实现 None/Base/RawText/StringTableEntry 4种，其余抛 NotImplementedError
# 并做容错跳过（避免 crash）。

from __future__ import annotations
from typing import TYPE_CHECKING, Optional, Any

from .base import PropertyData
from .registry import register

if TYPE_CHECKING:
    from ..core.reader import BinaryReader
    from ..core.uasset import UAsset

from ..core.versions import VER_UE4_FTEXT_HISTORY

# FText HistoryType 枚举值
TEXT_HISTORY_TYPE_NONE              = -1
TEXT_HISTORY_TYPE_BASE              = 0
TEXT_HISTORY_TYPE_NAMESPACE_AND_KEY = 1   # 旧格式
TEXT_HISTORY_TYPE_NO_LOCALIZATION   = 2
TEXT_HISTORY_TYPE_STRING_TABLE      = 3
TEXT_HISTORY_TYPE_ORDERED_FORMAT    = 4
TEXT_HISTORY_TYPE_ARGUMENT_FORMAT   = 5
TEXT_HISTORY_TYPE_AS_DATE           = 6
TEXT_HISTORY_TYPE_TRANSFORM         = 7
TEXT_HISTORY_TYPE_AS_NUMBER         = 8
TEXT_HISTORY_TYPE_GENDER            = 9
TEXT_HISTORY_TYPE_RAW_TEXT          = 11
TEXT_HISTORY_TYPE_STRING_TABLE_ENTRY = 3  # same as STRING_TABLE (UAssetAPI 映射)

# ETextFlag
TEXT_FLAG_CULTURE_INVARIANT = 1 << 2
TEXT_FLAG_IS_INITIALIZED    = 1 << 8


def _read_fname(reader: 'BinaryReader', asset: 'UAsset') -> str:
    idx, num = reader.read_fname_raw()
    return asset._resolve_fname(idx, num)


@register("TextProperty")
class TextPropertyData(PropertyData):
    """
    UE FText 属性。
    组成：flags(uint32) + history_type(int8) + ...history data...
    """
    __slots__ = ('flags', 'history_type', 'value', 'namespace',
                 'key', 'culture_invariant_string', 'raw_data')
    PROPERTY_TYPE = "TextProperty"

    def __init__(self):
        super().__init__()
        self.flags: int = 0
        self.history_type: int = TEXT_HISTORY_TYPE_NONE
        self.value: Optional[Any] = None
        self.namespace: Optional[str] = None
        self.key: Optional[str] = None
        self.culture_invariant_string: Optional[str] = None
        self.raw_data: Optional[dict] = None

    def read_value(self, reader, include_header, tag, leng, asset):
        if asset.object_version < VER_UE4_FTEXT_HISTORY:
            # 旧格式：直接读 namespace + key + string
            self.namespace = reader.read_fstring()
            self.key = reader.read_fstring()
            self.value = reader.read_fstring()
            return

        # 新格式（>= 479）
        self.flags = reader.read_uint32()
        self.history_type = reader.read_int8()

        ht = self.history_type

        if ht == TEXT_HISTORY_TYPE_NONE:
            has_culture_invariant = bool(self.flags & TEXT_FLAG_CULTURE_INVARIANT)
            if has_culture_invariant:
                self.culture_invariant_string = reader.read_fstring()

        elif ht == TEXT_HISTORY_TYPE_BASE:
            self.namespace = reader.read_fstring()
            self.key = reader.read_fstring()
            self.value = reader.read_fstring()

        elif ht == TEXT_HISTORY_TYPE_RAW_TEXT:
            self.value = reader.read_fstring()

        elif ht == TEXT_HISTORY_TYPE_STRING_TABLE:
            # TableId (FName) + Key (FString)
            table_id = _read_fname(reader, asset)
            key = reader.read_fstring()
            self.raw_data = {'TableId': table_id, 'Key': key}
            self.value = f'{table_id}:{key}'

        elif ht == TEXT_HISTORY_TYPE_NO_LOCALIZATION:
            self.value = reader.read_fstring()

        else:
            # 未实现的 HistoryType - 记录但不抛出异常
            # 我们没有 length 信息无法跳过，只能置为 None
            self.raw_data = {'UnimplementedHistoryType': ht}
            self.value = None

    def to_json(self, asset) -> dict:
        d = self._base_json()
        ht = self.history_type

        text_obj: dict = {
            '$type': 'UAssetAPI.UnrealTypes.FText, UAssetAPI',
            'Flags': self.flags,
            'HistoryType': ht,
        }

        if ht == TEXT_HISTORY_TYPE_NONE:
            text_obj['HasCultureInvariantString'] = self.culture_invariant_string is not None
            if self.culture_invariant_string is not None:
                text_obj['CultureInvariantString'] = self.culture_invariant_string

        elif ht == TEXT_HISTORY_TYPE_BASE:
            text_obj['Namespace'] = self.namespace
            text_obj['Key'] = self.key
            text_obj['SourceString'] = self.value

        elif ht in (TEXT_HISTORY_TYPE_RAW_TEXT, TEXT_HISTORY_TYPE_NO_LOCALIZATION):
            text_obj['Value'] = self.value

        elif ht == TEXT_HISTORY_TYPE_STRING_TABLE and self.raw_data:
            text_obj['TableId'] = self.raw_data.get('TableId')
            text_obj['Key'] = self.raw_data.get('Key')

        else:
            if self.raw_data:
                text_obj.update(self.raw_data)

        d['Value'] = text_obj
        return d
