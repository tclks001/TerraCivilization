# exports/string_table_export.py
# StringTableExport - 解析 UE StringTable 资产
#
# 设计说明：
# StringTable 是简单的 key→value 字符串映射表，用于多语言文本。
# body 格式（在 NormalExport 属性列表之后）：
#   FString table_namespace
#   int32 n
#   [(FString key, FString value) x n]
#
# 相比 DataTableExport 非常简单，不涉及结构体类型推断。

from __future__ import annotations
from typing import TYPE_CHECKING, List, Optional, Tuple

from .normal_export import NormalExport

if TYPE_CHECKING:
    from ..core.reader import BinaryReader
    from ..core.uasset import UAsset
    from ..core.import_export import FObjectExport


class StringTableExport(NormalExport):
    """
    StringTable export 解析器。

    table_namespace: 命名空间字符串
    entries: [(key, value), ...] 键值对列表
    """
    __slots__ = ('_header', 'properties', 'object_guid', '_parse_errors', '_raw_data',
                 'table_namespace', 'entries')

    def __init__(self, header: 'FObjectExport'):
        super().__init__(header)
        self.table_namespace: Optional[str] = None
        self.entries: List[Tuple[str, str]] = []

    def read(self, reader: 'BinaryReader', asset: 'UAsset', next_starting: int = -1):
        try:
            # Step 1: 读取 NormalExport 属性列表
            super()._do_read(reader, asset, next_starting)

            # Step 2: 读取 StringTable 二进制数据
            self.table_namespace = reader.read_fstring() or ''

            n = reader.read_int32()
            for _ in range(n):
                key = reader.read_fstring() or ''
                value = reader.read_fstring() or ''
                self.entries.append((key, value))

        except Exception as e:
            self._parse_errors.append(f"StringTableExport read failed: {e}")

    def to_json(self, asset: 'UAsset') -> dict:
        d = super().to_json(asset)
        d['$type'] = 'UAssetAPI.ExportTypes.StringTableExport, UAssetAPI'
        d['Table'] = {
            'TableNamespace': self.table_namespace,
            'Entries': {k: v for k, v in self.entries},
        }
        return d
