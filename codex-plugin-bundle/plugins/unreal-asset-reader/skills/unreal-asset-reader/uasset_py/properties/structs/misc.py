# properties/structs/misc.py
# 杂项 struct 类型：SoftObjectPath / GameplayTagContainer

from __future__ import annotations
from typing import TYPE_CHECKING, Optional, List

from ..base import PropertyData, CSHARP_TYPE_MAP
from ..registry import register

if TYPE_CHECKING:
    from ...core.reader import BinaryReader
    from ...core.uasset import UAsset

from ...core.versions import FSOFTOBJECTPATH_REMOVE_ASSET_PATH_FNAMES


def _read_fname(reader: 'BinaryReader', asset: 'UAsset') -> str:
    idx, num = reader.read_fname_raw()
    return asset._resolve_fname(idx, num)


# ── SoftObjectPath ────────────────────────────────────────────

@register("SoftObjectPathProperty")
class SoftObjectPathPropertyData(PropertyData):
    """
    FSoftObjectPath（作为 struct 使用时的版本）。

    版本分支：
    - ue5 >= ADD_SOFTOBJECTPATH_LIST (9)：int32 索引
    - ue5 >= FSOFTOBJECTPATH_REMOVE_ASSET_PATH_FNAMES (8) 且 < 9：FName×2 + FString
    - 旧格式：FString + FString
    """
    __slots__ = ('asset_path_name', 'sub_path_string', 'package_name', 'asset_name', 'soft_path_index')
    PROPERTY_TYPE = "SoftObjectPathProperty"

    def __init__(self):
        super().__init__()
        self.asset_path_name: Optional[str] = None
        self.sub_path_string: Optional[str] = None
        self.package_name: Optional[str] = None
        self.asset_name: Optional[str] = None
        self.soft_path_index: int = -1

    def read_value(self, reader, include_header, tag, leng, asset):
        from ...core.versions import ADD_SOFTOBJECTPATH_LIST, FSOFTOBJECTPATH_REMOVE_ASSET_PATH_FNAMES
        if asset.object_version_ue5 >= ADD_SOFTOBJECTPATH_LIST:
            self.soft_path_index = reader.read_int32()
            if 0 <= self.soft_path_index < len(asset.soft_object_paths_list):
                entry = asset.soft_object_paths_list[self.soft_path_index]
                self.package_name = entry['pkg']
                self.asset_name = entry['asset']
                self.sub_path_string = entry.get('sub')
        elif asset.object_version_ue5 >= FSOFTOBJECTPATH_REMOVE_ASSET_PATH_FNAMES:
            self.package_name = _read_fname(reader, asset)
            self.asset_name = _read_fname(reader, asset)
            self.sub_path_string = reader.read_fstring()
        else:
            self.asset_path_name = reader.read_fstring()
            self.sub_path_string = reader.read_fstring()

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['$type'] = CSHARP_TYPE_MAP.get('SoftObjectPathPropertyData', d['$type'])
        if self.package_name is not None:
            d['Value'] = {
                'PackageName': self.package_name,
                'AssetName': self.asset_name,
                'SubPathString': self.sub_path_string,
            }
        else:
            d['Value'] = {
                'AssetPathName': self.asset_path_name,
                'SubPathString': self.sub_path_string,
            }
        return d


# ── GameplayTagContainer ──────────────────────────────────────

@register("GameplayTagContainerProperty")
class GameplayTagContainerPropertyData(PropertyData):
    """
    FGameplayTagContainer - 数组 FName（各 gameplay tag）。
    格式：int32 count + count × FName
    """
    __slots__ = ('tags',)
    PROPERTY_TYPE = "GameplayTagContainerProperty"

    def __init__(self):
        super().__init__()
        self.tags: List[str] = []

    def read_value(self, reader, include_header, tag, leng, asset):
        count = reader.read_int32()
        self.tags = [_read_fname(reader, asset) for _ in range(count)]

    def to_json(self, asset) -> dict:
        d = self._base_json()
        d['$type'] = CSHARP_TYPE_MAP.get('GameplayTagContainerPropertyData', d['$type'])
        d['Value'] = [
            {'$type': 'UAssetAPI.UnrealTypes.FGameplayTag, UAssetAPI', 'TagName': tag}
            for tag in self.tags
        ]
        return d
