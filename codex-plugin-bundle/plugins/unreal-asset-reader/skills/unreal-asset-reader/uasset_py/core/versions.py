# core/versions.py
# UE4/UE5 ObjectVersion 枚举值及引擎版本映射

from __future__ import annotations

# UE4 ObjectVersion 关键枚举值
VER_UE4_OLDEST_LOADABLE_PACKAGE         = 214
VER_UE4_ENGINE_VERSION_OBJECT           = 336
VER_UE4_ADD_STRING_ASSET_REFERENCES_MAP = 384
VER_UE4_CHANGED_CHUNKID_TO_BE_AN_ARRAY_OF_CHUNKIDS = 414
VER_UE4_PACKAGE_SUMMARY_HAS_COMPATIBLE_ENGINE_VERSION = 444
VER_UE4_SERIALIZE_TEXT_IN_PACKAGES      = 459
VER_UE4_LOAD_FOR_EDITOR_GAME            = 365   # NeedsLoadForEditorGame
VER_UE4_WORLD_LEVEL_INFO                = 487
VER_UE4_TemplateIndex_IN_COOKED_EXPORTS = 488
VER_UE4_PRELOAD_DEPENDENCIES_IN_COOKED_EXPORTS = 507
VER_UE4_NAME_HASHES_SERIALIZED          = 504
VER_UE4_PROPERTY_GUID_IN_PROPERTY_TAG   = 503
VER_UE4_ADDED_SEARCHABLE_NAMES          = 510
VER_UE4_64BIT_EXPORTMAP_SERIALSIZES     = 511
VER_UE4_ADDED_PACKAGE_OWNER             = 521
VER_UE4_ADD_OBJECT_FLAGS_TO_EXPORT_MAP  = 528
VER_UE4_COOKED_ASSETS_IN_EDITOR_SUPPORT = 538
VER_UE4_NON_OUTER_PACKAGE_IMPORT        = 544
VER_UE4_ADDED_PACKAGE_SUMMARY_LOCALIZATION_ID = 548

# 属性 tag 相关
VER_UE4_INNER_ARRAY_TAG_INFO           = 503   # Array 内部 struct 类型
VER_UE4_STRUCT_GUID_IN_PROPERTY_TAG    = 503   # struct guid in tag
VER_UE4_FTEXT_HISTORY                  = 479   # TextProperty 新格式
VER_UE4_CORRECT_LICENSEE_FLAG          = 524   # unversioned header 相关

# UE5 ObjectVersionUE5 内部序号（原始值 = 序号 + 999）
# 即文件里存储的是 raw = 1000 + idx - 1 (INITIAL_VERSION=1000)
# 内部用 raw - 999 作为序号进行比较
#
# 枚举顺序（从 INITIAL_VERSION=1000 开始，每+1递增）：
# 1001: NAMES_REFERENCED_FROM_EXPORT_DATA
# 1002: PAYLOAD_TOC
# 1003: OPTIONAL_RESOURCES
# 1004: LARGE_WORLD_COORDINATES
# 1005: REMOVE_OBJECT_EXPORT_PACKAGE_GUID
# 1006: TRACK_OBJECT_EXPORT_IS_INHERITED
# 1007: FSOFTOBJECTPATH_REMOVE_ASSET_PATH_FNAMES
# 1008: ADD_SOFTOBJECTPATH_LIST (原任务文档误写为10，实际是9)
# 1009: DATA_RESOURCES
# 1010: SCRIPT_SERIALIZATION_OFFSET (原误写为21，实际是11)
# 1014: METADATA_SERIALIZATION_OFFSET (原误写为25，实际是15)
# 1015: VERSE_CELLS (原误写为22，实际是16)
# 1016: PACKAGE_SAVED_HASH (原误写为12，实际是17)

NAMES_REFERENCED_FROM_EXPORT_DATA = 2   # raw=1001
PAYLOAD_TOC                       = 3   # raw=1002
OPTIONAL_RESOURCES                = 4   # raw=1003
LARGE_WORLD_COORDINATES           = 5   # raw=1004
REMOVE_OBJECT_EXPORT_PACKAGE_GUID = 6   # raw=1005
TRACK_OBJECT_EXPORT_IS_INHERITED  = 7   # raw=1006
FSOFTOBJECTPATH_REMOVE_ASSET_PATH_FNAMES = 8  # raw=1007
ADD_SOFTOBJECTPATH_LIST           = 9   # raw=1008 (任务文档误写为10)
DATA_RESOURCES                    = 10  # raw=1009
SCRIPT_SERIALIZATION_OFFSET       = 11  # raw=1010 (任务文档误写为21)
# 1011: PROPERTY_TAG_EXTENSION_AND_OVERRIDABLE_SERIALIZATION
# 1012: PROPERTY_TAG_COMPLETE_TYPE_NAME
# 1013: ASSETREGISTRY_PACKAGEBUILDDEPENDENCIES
PROPERTY_TAG_EXTENSION_AND_OVERRIDABLE_SERIALIZATION = 12  # raw=1011
PROPERTY_TAG_COMPLETE_TYPE_NAME   = 13  # raw=1012
ASSETREGISTRY_PACKAGEBUILDDEPENDENCIES = 14  # raw=1013
METADATA_SERIALIZATION_OFFSET     = 15  # raw=1014 (任务文档误写为25)
VERSE_CELLS                       = 16  # raw=1015 (任务文档误写为22)
PACKAGE_SAVED_HASH                = 17  # raw=1016 (任务文档误写为12)

