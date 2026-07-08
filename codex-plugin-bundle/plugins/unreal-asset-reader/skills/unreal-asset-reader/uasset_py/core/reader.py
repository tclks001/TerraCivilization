# core/reader.py
# 高性能二进制读取器，使用 memoryview + 预编译 Struct

from __future__ import annotations
import struct
from typing import Optional

# 预编译常用格式（小端）
_S_INT8   = struct.Struct('<b')
_S_UINT8  = struct.Struct('<B')
_S_INT16  = struct.Struct('<h')
_S_UINT16 = struct.Struct('<H')
_S_INT32  = struct.Struct('<i')
_S_UINT32 = struct.Struct('<I')
_S_INT64  = struct.Struct('<q')
_S_UINT64 = struct.Struct('<Q')
_S_FLOAT  = struct.Struct('<f')
_S_DOUBLE = struct.Struct('<d')
_S_GUID   = struct.Struct('<4I')  # 4x uint32


class BinaryReader:
    """
    基于 memoryview 的零拷贝二进制读取器。

    设计选择：
    - memoryview 避免 bytes 切片时的内存拷贝，对大文件有明显性能收益
    - struct.Struct 预编译格式字符串，避免每次调用重新解析
    - 位置管理用 Python int，避免 ctypes 开销
    """
    __slots__ = ('_mv', '_pos', '_size')

    def __init__(self, data: bytes | bytearray | memoryview):
        if isinstance(data, memoryview):
            self._mv = data
        else:
            self._mv = memoryview(data).cast('B')
        self._pos = 0
        self._size = len(self._mv)

    # ── 位置控制 ──────────────────────────────────────────

    @property
    def position(self) -> int:
        return self._pos

    @position.setter
    def position(self, value: int):
        self._pos = value

    def seek(self, pos: int):
        self._pos = pos

    def skip(self, count: int):
        self._pos += count

    def tell(self) -> int:
        return self._pos

    def remaining(self) -> int:
        return self._size - self._pos

    # ── 低层读取 ──────────────────────────────────────────

    def read_bytes(self, count: int) -> memoryview:
        end = self._pos + count
        if end > self._size:
            raise EOFError(f"Read beyond EOF: pos={self._pos}, count={count}, size={self._size}")
        chunk = self._mv[self._pos:end]
        self._pos = end
        return chunk

    def read_bytes_copy(self, count: int) -> bytes:
        return bytes(self.read_bytes(count))

    def _read1(self, s: struct.Struct):
        end = self._pos + s.size
        if end > self._size:
            raise EOFError(f"Unexpected end of file reading {s.size} bytes at pos {self._pos} (size {self._size})")
        val = s.unpack_from(self._mv, self._pos)[0]
        self._pos = end
        return val

    def _read_n(self, s: struct.Struct):
        end = self._pos + s.size
        if end > self._size:
            raise EOFError(f"Unexpected end of file reading {s.size} bytes at pos {self._pos} (size {self._size})")
        vals = s.unpack_from(self._mv, self._pos)
        self._pos = end
        return vals

    # ── 标量读取 ──────────────────────────────────────────

    def read_int8(self)   -> int: return self._read1(_S_INT8)
    def read_uint8(self)  -> int: return self._read1(_S_UINT8)
    def read_int16(self)  -> int: return self._read1(_S_INT16)
    def read_uint16(self) -> int: return self._read1(_S_UINT16)
    def read_int32(self)  -> int: return self._read1(_S_INT32)
    def read_uint32(self) -> int: return self._read1(_S_UINT32)
    def read_int64(self)  -> int: return self._read1(_S_INT64)
    def read_uint64(self) -> int: return self._read1(_S_UINT64)
    def read_float(self)  -> float: return self._read1(_S_FLOAT)
    def read_double(self) -> float: return self._read1(_S_DOUBLE)
    def read_bool32(self) -> bool: return self._read1(_S_INT32) != 0

    # ── 复合类型 ──────────────────────────────────────────

    def read_guid(self):
        """读取 FGuid（4 x uint32），返回 (a, b, c, d)"""
        return self._read_n(_S_GUID)

    def read_fstring(self) -> Optional[str]:
        """
        读取 UE FString：
        - length == 0 → None
        - length > 0  → UTF-8（含末尾 null）
        - length < 0  → UTF-16LE（abs(length) 个 wchar，含末尾 null）
        """
        length = self._read1(_S_INT32)
        if length == 0:
            return None
        if length > 0:
            raw = bytes(self.read_bytes(length))
            return raw[:-1].decode('utf-8', errors='replace')
        else:
            byte_count = (-length) * 2
            raw = bytes(self.read_bytes(byte_count))
            return raw[:-2].decode('utf-16-le', errors='replace')

    def read_fname_raw(self) -> tuple[int, int]:
        """读取 FName 的原始索引对 (name_map_idx, number)"""
        idx = self._read1(_S_INT32)
        num = self._read1(_S_INT32)
        return idx, num
