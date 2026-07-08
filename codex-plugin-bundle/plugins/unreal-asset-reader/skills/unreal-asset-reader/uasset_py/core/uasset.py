# core/uasset.py
# UAsset 文件解析主体（header + name map + import/export map）

from __future__ import annotations
import struct
from typing import Optional, List

from .reader import BinaryReader
from .types import FGuid, FEngineVersion
from .versions import (
    # UE4
    VER_UE4_ENGINE_VERSION_OBJECT,
    VER_UE4_PACKAGE_SUMMARY_HAS_COMPATIBLE_ENGINE_VERSION,
    VER_UE4_SERIALIZE_TEXT_IN_PACKAGES,
    VER_UE4_LOAD_FOR_EDITOR_GAME,
    VER_UE4_WORLD_LEVEL_INFO,
    VER_UE4_TemplateIndex_IN_COOKED_EXPORTS,
    VER_UE4_PRELOAD_DEPENDENCIES_IN_COOKED_EXPORTS,
    VER_UE4_NAME_HASHES_SERIALIZED,
    VER_UE4_ADDED_SEARCHABLE_NAMES,
    VER_UE4_64BIT_EXPORTMAP_SERIALSIZES,
    VER_UE4_ADDED_PACKAGE_OWNER,
    VER_UE4_COOKED_ASSETS_IN_EDITOR_SUPPORT,
    VER_UE4_NON_OUTER_PACKAGE_IMPORT,
    VER_UE4_ADD_STRING_ASSET_REFERENCES_MAP,
    VER_UE4_CHANGED_CHUNKID_TO_BE_AN_ARRAY_OF_CHUNKIDS,
    VER_UE4_ADDED_PACKAGE_SUMMARY_LOCALIZATION_ID,
    # UE5
    NAMES_REFERENCED_FROM_EXPORT_DATA,
    PAYLOAD_TOC,
    OPTIONAL_RESOURCES,
    LARGE_WORLD_COORDINATES,
    REMOVE_OBJECT_EXPORT_PACKAGE_GUID,
    TRACK_OBJECT_EXPORT_IS_INHERITED,
    ADD_SOFTOBJECTPATH_LIST,
    DATA_RESOURCES,
    SCRIPT_SERIALIZATION_OFFSET,
    PACKAGE_SAVED_HASH,
    VERSE_CELLS,
    METADATA_SERIALIZATION_OFFSET,
    # Flags
    PKG_FilterEditorOnly,
    PKG_UnversionedProperties,
)
from .custom_versions import FCustomVersion
from .import_export import FObjectImport, FObjectExport

UASSET_MAGIC = 0x9E2A83C1
_MAX_GENERATION_COUNT = 10000  # 合理上限，超过则触发 Valorant 兜底逻辑


