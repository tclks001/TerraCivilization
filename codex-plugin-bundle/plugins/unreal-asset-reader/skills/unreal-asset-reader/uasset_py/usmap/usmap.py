# usmap/usmap.py
# Usmap 最小实现 - 仅提供 schema 查询接口
#
# 设计说明：
# .usmap 文件包含类的属性 schema（名字 + 类型），用于 unversioned 属性解析。
# Phase 1 的最小实现：
# - 接口完整（get_schema 等）
# - 实际解析暂不实现，返回 None（触发容错路径）
# - Phase 2 可扩展为完整实现

from __future__ import annotations
from typing import Optional, List
from pathlib import Path


class UsmapSchema:
    """某个类的 schema（属性列表）"""
    __slots__ = ('class_name', 'props')

    def __init__(self, class_name: str, props: List['UsmapProperty']):
        self.class_name = class_name
        self.props = props

    def get_property(self, index: int) -> Optional['UsmapProperty']:
        if 0 <= index < len(self.props):
            return self.props[index]
        return None


class UsmapProperty:
    """schema 中单个属性信息"""
    __slots__ = ('name', 'type_name', 'struct_type', 'enum_type',
                 'inner_type', 'value_type', 'array_index')

    def __init__(self, name: str, type_name: str):
        self.name = name
        self.type_name = type_name
        self.struct_type: str = 'None'
        self.enum_type: str = 'None'
        self.inner_type: str = 'None'
        self.value_type: str = 'None'
        self.array_index: int = 0


class Usmap:
    """
    .usmap 文件的最小实现。

    当前：不加载任何 schema，所有查询返回 None。
    unversioned 资产解析时，若 schema=None 则降级为 RawExport。
    """
    __slots__ = ('_schemas', '_path')

    def __init__(self, path: Optional[str] = None):
        self._schemas: dict[str, UsmapSchema] = {}
        self._path = path

        if path and Path(path).exists():
            self._load(path)

    def _load(self, path: str):
        """加载 .usmap 文件（Phase 1：暂不实现）。"""
        # Phase 2 TODO: 解析 .usmap 格式
        pass

    def get_schema(self, class_name: str) -> Optional[UsmapSchema]:
        """根据类名获取 schema。"""
        return self._schemas.get(class_name)

    def get_schema_from_name(self, class_name: str) -> Optional[UsmapSchema]:
        """别名（兼容接口）。"""
        return self.get_schema(class_name)
