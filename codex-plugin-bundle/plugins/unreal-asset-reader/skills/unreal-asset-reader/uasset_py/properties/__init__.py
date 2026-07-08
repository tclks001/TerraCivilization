# properties/__init__.py
from .registry import register, get_property_class
from .base import PropertyData, read_property, read_property_tag_only

__all__ = [
    'PropertyData',
    'read_property',
    'read_property_tag_only',
    'register',
    'get_property_class',
]
