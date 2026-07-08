# exports/class_export.py
# ClassExport - 解析 UE UClass（BlueprintGeneratedClass 等）
#
# 设计说明：
# ClassExport 继承 StructExport，在 StructExport 的内容之后有大量额外字段：
#   - ClassFlags (uint32)
#   - ClassWithin (FPackageIndex)
#   - ClassConfigName (FName)
#   - Interfaces (array of FImplementedInterface)
#   - ClassGeneratedBy (FPackageIndex / FSoftObjectPath)
#   - bDeprecatedForceScriptOrder (bool)
#   - bCooked (bool)
#   - ClassDefaultObject (FPackageIndex)
#   等等...
#
# Phase 2 策略：继承 StructExport，读完 StructExport 内容后，
# 把剩余字节作为 class_raw_extras 存储，不强求完整解析。
# 这样能保证 StructExport 部分正确，Class 特有字段 raw fallback。
#
# Phase 3 可以在此基础上完整实现 ClassExport。

from __future__ import annotations
from typing import TYPE_CHECKING, Optional

from .struct_export import StructExport

if TYPE_CHECKING:
    from ..core.reader import BinaryReader
    from ..core.uasset import UAsset
    from ..core.import_export import FObjectExport


class ClassExport(StructExport):
    """
    UClass export 解析器（BlueprintGeneratedClass 等）。

    Phase 2：StructExport 内容读完后，剩余字节存为 class_raw_extras。
    """
    __slots__ = ('_header', 'properties', 'object_guid', '_parse_errors', '_raw_data',
                 'super_struct', 'children', 'loaded_properties',
                 'script_bytecode_size', 'script_bytecode_raw',
                 'class_raw_extras')

    def __init__(self, header: 'FObjectExport'):
        super().__init__(header)
        self.class_raw_extras: bytes = b''

    def read(self, reader: 'BinaryReader', asset: 'UAsset', next_starting: int = -1):
        try:
            # StructExport 部分（包含 NormalExport + UStruct 字段）
            super().read(reader, asset, next_starting)

            # 剩余字节作为 class 特有字段的 raw fallback
            remaining = reader.remaining()
            if next_starting > 0:
                remaining = max(0, next_starting - reader.position)
            if remaining > 0:
                avail = min(remaining, reader.remaining())
                if avail > 0:
                    self.class_raw_extras = bytes(reader.read_bytes(avail))

        except Exception as e:
            self._parse_errors.append(f"ClassExport read failed: {e}")

    def to_json(self, asset: 'UAsset') -> dict:
        import base64
        d = super().to_json(asset)
        d['$type'] = 'UAssetAPI.ExportTypes.ClassExport, UAssetAPI'
        # Phase 2：class 特有字段以 raw base64 存储
        d['ClassRawExtras'] = base64.b64encode(self.class_raw_extras).decode('ascii')
        return d
