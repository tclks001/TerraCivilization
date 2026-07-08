# exports/data_table_export.py
# DataTableExport - 解析 UE DataTable 资产的行数据
#
# 设计说明：
# DataTable 的 export body 分两部分：
#   1. NormalExport 的属性列表（含 RowStruct 属性，存储行结构体类型引用）
#   2. 行数据：numEntries 个 (FName row_name, StructPropertyData row_data) 对
#
# 关键：每行数据是 NTPL 格式（None-Terminated Property List），与 StructPropertyData
# 的 _read_ntpl() 完全相同，只是 include_header=True，读完所有属性直到 "None"。

from __future__ import annotations
from typing import TYPE_CHECKING, List, Optional, Tuple, Any

from .normal_export import NormalExport

if TYPE_CHECKING:
    from ..core.reader import BinaryReader
    from ..core.uasset import UAsset
    from ..core.import_export import FObjectExport
    from ..properties.base import PropertyData


class DataTableRow:
    """
    DataTable 单行数据。
    row_name: 行名（FName 字符串，如 "AntLionAttrData_11"）
    properties: 该行的属性列表（NTPL 格式，属性类型由 RowStruct 决定）
    """
    __slots__ = ('row_name', 'struct_type', 'properties', '_parse_errors')

    def __init__(self, row_name: str, struct_type: Optional[str] = None):
        self.row_name = row_name
        self.struct_type: Optional[str] = struct_type
        self.properties: List['PropertyData'] = []
        self._parse_errors: List[str] = []

    def to_json(self, asset: 'UAsset') -> dict:
        props_json = []
        for p in self.properties:
            try:
                props_json.append(p.to_json(asset))
            except Exception as e:
                props_json.append({'_error': str(e)})
        d = {
            '$type': 'UAssetAPI.PropertyTypes.Structs.StructPropertyData, UAssetAPI',
            'StructType': self.struct_type,
            'Name': self.row_name,
            'ArrayIndex': 0,
            'IsZero': False,
            'PropertyGuid': None,
            'Ancestry': {'$type': 'UAssetAPI.PropertyTypes.Objects.AncestryInfo, UAssetAPI',
                         'Lineage': []},
            'Value': props_json,
            'SerializeNone': True,
        }
        if self._parse_errors:
            d['_ParseErrors'] = self._parse_errors
        return d


class DataTableExport(NormalExport):
    """
    DataTable export 解析器。

    继承 NormalExport，在 super().read() 之后：
    1. 从属性列表里找 RowStruct 属性确定行结构体类型
    2. 逐行读取 (FName, NTPL属性列表) 对

    为什么继承 NormalExport 而不是单独实现：
    - DataTable 的 export body 就是 NormalExport + 额外行数据
    - 复用 NormalExport 的属性读取可以正确处理 SerializationControl 等版本差异
    """
    __slots__ = ('_header', 'properties', 'object_guid', '_parse_errors', '_raw_data',
                 'table_data', 'row_struct_name')

    def __init__(self, header: 'FObjectExport'):
        super().__init__(header)
        self.table_data: List[DataTableRow] = []
        self.row_struct_name: Optional[str] = None

    def _resolve_row_struct(self, asset: 'UAsset') -> Optional[str]:
        """
        从 NormalExport 属性列表里找 RowStruct（ObjectPropertyData），
        返回其指向的 import/export 的 object_name。
        """
        for prop in self.properties:
            if getattr(prop, 'name', '') == 'RowStruct':
                # ObjectPropertyData 的值是 int（FPackageIndex）
                value = getattr(prop, 'value', None)
                if value is None:
                    continue
                idx = int(value)
                if idx < 0:
                    imp_idx = -idx - 1
                    if 0 <= imp_idx < len(asset.imports):
                        return asset.imports[imp_idx].object_name
                elif idx > 0:
                    exp_idx = idx - 1
                    if 0 <= exp_idx < len(asset.exports):
                        return asset.exports[exp_idx].object_name
        return None

    def _read_row(self, reader: 'BinaryReader', asset: 'UAsset',
                  struct_type: Optional[str] = None) -> DataTableRow:
        """读取单行数据：row_name(FName) + NTPL 属性列表。"""
        from ..properties.base import read_property

        row_name = asset._read_fname(reader)
        row = DataTableRow(row_name, struct_type=struct_type)

        # 读 NTPL 属性列表（None-Terminated Property List）
        while True:
            try:
                prop = read_property(reader, asset, include_header=True)
                if prop is None:
                    break
                row.properties.append(prop)
            except Exception as e:
                row._parse_errors.append(f"Row '{row_name}' property parse error: {e}")
                break

        return row

    def read(self, reader: 'BinaryReader', asset: 'UAsset', next_starting: int = -1):
        """
        读取 DataTableExport body。

        流程：
        1. super().read() → NormalExport 的属性列表（含 RowStruct）
        2. 解析 RowStruct 确定行结构体类型名
        3. 读 numEntries，逐行读取
        """
        try:
            # Step 1: 读取 NormalExport 部分（属性列表）
            super()._do_read(reader, asset, next_starting)

            # Step 2: 确定行结构体类型
            self.row_struct_name = self._resolve_row_struct(asset)

            # Step 3: 读行数
            num_entries = reader.read_int32()

            # Step 4: 逐行读取
            for _ in range(num_entries):
                try:
                    row = self._read_row(reader, asset, struct_type=self.row_struct_name)
                    self.table_data.append(row)
                except Exception as e:
                    self._parse_errors.append(f"Row {len(self.table_data)} parse error: {e}")
                    break

        except Exception as e:
            self._parse_errors.append(f"DataTableExport read failed: {e}")

    def to_json(self, asset: 'UAsset') -> dict:
        from ..properties.base import CSHARP_TYPE_MAP

        # 构建 NormalExport 基础 JSON
        d = super().to_json(asset)

        # 覆盖 $type 为 DataTableExport
        d['$type'] = CSHARP_TYPE_MAP.get(
            'DataTableExport',
            'UAssetAPI.ExportTypes.DataTableExport, UAssetAPI'
        )

        # 构建 Table（与 C# UAssetAPI 格式兼容）
        # C# 格式: {"$type": "UAssetAPI.ExportTypes.DataTable, UAssetAPI", "Data": [...]}
        # 每个行是一个 StructPropertyData，Name 字段是行名
        rows_json = []
        for row in self.table_data:
            try:
                rows_json.append(row.to_json(asset))
            except Exception as e:
                rows_json.append({'_error': str(e), 'Name': row.row_name})

        d['Table'] = {
            '$type': 'UAssetAPI.ExportTypes.DataTable, UAssetAPI',
            'Data': rows_json,
        }

        return d
