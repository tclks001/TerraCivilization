# exports/level_export.py
# LevelExport - 解析 UE Level 资产（关卡）
#
# 设计说明：
# Level 资产格式非常复杂，包含 Actor 数组、Model、序列化的 World 引用等，
# 远超 NormalExport 的标准属性列表格式。
#
# Phase 2 策略：
# 1. super().read() 读取 NormalExport 属性列表（Level 里也有标准属性）
# 2. 读取基础字段：owner(FPackageIndex) + actors 数组（简化，可能不准确）
# 3. 剩余字节作为 level_raw 存储
#
# 注意：Level export 的实际格式依赖 ULevel::Serialize，包含：
# - UObject::Serialize 的部分（属性 + NetIndex 等）
# - Actors TArray<AActor*>（序列化为 FPackageIndex 数组）
# - Model（复杂 UModel 结构）
# - 等等
#
# Phase 2 不保证 level_raw 的语义正确性，只确保不崩溃。

from __future__ import annotations
from typing import TYPE_CHECKING, List, Optional

from .normal_export import NormalExport

if TYPE_CHECKING:
    from ..core.reader import BinaryReader
    from ..core.uasset import UAsset
    from ..core.import_export import FObjectExport


class LevelExport(NormalExport):
    """
    Level export 解析器。

    Phase 2 只读 NormalExport 部分 + 前几个基础字段，剩余 raw fallback。

    owner: int（FPackageIndex，level 的 outer）
    actors: list[int]（FPackageIndex 数组，关卡内 Actor 列表）
    level_raw: bytes（剩余未解析字节）
    """
    __slots__ = ('_header', 'properties', 'object_guid', '_parse_errors', '_raw_data',
                 'owner', 'actors', 'level_raw')

    def __init__(self, header: 'FObjectExport'):
        super().__init__(header)
        self.owner: int = 0
        self.actors: List[int] = []
        self.level_raw: bytes = b''

    def read(self, reader: 'BinaryReader', asset: 'UAsset', next_starting: int = -1):
        try:
            # Step 1: NormalExport 属性列表
            super()._do_read(reader, asset, next_starting)

            # Step 2: 尝试读取基础字段
            # Level 序列化时先有 NetIndex 等 UObject 字段，Phase 2 直接读 owner + actors
            if reader.remaining() >= 4:
                self.owner = reader.read_int32()

            if reader.remaining() >= 4:
                num_actors = reader.read_int32()
                # 合理性检查（防止读到垃圾数据）
                if 0 <= num_actors <= 100000:
                    for _ in range(num_actors):
                        if reader.remaining() < 4:
                            break
                        self.actors.append(reader.read_int32())

            # Step 3: 剩余字节存 raw
            remaining = reader.remaining()
            if next_starting > 0:
                remaining = max(0, next_starting - reader.position)
            avail = min(remaining, reader.remaining())
            if avail > 0:
                self.level_raw = bytes(reader.read_bytes(avail))

        except Exception as e:
            self._parse_errors.append(f"LevelExport read failed: {e}")
            # 失败时存剩余字节
            try:
                rem = reader.remaining()
                if rem > 0:
                    self.level_raw = bytes(reader.read_bytes(rem))
            except Exception:
                pass

    def to_json(self, asset: 'UAsset') -> dict:
        import base64
        d = super().to_json(asset)
        d['$type'] = 'UAssetAPI.ExportTypes.LevelExport, UAssetAPI'
        d['Owner'] = self.owner
        d['Actors'] = self.actors
        d['LevelRaw'] = base64.b64encode(self.level_raw).decode('ascii')
        return d
