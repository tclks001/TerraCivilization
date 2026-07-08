# core/import_export.py
# Import / Export map entry 数据类

from __future__ import annotations
from typing import Optional


class FObjectImport:
    """Import map entry"""
    __slots__ = (
        'class_package', 'class_name', 'outer_index',
        'object_name', 'package_name', 'import_optional',
    )

    def __init__(self):
        self.class_package: str = ''
        self.class_name: str = ''
        self.outer_index: int = 0       # FPackageIndex raw int32
        self.object_name: str = ''
        self.package_name: Optional[str] = None
        self.import_optional: bool = False

    def to_dict(self) -> dict:
        return {
            "ObjectName":   self.object_name,
            "ClassName":    self.class_name,
            "ClassPackage": self.class_package,
            "OuterIndex":   self.outer_index,
        }


class FObjectExport:
    """Export map entry（仅 header 字段，不含 export data body）"""
    __slots__ = (
        'index', 'object_name', 'outer_index', 'class_index', 'super_index',
        'template_index', 'object_flags', 'serial_size', 'serial_offset',
        'script_serialization_start_offset', 'script_serialization_end_offset',
        'forced_export', 'not_for_client', 'not_for_server',
        'package_guid_bytes', 'is_inherited_instance',
        'package_flags', 'not_always_loaded_for_editor_game',
        'is_asset', 'generate_public_hash',
        'first_export_dependency_offset',
        'serialization_before_serialization_dependencies_size',
        'create_before_serialization_dependencies_size',
        'serialization_before_create_dependencies_size',
        'create_before_create_dependencies_size',
    )

    def __init__(self):
        self.index: int = 0
        self.object_name: str = ''
        self.outer_index: int = 0
        self.class_index: int = 0
        self.super_index: int = 0
        self.template_index: int = 0
        self.object_flags: int = 0
        self.serial_size: int = 0
        self.serial_offset: int = 0
        self.script_serialization_start_offset: int = 0
        self.script_serialization_end_offset: int = 0
        self.forced_export: bool = False
        self.not_for_client: bool = False
        self.not_for_server: bool = False
        self.package_guid_bytes: Optional[bytes] = None
        self.is_inherited_instance: bool = False
        self.package_flags: int = 0
        self.not_always_loaded_for_editor_game: bool = False
        self.is_asset: bool = False
        self.generate_public_hash: bool = False
        self.first_export_dependency_offset: int = -1
        self.serialization_before_serialization_dependencies_size: int = 0
        self.create_before_serialization_dependencies_size: int = 0
        self.serialization_before_create_dependencies_size: int = 0
        self.create_before_create_dependencies_size: int = 0

    def to_dict(self) -> dict:
        return {
            "Index":      self.index,
            "ObjectName": self.object_name,
            "ExportType": "Export",       # Phase 0 暂不分类，Phase 1+ 会细化
            "OuterIndex": self.outer_index,
        }
