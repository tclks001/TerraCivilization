# core/types.py
# 基础数据类型定义，全部使用 __slots__ 以节省内存


class FGuid:
    """16字节 GUID"""
    __slots__ = ('a', 'b', 'c', 'd')

    def __init__(self, a: int, b: int, c: int, d: int):
        self.a = a
        self.b = b
        self.c = c
        self.d = d

    def __repr__(self) -> str:
        return f"{self.a:08X}-{self.b:08X}-{self.c:08X}-{self.d:08X}"

    def to_dict(self) -> dict:
        return {"A": self.a, "B": self.b, "C": self.c, "D": self.d}


class FEngineVersion:
    """引擎版本记录"""
    __slots__ = ('major', 'minor', 'patch', 'changelist', 'branch')

    def __init__(self, major: int, minor: int, patch: int, changelist: int, branch: str):
        self.major = major
        self.minor = minor
        self.patch = patch
        self.changelist = changelist
        self.branch = branch

    def __repr__(self) -> str:
        return f"{self.major}.{self.minor}.{self.patch}-{self.changelist}+{self.branch}"


class FPackageIndex:
    """
    包内对象引用索引：
      > 0 → export[index-1]
      < 0 → import[-index-1]
      == 0 → null（根包）
    """
    __slots__ = ('index',)

    def __init__(self, index: int):
        self.index = index

    @property
    def is_null(self) -> bool:
        return self.index == 0

    @property
    def is_export(self) -> bool:
        return self.index > 0

    @property
    def is_import(self) -> bool:
        return self.index < 0

    @property
    def export_index(self) -> int:
        """返回 export 数组下标（0-based），仅 is_export 时有效"""
        return self.index - 1

    @property
    def import_index(self) -> int:
        """返回 import 数组下标（0-based），仅 is_import 时有效"""
        return -self.index - 1

    def __repr__(self) -> str:
        if self.is_null:
            return "Null"
        if self.is_export:
            return f"Export[{self.export_index}]"
        return f"Import[{self.import_index}]"
