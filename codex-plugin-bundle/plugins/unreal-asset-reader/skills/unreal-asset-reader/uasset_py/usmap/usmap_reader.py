# usmap/usmap_reader.py
# Usmap 读取器（Phase 1 最小 stub）

from __future__ import annotations
from pathlib import Path
from .usmap import Usmap


def load_usmap(path: str) -> Usmap:
    """从文件路径加载 .usmap 文件。Phase 1 返回空 Usmap。"""
    return Usmap(path)
