# exports/export.py
# Export 基类 + RawExport（Phase 1）

from __future__ import annotations
from typing import TYPE_CHECKING, Optional

if TYPE_CHECKING:
    from ..core.import_export import FObjectExport
    from ..core.uasset import UAsset


class BaseExport:
    """
    所有 Export 子类型的基类。

    Phase 0 只携带 header 信息，不解析 export data body。
    Phase 1 会继承本类，实现 NormalExport 等具体子类型。
    """
    __slots__ = ('_header',)

    def __init__(self, header: 'FObjectExport'):
        self._header = header

    @property
    def object_name(self) -> str:
        return self._header.object_name

    @property
    def outer_index(self) -> int:
        return self._header.outer_index

    def to_dict(self) -> dict:
        return self._header.to_dict()


class RawExport(BaseExport):
    """
    无法解析为 NormalExport 时的原始字节 fallback。
    保留 export data 的原始字节，以便调试和后续处理。
    """
    __slots__ = ('_header', 'raw_data', '_error')

    def __init__(self, header: 'FObjectExport'):
        super().__init__(header)
        self.raw_data: Optional[bytes] = None
        self._error: Optional[str] = None

    def to_dict(self) -> dict:
        d = self._header.to_dict()
        d['ExportType'] = 'RawExport'
        if self._error:
            d['Error'] = self._error
        return d

    def to_json(self, asset: 'UAsset') -> dict:
        import base64
        from ..properties.base import CSHARP_TYPE_MAP
        raw_bytes = self.raw_data if self.raw_data else b''
        return {
            '$type': CSHARP_TYPE_MAP['RawExport'],
            'Data': base64.b64encode(raw_bytes).decode('ascii'),
            'ObjectName': self._header.object_name,
            'ClassIndex': self._header.class_index,
            'SuperIndex': self._header.super_index,
            'TemplateIndex': self._header.template_index,
            'OuterIndex': self._header.outer_index,
            'ObjectFlags': self._header.object_flags,
            'SerialSize': self._header.serial_size,
            'SerialOffset': self._header.serial_offset,
            '_Error': self._error,
        }
