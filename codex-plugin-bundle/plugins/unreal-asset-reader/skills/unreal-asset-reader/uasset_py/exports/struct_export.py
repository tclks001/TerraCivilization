# exports/struct_export.py
# StructExport - 解析 UE UStruct/UScriptStruct/UserDefinedStruct
#
# 设计说明：
# StructExport 的 body 格式（UAsset.cs StructExport.Read 参考）：
#   1. NormalExport 属性列表（super().read()）
#   2. super_struct: int32 (FPackageIndex)
#   3. children FPackageIndex 数组（两种格式，由 FFrameworkObjectVersion 决定）
#   4. loaded_properties（FProperty 链，由 FCoreObjectVersion.FProperties 决定）
#   5. script_bytecode_size: int32
#   6. script_bytecode_raw bytes（Phase 2 直接存 raw，Phase 3 再解析 Kismet）
#
# FProperty 读取：Phase 2 简化，仅读取公共字段（类型名、名称、ArrayDim、PropertyFlags、
# RepNotifyFunc、BlueprintReplicationCondition、MetaData），不解析子类特定字段。
# 解析失败时存储 raw bytes 兜底，不影响整体流程。
#
# 容错策略：
# - FProperty 解析失败 → 停止读 FProperty 链，存已读部分
# - 字节码 Phase 2 直接存 raw bytes，不报错

from __future__ import annotations
from typing import TYPE_CHECKING, List, Optional

from .normal_export import NormalExport
from ..kismet.tokens import EExprToken

if TYPE_CHECKING:
    from ..core.reader import BinaryReader
    from ..core.uasset import UAsset
    from ..core.import_export import FObjectExport


# FFrameworkObjectVersion GUID: {CFFC743F-43B0-4480-9391-14DF171D2073}
# FGuid 存储为 4 个 uint32 LE：A=CFFC743F, B=43B04480, C=939114DF, D=171D2073
_FRAMEWORK_OBJECT_VERSION_KEY = (0xCFFC743F, 0x43B04480, 0x939114DF, 0x171D2073)

# FFrameworkObjectVersion.RemoveUField_Next = 17
_REMOVE_UFIELD_NEXT_VERSION = 17

# FCoreObjectVersion GUID: {9F8BF812-FC4A-8CF7-D4D0-0C837E928BDA}
# FGuid 存储为 4 个 uint32 LE：A=9F8BF812, B=FC4A8CF7, C=D4D00C83, D=7E928BDA
_CORE_OBJECT_VERSION_KEY = (0x9F8BF812, 0xFC4A8CF7, 0xD4D00C83, 0x7E928BDA)

# FCoreObjectVersion.FProperties = 9
_FPROPERTIES_VERSION = 9


def _get_custom_version(asset: 'UAsset', key_tuple: tuple) -> int:
    """查询指定 GUID 在 asset custom_versions 中的值。"""
    a0, a1, a2, a3 = key_tuple
    for cv in asset.custom_versions:
        g = cv.key
        if g.a == a0 and g.b == a1 and g.c == a2 and g.d == a3:
            return cv.version
    return 0