class UAsset:
    """
    解析 .uasset 文件的 header、name map、import/export map。

    设计决策：
    - lazy=True 时只解析 header（toc/refs 所需），跳过 export data body
    - 所有解析分阶段：_parse_header → _read_name_map → _read_imports → _read_exports
    - 保留 position 快照用于调试
    """
    __slots__ = (
        # 版本信息
        'object_version', 'object_version_ue5', 'legacy_file_version',
        'file_version_licensee_ue', 'is_unversioned',
        'custom_versions',
        # Header 关键偏移
        'section_six_offset', 'name_count', 'name_offset',
        'soft_object_paths_count', 'soft_object_paths_offset',
        'gatherable_text_data_count', 'gatherable_text_data_offset',
        'export_count', 'export_offset',
        'import_count', 'import_offset',
        'depends_offset', 'soft_package_references_count',
        'soft_package_references_offset', 'searchable_names_offset',
        'thumbnail_table_offset', 'asset_registry_data_offset',
        'bulk_data_start_offset', 'world_tile_info_data_offset',
        'preload_dependency_count', 'preload_dependency_offset',
        'names_referenced_from_export_data_count', 'payload_toc_offset',
        'data_resource_offset',
        'cell_export_count', 'cell_export_offset',
        'cell_import_count', 'cell_import_offset',
        'meta_data_offset',
        # 包信息
        'package_flags', 'folder_name', 'localization_id',
        'package_guid', 'persistent_guid',
        'generations',
        'recorded_engine_version', 'recorded_compatible_with_engine_version',
        'compression_flags', 'package_source',
        'additional_packages_to_cook', 'chunk_ids',
        'saved_hash',
        # 解析结果
        'name_map', 'imports', 'exports',
        'soft_object_paths_list',  # FSoftObjectPath 列表（UE5 ADD_SOFTOBJECTPATH_LIST）
        # Phase 1 新增
        '_data', '_export_objects',
    )

    def __init__(
        self,
        data: bytes,
        object_version: int = 0,
        object_version_ue5: int = 0,
        lazy: bool = True,
    ):
        self.object_version = object_version
        self.object_version_ue5 = object_version_ue5
        self.is_unversioned = False
        self.custom_versions: List[FCustomVersion] = []

        # 偏移字段初始化为 0
        self.section_six_offset = 0
        self.name_count = 0
        self.name_offset = 0
        self.soft_object_paths_count = 0
        self.soft_object_paths_offset = 0
        self.gatherable_text_data_count = 0
        self.gatherable_text_data_offset = 0
        self.export_count = 0
        self.export_offset = 0
        self.import_count = 0
        self.import_offset = 0
        self.depends_offset = 0
        self.soft_package_references_count = 0
        self.soft_package_references_offset = 0
        self.searchable_names_offset = 0
        self.thumbnail_table_offset = 0
        self.asset_registry_data_offset = 0
        self.bulk_data_start_offset = 0
        self.world_tile_info_data_offset = 0
        self.preload_dependency_count = 0
        self.preload_dependency_offset = 0
        self.names_referenced_from_export_data_count = 0
        self.payload_toc_offset = 0
        self.data_resource_offset = 0
        self.cell_export_count = 0
        self.cell_export_offset = 0
        self.cell_import_count = 0
        self.cell_import_offset = 0
        self.meta_data_offset = 0
        self.package_flags = 0
        self.folder_name: Optional[str] = None
        self.localization_id: Optional[str] = None
        self.package_guid: Optional[tuple] = None
        self.persistent_guid: Optional[tuple] = None
        self.generations: List[tuple] = []
        self.recorded_engine_version: Optional[FEngineVersion] = None
        self.recorded_compatible_with_engine_version: Optional[FEngineVersion] = None
        self.compression_flags = 0
        self.package_source = 0
        self.additional_packages_to_cook: List[str] = []
        self.chunk_ids: List[int] = []
        self.saved_hash: Optional[bytes] = None
        self.legacy_file_version = 0
        self.file_version_licensee_ue = 0

        self.name_map: List[str] = []
        self.imports: List[FObjectImport] = []
        self.exports: List[FObjectExport] = []
        self.soft_object_paths_list: List[dict] = []  # {'pkg': str, 'asset': str, 'sub': str|None}

        self._data: bytes = data  # 保留原始数据用于 export body 解析
        self._export_objects: list = []  # 解析后的 export 对象（NormalExport/RawExport）

        r = BinaryReader(data)
        self._parse_header(r)
        self._read_name_map(r)
        self._read_imports(r)
        self._read_exports(r)
        if self.soft_object_paths_count > 0:
            self._read_soft_object_paths(r)

    # ──────────────────────────────────────────────────────────────────────
    # Helper：FName 解析（依赖 name_map，只在 name_map 读完后调用）
    # ──────────────────────────────────────────────────────────────────────

    def _resolve_fname(self, idx: int, number: int) -> str:
        if 0 <= idx < len(self.name_map):
            value = self.name_map[idx] or ''
        else:
            value = f'<invalid:{idx}>'
        if number == 0:
            return value
        return f"{value}_{number - 1}"

    def _read_fname(self, r: BinaryReader) -> str:
        idx, num = r.read_fname_raw()
        return self._resolve_fname(idx, num)

    # ──────────────────────────────────────────────────────────────────────
    # Phase 1：解析文件头
    # ──────────────────────────────────────────────────────────────────────

    def _parse_header(self, r: BinaryReader):
        # 1. Magic
        magic = r.read_uint32()
        if magic != UASSET_MAGIC:
            raise ValueError(f"Invalid uasset magic: 0x{magic:08X}")

        # 2. LegacyFileVersion
        self.legacy_file_version = r.read_int32()

        # 3. LegacyUE3Version（仅当不是 -4 时存在）
        if self.legacy_file_version != -4:
            r.skip(4)  # skip LegacyUE3Version

        # 4. fileVersionUE4
        # 关键设计说明：
        # - 对 UE4 资产：file_version_ue4 > 0 时即为实际 ObjectVersion（如 522）
        # - 对 UE5 资产：file_version_ue4 仍可能 > 0（如 522），但这只是向后兼容字段。
        #   实际 ObjectVersion 为外部传入的引擎版本对应值（如 VER_UE5_3 → 1003）。
        #   1003 >= VER_UE4_ADDED_PACKAGE_SUMMARY_LOCALIZATION_ID(548) 等条件才能正确触发。
        # 因此：file_version_ue4 仅用于判断 is_unversioned，不覆盖外部 object_version。
        file_version_ue4 = r.read_int32()
        if file_version_ue4 > 0:
            self.is_unversioned = False
            # 注意：不在这里覆盖 self.object_version，保持外部传入值
        else:
            self.is_unversioned = True
            # object_version 保持外部传入值（unversioned 时靠外部参数决定）

        # 5. ObjectVersionUE5（LegacyFileVersion <= -8）
        # 文件里存储的是 UE 原始枚举值（VER_UE5_INITIAL_VERSION=1000 起），
        # UAssetAPI 内部使用 (raw - 999) 的相对序号（VER_UE5_0EA→1, VER_UE5_3→15 等）。
        if self.legacy_file_version <= -8:
            ue5_ver_raw = r.read_int32()
            if not self.is_unversioned:
                # 有版本文件：将原始值转换为内部序号
                self.object_version_ue5 = max(0, ue5_ver_raw - 999)

        # 6. FileVersionLicenseeUE
        self.file_version_licensee_ue = r.read_int32()

        # 7. SavedHash + SectionSixOffset（UE5 PACKAGE_SAVED_HASH 分支）
        if self.object_version_ue5 >= PACKAGE_SAVED_HASH:
            self.saved_hash = bytes(r.read_bytes(20))
            self.section_six_offset = r.read_int32()

        # 8. CustomVersionContainer（LegacyFileVersion <= -2）
        if self.legacy_file_version <= -2:
            self._read_custom_versions(r)

        # 9. SectionSixOffset（非 PACKAGE_SAVED_HASH 分支）
        if self.object_version_ue5 < PACKAGE_SAVED_HASH:
            self.section_six_offset = r.read_int32()

        # 10. FolderName
        self.folder_name = r.read_fstring()

        # 11. PackageFlags
        self.package_flags = r.read_uint32()

        is_filter_editor_only = bool(self.package_flags & PKG_FilterEditorOnly)

        # 12. NameCount + NameOffset
        self.name_count  = r.read_int32()
        self.name_offset = r.read_int32()

        # 13. SoftObjectPaths（UE5 ADD_SOFTOBJECTPATH_LIST）
        if self.object_version_ue5 >= ADD_SOFTOBJECTPATH_LIST:
            self.soft_object_paths_count  = r.read_int32()
            self.soft_object_paths_offset = r.read_int32()

        # 14. LocalizationId
        if (not is_filter_editor_only
                and self.object_version >= VER_UE4_ADDED_PACKAGE_SUMMARY_LOCALIZATION_ID):
            self.localization_id = r.read_fstring()

        # 15. GatherableTextData
        if self.object_version >= VER_UE4_SERIALIZE_TEXT_IN_PACKAGES:
            self.gatherable_text_data_count  = r.read_int32()
            self.gatherable_text_data_offset = r.read_int32()

        # 16. ExportCount + ExportOffset
        self.export_count  = r.read_int32()
        self.export_offset = r.read_int32()

        # 17. ImportCount + ImportOffset
        self.import_count  = r.read_int32()
        self.import_offset = r.read_int32()

        # 18. VERSE_CELLS
        if self.object_version_ue5 >= VERSE_CELLS:
            self.cell_export_count  = r.read_int32()
            self.cell_export_offset = r.read_int32()
            self.cell_import_count  = r.read_int32()
            self.cell_import_offset = r.read_int32()

        # 19. METADATA_SERIALIZATION_OFFSET
        if self.object_version_ue5 >= METADATA_SERIALIZATION_OFFSET:
            self.meta_data_offset = r.read_int32()

        # 20. DependsOffset
        self.depends_offset = r.read_int32()

        # 21. SoftPackageReferences
        if self.object_version >= VER_UE4_ADD_STRING_ASSET_REFERENCES_MAP:
            self.soft_package_references_count  = r.read_int32()
            self.soft_package_references_offset = r.read_int32()

        # 22. SearchableNamesOffset
        if self.object_version >= VER_UE4_ADDED_SEARCHABLE_NAMES:
            self.searchable_names_offset = r.read_int32()

        # 23. ThumbnailTableOffset
        self.thumbnail_table_offset = r.read_int32()

        # 24. PackageGuid（仅非 PACKAGE_SAVED_HASH 时）
        if self.object_version_ue5 < PACKAGE_SAVED_HASH:
            self.package_guid = r.read_guid()

        # 25. PersistentGuid / 旧 PackageGuid
        if not is_filter_editor_only:
            if self.object_version >= VER_UE4_ADDED_PACKAGE_OWNER:
                self.persistent_guid = r.read_guid()
            if (self.object_version >= VER_UE4_ADDED_PACKAGE_OWNER
                    and self.object_version < VER_UE4_NON_OUTER_PACKAGE_IMPORT):
                r.skip(16)  # skip old OwnerPersistentGuid

        # 26. GenerationCount + Generations
        generation_count = r.read_int32()
        if generation_count < 0 or generation_count > _MAX_GENERATION_COUNT:
            # Valorant 特有的 garbage data 处理：
            # 正常流程：读了 generation_count (4 bytes)，发现值不合理
            # 需要回退到 generation_count 前面 4 bytes（总计 -8 = 已读4 + 往前4 = Valorant garbage start）
            # 然后依次读取：8 bytes ValorantGarbageData, 16 bytes PackageGuid, 再读 generation_count
            r.skip(-8)
            r.skip(8)   # ValorantGarbageData
            r.skip(16)  # PackageGuid
            generation_count = r.read_int32()

        self.generations = []
        for _ in range(generation_count):
            ec = r.read_int32()
            nc = r.read_int32()
            self.generations.append((ec, nc))

        # 27. RecordedEngineVersion
        if self.object_version >= VER_UE4_ENGINE_VERSION_OBJECT:
            self.recorded_engine_version = self._read_engine_version(r)
        else:
            changelist = r.read_uint32()
            self.recorded_engine_version = FEngineVersion(0, 0, 0, changelist, '')

        # 28. RecordedCompatibleWithEngineVersion
        if self.object_version >= VER_UE4_PACKAGE_SUMMARY_HAS_COMPATIBLE_ENGINE_VERSION:
            self.recorded_compatible_with_engine_version = self._read_engine_version(r)

        # 29. CompressionFlags
        self.compression_flags = r.read_uint32()

        # 30. CompressedChunks（必须为 0）
        num_compressed_chunks = r.read_int32()
        if num_compressed_chunks != 0:
            raise ValueError(f"Compressed chunks not supported: count={num_compressed_chunks}")

        # 31. PackageSource
        self.package_source = r.read_uint32()

        # 32. AdditionalPackagesToCook
        cook_count = r.read_int32()
        self.additional_packages_to_cook = [r.read_fstring() for _ in range(cook_count)]

        # 33. numTextureAllocations（LegacyFileVersion > -7，已废弃，跳过）
        if self.legacy_file_version > -7:
            r.skip(4)

        # 34. AssetRegistryDataOffset
        self.asset_registry_data_offset = r.read_int32()

        # 35. BulkDataStartOffset（int64）
        self.bulk_data_start_offset = r.read_int64()

        # 36. WorldTileInfoDataOffset
        if self.object_version >= VER_UE4_WORLD_LEVEL_INFO:
            self.world_tile_info_data_offset = r.read_int32()

        # 37. ChunkIDs
        if self.object_version >= VER_UE4_CHANGED_CHUNKID_TO_BE_AN_ARRAY_OF_CHUNKIDS:
            chunk_count = r.read_int32()
            self.chunk_ids = [r.read_int32() for _ in range(chunk_count)]
        else:
            r.skip(4)  # 旧格式单个 ChunkID

        # 38. PreloadDependency
        if self.object_version >= VER_UE4_PRELOAD_DEPENDENCIES_IN_COOKED_EXPORTS:
            self.preload_dependency_count  = r.read_int32()
            self.preload_dependency_offset = r.read_int32()

        # 39. NamesReferencedFromExportDataCount
        if self.object_version_ue5 >= NAMES_REFERENCED_FROM_EXPORT_DATA:
            self.names_referenced_from_export_data_count = r.read_int32()

        # 40. PayloadTocOffset（int64）
        if self.object_version_ue5 >= PAYLOAD_TOC:
            self.payload_toc_offset = r.read_int64()

        # 41. DataResourceOffset
        if self.object_version_ue5 >= OPTIONAL_RESOURCES:
            self.data_resource_offset = r.read_int32()

    # ──────────────────────────────────────────────────────────────────────
    # CustomVersionContainer 解析
    # ──────────────────────────────────────────────────────────────────────

    def _read_custom_versions(self, r: BinaryReader):
        if self.legacy_file_version > -3:
            # Enums 格式（极旧，不实现）
            raise NotImplementedError("CustomVersion Enums format not supported")
        elif self.legacy_file_version > -6:
            # Guids 格式：每条有 GUID + version + FString name
            count = r.read_int32()
            for _ in range(count):
                guid_vals = r.read_guid()
                guid = FGuid(*guid_vals)
                version = r.read_int32()
                name = r.read_fstring()
                cv = FCustomVersion(key=guid, version=version, friendly_name=name)
                self.custom_versions.append(cv)
        else:
            # Optimized 格式（LegacyFileVersion <= -6）：每条只有 GUID + version
            count = r.read_int32()
            for _ in range(count):
                guid_vals = r.read_guid()
                guid = FGuid(*guid_vals)
                version = r.read_int32()
                cv = FCustomVersion(key=guid, version=version, friendly_name=None)
                self.custom_versions.append(cv)

    # ──────────────────────────────────────────────────────────────────────
    # FEngineVersion 解析
    # ──────────────────────────────────────────────────────────────────────

    @staticmethod
    def _read_engine_version(r: BinaryReader) -> FEngineVersion:
        major    = r.read_uint16()
        minor    = r.read_uint16()
        patch    = r.read_uint16()
        changelist = r.read_uint32()
        branch   = r.read_fstring() or ''
        return FEngineVersion(major, minor, patch, changelist, branch)

    # ──────────────────────────────────────────────────────────────────────
    # Phase 2：Name Map
    # ──────────────────────────────────────────────────────────────────────

    def _read_name_map(self, r: BinaryReader):
        r.seek(self.name_offset)
        # 对于 object_version < 504 的旧资产，默认不序列化 hash
        # 对于 object_version >= 504 的资产，默认序列化 hash
        will_serialize_hashes = (self.object_version >= VER_UE4_NAME_HASHES_SERIALIZED)

        for _ in range(self.name_count):
            s = r.read_fstring()
            self.name_map.append(s or '')

            if will_serialize_hashes:
                r.skip(4)  # CityHash64，UE4 >= 504 时始终存在

    # ──────────────────────────────────────────────────────────────────────
    # Phase 3：Import Map
    # ──────────────────────────────────────────────────────────────────────

    def _read_imports(self, r: BinaryReader):
        if self.import_count == 0:
            return
        r.seek(self.import_offset)

        is_filter_editor_only = bool(self.package_flags & PKG_FilterEditorOnly)

        for _ in range(self.import_count):
            imp = FObjectImport()
            imp.class_package = self._read_fname(r)
            imp.class_name    = self._read_fname(r)
            imp.outer_index   = r.read_int32()
            imp.object_name   = self._read_fname(r)

            if (self.object_version >= VER_UE4_NON_OUTER_PACKAGE_IMPORT
                    and not is_filter_editor_only):
                imp.package_name = self._read_fname(r)

            if self.object_version_ue5 >= OPTIONAL_RESOURCES:
                imp.import_optional = (r.read_int32() == 1)

            self.imports.append(imp)

    # ──────────────────────────────────────────────────────────────────────
    # Phase 4：Export Map
    # ──────────────────────────────────────────────────────────────────────

    def _read_exports(self, r: BinaryReader):
        if self.export_count == 0:
            return
        r.seek(self.export_offset)

        has_unversioned = bool(self.package_flags & PKG_UnversionedProperties)

        for i in range(self.export_count):
            exp = FObjectExport()
            exp.index = i

            # ── 正确字段顺序（依照 C# Export.cs ReadExportMapEntry） ──
            # 1. ClassIndex → SuperIndex → [TemplateIndex] → OuterIndex → ObjectName → ObjectFlags
            exp.class_index  = r.read_int32()
            exp.super_index  = r.read_int32()
            if self.object_version >= VER_UE4_TemplateIndex_IN_COOKED_EXPORTS:
                exp.template_index = r.read_int32()
            exp.outer_index  = r.read_int32()
            exp.object_name  = self._read_fname(r)
            exp.object_flags = r.read_uint32()

            # 2. SerialSize + SerialOffset
            if self.object_version >= VER_UE4_64BIT_EXPORTMAP_SERIALSIZES:
                exp.serial_size   = r.read_int64()
                exp.serial_offset = r.read_int64()
            else:
                exp.serial_size   = r.read_int32()
                exp.serial_offset = r.read_int32()

            # 3. Forced/NotForClient/NotForServer
            exp.forced_export  = r.read_bool32()
            exp.not_for_client = r.read_bool32()
            exp.not_for_server = r.read_bool32()

            # 4. PackageGuid（UE5 REMOVE_OBJECT_EXPORT_PACKAGE_GUID=6 之前存在）
            if self.object_version_ue5 < REMOVE_OBJECT_EXPORT_PACKAGE_GUID:
                exp.package_guid_bytes = bytes(r.read_bytes(16))

            # 5. IsInheritedInstance（UE5 TRACK_OBJECT_EXPORT_IS_INHERITED=7 起）
            if self.object_version_ue5 >= TRACK_OBJECT_EXPORT_IS_INHERITED:
                exp.is_inherited_instance = r.read_bool32()

            # 6. PackageFlags（始终存在于 export map）
            exp.package_flags = r.read_uint32()

            # 7. NotAlwaysLoadedForEditorGame（obj_ver >= VER_UE4_LOAD_FOR_EDITOR_GAME=365）
            if self.object_version >= VER_UE4_LOAD_FOR_EDITOR_GAME:
                exp.not_always_loaded_for_editor_game = r.read_bool32()

            # 8. IsAsset（obj_ver >= VER_UE4_COOKED_ASSETS_IN_EDITOR_SUPPORT=538）
            if self.object_version >= VER_UE4_COOKED_ASSETS_IN_EDITOR_SUPPORT:
                exp.is_asset = r.read_bool32()

            # 9. GeneratePublicHash（ue5 >= OPTIONAL_RESOURCES=4）
            if self.object_version_ue5 >= OPTIONAL_RESOURCES:
                exp.generate_public_hash = r.read_bool32()

            # 10. PreloadDependencies（obj_ver >= 507）
            if self.object_version >= VER_UE4_PRELOAD_DEPENDENCIES_IN_COOKED_EXPORTS:
                exp.first_export_dependency_offset = r.read_int32()
                exp.serialization_before_serialization_dependencies_size = r.read_int32()
                exp.create_before_serialization_dependencies_size        = r.read_int32()
                exp.serialization_before_create_dependencies_size        = r.read_int32()
                exp.create_before_create_dependencies_size               = r.read_int32()

            # 11. ScriptSerializationOffset（!unversioned && ue5 >= SCRIPT_SERIALIZATION_OFFSET=11）
            if (not has_unversioned
                    and self.object_version_ue5 >= SCRIPT_SERIALIZATION_OFFSET):
                exp.script_serialization_start_offset = r.read_int64()
                exp.script_serialization_end_offset   = r.read_int64()

            self.exports.append(exp)

    # ──────────────────────────────────────────────────────────────────────
    # Phase 5：Soft Object Paths List（UE5 ADD_SOFTOBJECTPATH_LIST）
    # ──────────────────────────────────────────────────────────────────────

    def _read_soft_object_paths(self, r: BinaryReader):
        """
        读取 FSoftObjectPath 列表（UE5 object_version_ue5 >= ADD_SOFTOBJECTPATH_LIST=9）。
        每个 entry：PackageName(FName) + AssetName(FName) + SubPath(FString)。
        用于 SoftObjectProperty 的 int32 索引解析。
        """
        if self.soft_object_paths_offset <= 0:
            return
        r.seek(self.soft_object_paths_offset)
        self.soft_object_paths_list = []
        for _ in range(self.soft_object_paths_count):
            pkg_idx, pkg_num = r.read_fname_raw()
            asset_idx, asset_num = r.read_fname_raw()
            sub = r.read_fstring()
            self.soft_object_paths_list.append({
                'pkg': self._resolve_fname(pkg_idx, pkg_num),
                'asset': self._resolve_fname(asset_idx, asset_num),
                'sub': sub,
            })

    # ──────────────────────────────────────────────────────────────────────
    # 对外接口
    # ──────────────────────────────────────────────────────────────────────

    def toc(self) -> dict:
        return {
            "NameMapCount": self.name_count,
            "ImportCount":  self.import_count,
            "ExportCount":  self.export_count,
            "Exports": [e.to_dict() for e in self.exports],
        }

    def refs(self) -> dict:
        return {
            "Imports": [i.to_dict() for i in self.imports],
        }

    def _ensure_property_modules_loaded(self):
        """确保所有属性类型模块已导入（触发 @register 装饰器）。"""
        import importlib
        # 按依赖顺序导入，确保 base → simple → text → struct → array/map/set
        mods = [
            'uasset_py.properties.simple',
            'uasset_py.properties.text_property',
            'uasset_py.properties.structs.math',
            'uasset_py.properties.structs.core',
            'uasset_py.properties.structs.misc',
            'uasset_py.properties.structs.engine',
            'uasset_py.properties.struct_property',
            'uasset_py.properties.array_property',
            'uasset_py.properties.map_property',
            'uasset_py.properties.set_property',
        ]
        for mod_name in mods:
            try:
                importlib.import_module(mod_name)
            except ImportError:
                pass

    def _get_export_class_name(self, exp_hdr) -> str:
        """获取 export 的 class 名（从 import/export map 中解析）。"""
        cls_idx = exp_hdr.class_index
        if cls_idx < 0:
            imp_idx = -cls_idx - 1
            if 0 <= imp_idx < len(self.imports):
                return self.imports[imp_idx].object_name
        elif cls_idx > 0:
            exp_idx = cls_idx - 1
            if 0 <= exp_idx < len(self.exports):
                return self.exports[exp_idx].object_name
        return ''

    def _convert_export_to_child(self, exp_hdr) -> 'type':
        """
        根据 export 的 class type 名称分发到对应的 Export 子类。

        对应 C# UAsset.ConvertExportToChildExportAndRead 的分发逻辑：
        - 负 class_index → Imports[-idx-1].object_name 就是类型名
        - 正 class_index → Exports[idx-1].object_name
        - 0 → "Class"（UClass 自身）
        """
        from ..exports.normal_export import NormalExport
        from ..exports.data_table_export import DataTableExport
        from ..exports.enum_export import EnumExport
        from ..exports.string_table_export import StringTableExport
        from ..exports.struct_export import StructExport
        from ..exports.function_export import FunctionExport
        from ..exports.class_export import ClassExport
        from ..exports.level_export import LevelExport

        cls_type = self._get_export_class_name(exp_hdr)

        # 精确匹配
        if cls_type == 'Level':
            return LevelExport
        if cls_type in ('Enum', 'UserDefinedEnum'):
            return EnumExport
        if cls_type == 'Function':
            return FunctionExport
        if cls_type in ('UserDefinedStruct', 'ScriptStruct'):
            return StructExport
        if cls_type == 'Class':
            # class_index == 0 → 是 UClass 本身
            return ClassExport

        # 后缀匹配
        if cls_type.endswith('DataTable'):
            return DataTableExport
        if cls_type.endswith('StringTable'):
            return StringTableExport
        if cls_type.endswith('BlueprintGeneratedClass'):
            return ClassExport

        return NormalExport

    def parse_export_bodies(self):
        """
        解析所有 export data body，填充 _export_objects。
        每个 export 独立容错：失败则保存为 RawExport。

        Phase 2：根据 class type 分发到对应的 Export 子类
        （DataTableExport、EnumExport、StringTableExport、
          StructExport、FunctionExport、ClassExport、LevelExport）
        """
        self._ensure_property_modules_loaded()

        from ..exports.export import RawExport

        self._export_objects = []
        data = self._data
        r = BinaryReader(data)

        for i, exp_hdr in enumerate(self.exports):
            if exp_hdr.serial_size <= 0 or exp_hdr.serial_offset <= 0:
                raw = RawExport(exp_hdr)
                raw._error = "Zero serial size or offset"
                self._export_objects.append(raw)
                continue

            # 计算 next export 起始位置
            next_starting = -1
            if i + 1 < len(self.exports):
                next_exp = self.exports[i + 1]
                if next_exp.serial_offset > 0:
                    next_starting = next_exp.serial_offset

            # 分发到对应 Export 子类
            export_cls = self._convert_export_to_child(exp_hdr)

            try:
                r.seek(exp_hdr.serial_offset)
                exp_obj = export_cls(exp_hdr)
                exp_obj.read(r, self, next_starting)
                self._export_objects.append(exp_obj)
            except Exception as e:
                # 解析失败 → RawExport
                raw = RawExport(exp_hdr)
                raw._error = str(e)
                try:
                    r.seek(exp_hdr.serial_offset)
                    raw.raw_data = bytes(r.read_bytes(min(exp_hdr.serial_size, r.remaining())))
                except Exception:
                    raw.raw_data = b''
                self._export_objects.append(raw)

    def serialize_json(self) -> dict:
        """
        输出完整 JSON（兼容 UAssetAPI 格式）。
        若 _export_objects 为空，先调用 parse_export_bodies()。
        """
        if not self._export_objects:
            self.parse_export_bodies()

        imports_json = []
        for imp in self.imports:
            imports_json.append({
                '$type': 'UAssetAPI.Import, UAssetAPI',
                'ClassPackage': imp.class_package,
                'ClassName': imp.class_name,
                'OuterIndex': imp.outer_index,
                'ObjectName': imp.object_name,
                'PackageName': imp.package_name,
                'bImportOptional': imp.import_optional,
            })

        exports_json = []
        for exp_obj in self._export_objects:
            if hasattr(exp_obj, 'to_json'):
                try:
                    exports_json.append(exp_obj.to_json(self))
                except Exception as e:
                    exports_json.append({'_error': str(e)})
            else:
                exports_json.append(exp_obj.to_dict())

        result = {
            '$schema': 'https://github.com/atenfyr/UAssetAPI/blob/master/Schema/schema.json',
            'Info': 'Serialized with uasset_py (Python)',
            'NameMap': list(self.name_map),
            'Imports': imports_json,
            'Exports': exports_json,
        }

        return result
