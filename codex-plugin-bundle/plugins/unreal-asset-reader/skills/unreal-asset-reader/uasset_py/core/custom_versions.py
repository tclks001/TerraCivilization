# core/custom_versions.py
# 自定义版本容器解析

from __future__ import annotations
from dataclasses import dataclass
from typing import Optional
from .types import FGuid


@dataclass
class FCustomVersion:
    """单条自定义版本记录"""
    __slots__ = ('key', 'version', 'friendly_name')

    key: FGuid
    version: int
    friendly_name: Optional[str]  # Guids 格式有，Optimized 格式无