def _read_fproperty_raw(reader: 'BinaryReader', asset: 'UAsset') -> Optional[dict]:
    """
    Phase 2/3 简化版 FProperty 读取。

    UE FProperty 公共字段序列化顺序：
    1. SerializedType (FName) - FProperty 子类类型名（如 "StrProperty"）
    2. NamePrivate (FName) - 字段名称
    3. Flags (EObjectFlags uint32) - 对象标志
    4. ArrayDim (int32) - 数组维度
    5. ElementSize (int32) - 单元素大小
    6. PropertyFlags (EPropertyFlags uint64) - 属性标志
    7. RepIndex (uint16) - 复制索引
    8. RepNotifyFunc (FName) - 网络复制通知函数名
    9. BlueprintReplicationCondition (uint8) - 蓝图复制条件
    10. MetaData TMap<FName, FString> - 元数据

    Phase 3 修正：加入 Flags(uint32) + ElementSize(int32) + RepIndex(uint16)，
    与 UAssetAPI FProperty.Read() 的实际字段顺序对齐。

    Phase 3 策略：读取公共字段，不解析子类特定字段（如 ObjectProperty 的 PropertyClass）。
    整个 FProperty chain 用 try/except 包裹，失败时 raw fallback。
    """
    try:
        # FProperty class name（表示这是哪种 FProperty，如 "FloatProperty"）
        prop_class = asset._read_fname(reader)

        # FField::NamePrivate
        name = asset._read_fname(reader)

        # FField::FlagsPrivate (EObjectFlags uint32)
        flags = reader.read_uint32()

        # FProperty 公共字段
        array_dim = reader.read_int32()
        element_size = reader.read_int32()
        property_flags = reader.read_uint64()
        rep_index = reader.read_uint16()
        rep_notify_func = asset._read_fname(reader)
        blueprint_replication_condition = reader.read_uint8()

        # MetaData: TMap<FName, FString>
        meta_count = reader.read_int32()
        meta_data = {}
        for _ in range(meta_count):
            k = asset._read_fname(reader)
            v = reader.read_fstring() or ''
            meta_data[k] = v

        return {
            'PropertyClass': prop_class,
            'Name': name,
            'Flags': flags,
            'ArrayDim': array_dim,
            'ElementSize': element_size,
            'PropertyFlags': property_flags,
            'RepIndex': rep_index,
            'RepNotifyFunc': rep_notify_func,
            'BlueprintReplicationCondition': blueprint_replication_condition,
            'MetaData': meta_data,
        }
    except Exception:
        return None