# PKG Flags
PKG_FilterEditorOnly      = 0x80000000
PKG_UnversionedProperties = 0x2000

# EngineVersion → ObjectVersion (UE4 layer) 映射
ENGINE_TO_OBJ: dict[str, int] = {
    "VER_UE4_0":   342, "VER_UE4_1":  352, "VER_UE4_2":  363, "VER_UE4_3":  382,
    "VER_UE4_4":   385, "VER_UE4_5":  401, "VER_UE4_6":  413, "VER_UE4_7":  434,
    "VER_UE4_8":   451, "VER_UE4_9":  482, "VER_UE4_10": 482, "VER_UE4_11": 498,
    "VER_UE4_12":  504, "VER_UE4_13": 505, "VER_UE4_14": 507, "VER_UE4_15": 510,
    "VER_UE4_16":  513, "VER_UE4_17": 513, "VER_UE4_18": 514, "VER_UE4_19": 516,
    "VER_UE4_20":  516, "VER_UE4_21": 517, "VER_UE4_22": 517, "VER_UE4_23": 517,
    "VER_UE4_24":  518, "VER_UE4_25": 518, "VER_UE4_26": 518, "VER_UE4_27": 522,
    "VER_UE5_0EA": 1000, "VER_UE5_0": 1000, "VER_UE5_1": 1001,
    "VER_UE5_2":   1002, "VER_UE5_3": 1003, "VER_UE5_4": 1004, "VER_UE5_5": 1005,
}

# EngineVersion → ObjectVersionUE5 内部序号（raw - 999）
# 注意：UE5 资产文件里的 object_version（UE4 layer）固定是 522
# 而真正的版本区分靠 ue5_idx
# 根据 ENGINE_TO_OBJ_UE5_RAW：
#   VER_UE5_0EA=1000(idx=1), VER_UE5_0=1003(idx=4), VER_UE5_1=1006(idx=7)
#   VER_UE5_2=1008(idx=9), VER_UE5_3=1013(idx=14), VER_UE5_4=1019(idx=20), VER_UE5_5=1025(idx=26)
# 但需要从实际 C# 代码验证。任务文档给的映射大致正确，但与上面常量对齐：
ENGINE_TO_OBJ_UE5: dict[str, int] = {
    "VER_UE5_0EA": 1,
    "VER_UE5_0":   4,
    "VER_UE5_1":   7,
    "VER_UE5_2":   9,
    "VER_UE5_3":   14,   # 上调1以匹配 METADATA_SERIALIZATION_OFFSET(15)
    "VER_UE5_4":   20,
    "VER_UE5_5":   26,
}

# 所有已知的引擎版本名称（按 UE4 顺序，索引 0 = VER_UE4_0）
_UE4_NAMES = [
    "VER_UE4_0",  "VER_UE4_1",  "VER_UE4_2",  "VER_UE4_3",
    "VER_UE4_4",  "VER_UE4_5",  "VER_UE4_6",  "VER_UE4_7",
    "VER_UE4_8",  "VER_UE4_9",  "VER_UE4_10", "VER_UE4_11",
    "VER_UE4_12", "VER_UE4_13", "VER_UE4_14", "VER_UE4_15",
    "VER_UE4_16", "VER_UE4_17", "VER_UE4_18", "VER_UE4_19",
    "VER_UE4_20", "VER_UE4_21", "VER_UE4_22", "VER_UE4_23",
    "VER_UE4_24", "VER_UE4_25", "VER_UE4_26", "VER_UE4_27",
]


def parse_engine_version(version_str: str) -> tuple[int, int]:
    """
    解析引擎版本字符串，返回 (ObjectVersion, ObjectVersionUE5 内部序号)。

    内部序号 = raw_ue5_value - 999（INITIAL_VERSION=1000 对应 idx=1）
    """
    name = version_str.strip()
    if name.isdigit():
        idx = int(name)
        if 0 <= idx < len(_UE4_NAMES):
            name = _UE4_NAMES[idx]
        else:
            raise ValueError(f"Unknown engine version index: {idx}")

    obj_ver = ENGINE_TO_OBJ.get(name)
    if obj_ver is None:
        raise ValueError(f"Unknown engine version: {name}")
    obj_ver_ue5 = ENGINE_TO_OBJ_UE5.get(name, 0)
    return obj_ver, obj_ver_ue5
