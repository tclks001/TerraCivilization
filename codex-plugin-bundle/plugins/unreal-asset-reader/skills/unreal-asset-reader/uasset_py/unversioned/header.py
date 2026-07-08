# unversioned/header.py
# FUnversionedHeader - unversioned 资产的属性 schema header
#
# 设计说明：
# Unversioned 属性不使用属性名和类型标签，而是通过 .usmap 文件的 schema
# 来确定每个属性的类型和位置。header 本身只记录哪些属性存在（fragment chain）
# 以及哪些属性是 "zero"（默认值，不需要读取实际数据）。
#
# Fragment 结构（每个 fragment = uint16）：
#   [15:9] = SkipNum (7bit)  - 跳过的属性数量（默认值/缺失属性）
#   [8:2]  = ValueNum (7bit) - 本 fragment 包含的属性数量
#   [1]    = bHasAnyZeroes   - 本 fragment 是否有 zero 属性
#   [0]    = bIsLast         - 是否是最后一个 fragment
#
# Zero mask 编码：
#   ZeroMaskNum <= 8  → 1 byte
#   ZeroMaskNum <= 16 → 2 bytes（uint16）
#   else             → ceil(ZeroMaskNum/32) 个 int32

from __future__ import annotations
from typing import TYPE_CHECKING, List
import math

if TYPE_CHECKING:
    from ..core.reader import BinaryReader


class FFragment:
    """单个 unversioned header fragment"""
    __slots__ = ('skip_num', 'value_num', 'has_any_zeroes', 'is_last', 'first_num')

    def __init__(self, raw: int):
        # fragment bits 解包：按规范定义
        self.skip_num       = (raw >> 9) & 0x7F
        self.value_num      = (raw >> 2) & 0x7F
        self.has_any_zeroes = bool(raw & 2)
        self.is_last        = bool(raw & 1)
        self.first_num      = 0  # 由 FUnversionedHeader 填充（累计 prop index）


class FUnversionedHeader:
    """
    FUnversionedHeader 解析。

    提供的主要接口：
    - fragments: fragment 列表（包含位置信息）
    - zero_mask_bits: 各属性是否为 zero 的位数组
    - has_non_zero_values: 是否有需要实际读取的属性
    - property_count: 总属性数量（用于 usmap schema 查询）

    unversioned 属性读取流程：
    1. 读 FUnversionedHeader
    2. 遍历 fragments，对每个 fragment：
       a. 跳过 skip_num 个属性（默认值）
       b. 读取 value_num 个属性：
          - 如果 has_any_zeroes 且 zero_mask[zero_idx] = 1 → 跳过（zero 属性）
          - 否则从 reader 读取实际值
    3. 通过 usmap schema 确定每个属性的名字和类型
    """
    __slots__ = ('fragments', 'zero_mask_bits', 'has_non_zero_values',
                 'schema_iter_state')

    def __init__(self):
        self.fragments: List[FFragment] = []
        self.zero_mask_bits: List[bool] = []   # 每个 value slot 是否为 zero
        self.has_non_zero_values: bool = False

    @classmethod
    def read(cls, reader: 'BinaryReader') -> 'FUnversionedHeader':
        hdr = cls()

        # 1. 读取 fragment 链
        total_skip = 0
        total_value = 0
        zero_mask_num = 0

        first_num = 0
        while True:
            raw = reader.read_uint16()
            frag = FFragment(raw)
            frag.first_num = first_num + frag.skip_num
            first_num = frag.first_num + frag.value_num
            hdr.fragments.append(frag)

            if frag.has_any_zeroes:
                zero_mask_num += frag.value_num

            if frag.is_last:
                break

        # 2. 检查是否有非 zero 属性
        total_values_with_zeros = sum(f.value_num for f in hdr.fragments)
        hdr.has_non_zero_values = (total_values_with_zeros > zero_mask_num)

        # 3. 读 zero mask
        if zero_mask_num > 0:
            hdr.zero_mask_bits = hdr._read_zero_mask(reader, zero_mask_num)
        else:
            hdr.zero_mask_bits = []

        return hdr

    @staticmethod
    def _read_zero_mask(reader: 'BinaryReader', num: int) -> List[bool]:
        """读取 zero mask，返回 bool 列表（True=is_zero）。"""
        if num <= 8:
            raw = reader.read_uint8()
            n_bytes = 1
        elif num <= 16:
            raw = reader.read_uint16()
            n_bytes = 2
        else:
            # ceil(num/32) 个 int32
            n_ints = math.ceil(num / 32)
            raw = 0
            for i in range(n_ints):
                chunk = reader.read_uint32()
                raw |= (chunk << (i * 32))
            n_bytes = n_ints * 4

        return [bool((raw >> i) & 1) for i in range(num)]

    def get_property_iterator(self):
        """
        返回迭代器，生成 (schema_index, is_zero) 对。

        schema_index：属性在 usmap schema 中的索引（从 0 开始）。
        is_zero：True 表示为 zero 值（不读数据，使用默认值）。
        """
        zero_idx = 0
        for frag in self.fragments:
            # skip_num 个属性不存在（默认值，不输出）
            # value_num 个属性需要处理
            for i in range(frag.value_num):
                schema_idx = frag.first_num + i
                if frag.has_any_zeroes:
                    is_zero = self.zero_mask_bits[zero_idx] if zero_idx < len(self.zero_mask_bits) else False
                    zero_idx += 1
                else:
                    is_zero = False
                yield schema_idx, is_zero