class StructExport(NormalExport):
    """
    UStruct export 解析器（UScriptStruct / UserDefinedStruct 等）。

    字段：
    - super_struct: int（FPackageIndex）
    - children: list[int]（FPackageIndex 数组）
    - loaded_properties: list[dict]（FProperty 链简化表示，Phase 2）
    - script_bytecode_size: int
    - script_bytecode_raw: bytes（Phase 2 raw；Phase 3 做 Kismet 解析）

    为什么 Phase 2 用 raw：
    Kismet 字节码（UE 蓝图 VM 指令）有复杂的递归结构，
    需要专门的 ExpressionSerializer 支持，Phase 3 单独实现。
    """
    __slots__ = ('_header', 'properties', 'object_guid', '_parse_errors', '_raw_data',
                 'super_struct', 'children', 'loaded_properties',
                 'script_bytecode_size', 'script_bytecode_raw', 'script_bytecode')

    def __init__(self, header: 'FObjectExport'):
        super().__init__(header)
        self.super_struct: int = 0
        self.children: List[int] = []
        self.loaded_properties: List[dict] = []
        self.script_bytecode_size: int = 0
        self.script_bytecode_raw: Optional[bytes] = None
        self.script_bytecode: Optional[List] = None  # Phase 3 Kismet 解析结果

    def read(self, reader: 'BinaryReader', asset: 'UAsset', next_starting: int = -1):
        try:
            # Step 1: NormalExport 属性列表
            super()._do_read(reader, asset, next_starting)

            # Step 2: 读 StructExport 特定字段
            self._read_struct_data(reader, asset)

        except Exception as e:
            self._parse_errors.append(f"StructExport read failed: {e}")

    def _read_struct_data(self, reader: 'BinaryReader', asset: 'UAsset'):
        framework_ver = _get_custom_version(asset, _FRAMEWORK_OBJECT_VERSION_KEY)
        core_ver = _get_custom_version(asset, _CORE_OBJECT_VERSION_KEY)

        # super_struct (FPackageIndex)
        self.super_struct = reader.read_int32()

        # children
        if framework_ver >= _REMOVE_UFIELD_NEXT_VERSION:
            # 新格式（UE4.25+）：显式数组
            n = reader.read_int32()
            self.children = [reader.read_int32() for _ in range(n)]
        else:
            # 旧格式：单个 first_child（链表头，0 = null）
            first_child = reader.read_int32()
            self.children = [first_child] if first_child != 0 else []

        # loaded_properties（FProperty 链）
        # 版本判断：
        #   UE4 路径：需要 FCoreObjectVersion.FProperties >= 9
        #   UE5 路径：从 UE5 开始 FProperty 链总是被序列化（不依赖 custom version）
        # 注：Phase 2 简化读取，复杂 FProperty 子类可能读偏
        # → 用整体 try/except，失败后用 raw bytes 兜底
        has_fproperties = (core_ver >= _FPROPERTIES_VERSION) or (asset.object_version_ue5 > 0)
        if has_fproperties:
            fp_start = reader.position
            fp_ok = True
            try:
                n = reader.read_int32()
                for i in range(n):
                    fp = _read_fproperty_raw(reader, asset)
                    if fp is not None:
                        self.loaded_properties.append(fp)
                    else:
                        self._parse_errors.append(
                            f"FProperty[{i}] parse failed, stopping"
                        )
                        fp_ok = False
                        break
            except Exception as e:
                self._parse_errors.append(f"FProperty chain read error: {e}")
                fp_ok = False

            if not fp_ok:
                # FProperty 读取失败：整个 FProperty+ByteCode 区域作 raw
                # 我们把已读但不完整的状态放弃，直接存剩余字节
                remaining = reader.remaining()
                if remaining > 0:
                    self.script_bytecode_raw = bytes(reader.read_bytes(remaining))
                return  # 不再继续读 bytecode

        # script_bytecode
        # UStruct::Serialize 序列化两个大小字段：
        #   ScriptBytecodeSize (int32) - VM 内存展开后大小（无意义于解析）
        #   ScriptStorageSize  (int32) - 实际存储的字节数（这才是 read_size）
        self.script_bytecode_size = reader.read_int32()
        script_storage_size = reader.read_int32()
        read_size = script_storage_size
        if read_size > 0:
                start_pos = reader.position
                # Phase 3：调用 Kismet 解析器
                try:
                    from ..kismet.expression import read_expression
                    self.script_bytecode = []
                    end_pos = start_pos + read_size
                    while reader.position < end_pos:
                        expr = read_expression(reader, asset)
                        self.script_bytecode.append(expr)
                        if expr.token == EExprToken.EX_EndOfScript:
                            break
                    # 如果还剩字节（解析完 EndOfScript 后正常），跳到 end_pos
                    if reader.position < end_pos:
                        reader.seek(end_pos)
                    self.script_bytecode_raw = None
                except Exception as e:
                    # fallback：存 raw bytes，不影响整体解析
                    self._parse_errors.append(
                        f"Kismet parse fallback at pos {start_pos}: {e}"
                    )
                    reader.seek(start_pos)
                    self.script_bytecode_raw = bytes(reader.read_bytes(read_size))
                    self.script_bytecode = None

    def to_json(self, asset: 'UAsset') -> dict:
        import base64
        d = super().to_json(asset)
        d['$type'] = 'UAssetAPI.ExportTypes.StructExport, UAssetAPI'
        d['SuperStruct'] = self.super_struct
        d['Children'] = self.children
        d['LoadedProperties'] = self.loaded_properties
        d['ScriptBytecodeSize'] = self.script_bytecode_size
        if self.script_bytecode is not None:
            # Phase 3：输出 Kismet 解析结果
            d['ScriptBytecode'] = [expr.to_dict() for expr in self.script_bytecode]
        elif self.script_bytecode_raw:
            # Fallback：raw bytes 以 base64 输出
            d['ScriptBytecodeRaw'] = base64.b64encode(self.script_bytecode_raw).decode('ascii')
        else:
            d['ScriptBytecode'] = []
        return d
