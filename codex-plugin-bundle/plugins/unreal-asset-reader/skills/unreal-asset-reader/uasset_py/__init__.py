"""
uasset_py - UAsset Python Parser
Phase 0: header + toc/refs
"""

from .core.uasset import UAsset
from .core.versions import parse_engine_version

__version__ = "0.1.0"
__all__ = ["UAsset", "parse_engine_version"]
