# properties/registry.py
# 属性类注册表（装饰器模式，自动注册）

from __future__ import annotations
from typing import TYPE_CHECKING, Optional, Type

if TYPE_CHECKING:
    from .base import PropertyData

_REGISTRY: dict[str, Type['PropertyData']] = {}


def register(prop_type_name: str):
    """
    装饰器：将 PropertyData 子类注册到全局注册表。

    选择装饰器而非手动注册的原因：
    - 每个属性类自描述，避免与定义分离的维护负担
    - Python 模块级代码在 import 时执行，保证自动注册
    """
    def decorator(cls):
        _REGISTRY[prop_type_name] = cls
        return cls
    return decorator


def get_property_class(type_name: str) -> Optional[Type['PropertyData']]:
    """根据属性类型名查找对应的 PropertyData 子类。"""
    return _REGISTRY.get(type_name)


def all_registered() -> dict[str, Type['PropertyData']]:
    """返回所有已注册的属性类型映射（调试用）。"""
    return dict(_REGISTRY)
