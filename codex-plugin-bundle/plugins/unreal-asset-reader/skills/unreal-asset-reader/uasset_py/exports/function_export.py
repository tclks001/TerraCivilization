# exports/function_export.py
# FunctionExport - 解析 UE UFunction（蓝图函数）
#
# 设计说明：
# UFunction 继承 UStruct，在 StructExport 内容后额外存一个 FunctionFlags(uint32)。
# Phase 2 实现：super().read() → 读 FunctionFlags。

from __future__ import annotations
from typing import TYPE_CHECKING

from .struct_export import StructExport

if TYPE_CHECKING:
    from ..core.reader import BinaryReader
    from ..core.uasset import UAsset
    from ..core.import_export import FObjectExport


class FunctionExport(StructExport):
    """
    UFunction export 解析器。

    function_flags: uint32（EFunctionFlags）
    """
    __slots__ = ('_header', 'properties', 'object_guid', '_parse_errors', '_raw_data',
                 'super_struct', 'children', 'loaded_properties',
                 'script_bytecode_size', 'script_bytecode_raw', 'script_bytecode',
                 'function_flags')

    def __init__(self, header: 'FObjectExport'):
        super().__init__(header)
        self.function_flags: int = 0

    def read(self, reader: 'BinaryReader', asset: 'UAsset', next_starting: int = -1):
        try:
            # StructExport.read() 包含 NormalExport + super_struct/children/props/bytecode
            super().read(reader, asset, next_starting)

            # FunctionFlags（仅当 StructExport 读取没有完全失败时继续）
            if reader.remaining() >= 4:
                self.function_flags = reader.read_uint32()

        except Exception as e:
            self._parse_errors.append(f"FunctionExport read failed: {e}")

    def to_json(self, asset: 'UAsset') -> dict:
        d = super().to_json(asset)
        d['$type'] = 'UAssetAPI.ExportTypes.FunctionExport, UAssetAPI'
        d['FunctionFlags'] = self.function_flags
        return d
