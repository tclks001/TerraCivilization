# kismet/expression.py
# KismetExpression 基类 + read_expression() 分派入口 + read_expression_array()
#
# 设计说明：
# - KismetExpression 是统一的数据容器（dataclass），所有字段存入 fields dict
# - read_expression() 读 1 字节 token，查 _HANDLERS 分派表，调用对应 handler
# - 不用子类继承，避免 ~75 个类的模板膨胀；fields dict 展开为 JSON 输出
# - to_dict() 输出兼容 UAssetAPI JSON 格式（$type / Token / Inst 字段）

from __future__ import annotations
from dataclasses import dataclass, field
from typing import TYPE_CHECKING, Any, Optional

from .tokens import EExprToken

if TYPE_CHECKING:
    from ..core.reader import BinaryReader
    from ..core.uasset import UAsset


@dataclass
class KismetExpression:
    """
    统一的字节码表达式容器。

    - token: EExprToken 原始 int 值
    - inst:  token 名去掉 "EX_" 前缀（如 "StringConst"）
    - fields: 各 token 的 payload 字段（dict），to_dict() 时展开到顶层
    """
    token: int
    inst: str
    fields: dict = field(default_factory=dict)

    def to_dict(self) -> dict:
        """
        输出兼容 UAssetAPI 格式的 dict。

        格式：
        {
          "$type": "UAssetAPI.Kismet.Bytecode.Expressions.EX_<Inst>, UAssetAPI",
          "Token": "EX_<Inst>",
          "Inst": "<Inst>",
          ... fields ...
        }
        """
        d: dict = {}
        d['$type'] = (
            f'UAssetAPI.Kismet.Bytecode.Expressions.EX_{self.inst}, UAssetAPI'
        )
        # fields 展开到顶层（先写 fields，再覆盖固定字段保证顺序）
        d.update(_serialize_fields(self.fields))
        d['Token'] = f'EX_{self.inst}'
        d['Inst'] = self.inst
        return d


def _serialize_fields(fields: dict) -> dict:
    """递归序列化 fields，KismetExpression 转 dict，list 递归处理。"""
    result = {}
    for k, v in fields.items():
        result[k] = _serialize_value(v)
    return result


def _serialize_value(v: Any) -> Any:
    if isinstance(v, KismetExpression):
        return v.to_dict()
    if isinstance(v, list):
        return [_serialize_value(item) for item in v]
    if isinstance(v, tuple):
        return [_serialize_value(item) for item in v]
    return v


def read_expression(reader: 'BinaryReader', asset: 'UAsset') -> KismetExpression:
    """
    读取一条 Kismet 表达式。

    流程：
    1. read_uint8() → token 字节
    2. 查分派表 _HANDLERS
    3. 调用 handler(reader, asset) → fields dict
    4. 构造 KismetExpression 并返回
    """
    # 延迟导入避免循环
    from .expressions import _HANDLERS

    token_byte = reader.read_uint8()
    try:
        token_enum = EExprToken(token_byte)
        inst = token_enum.name[3:]  # "EX_StringConst" → "StringConst"
    except ValueError:
        raise ValueError(f"Unknown EExprToken: 0x{token_byte:02X} at pos {reader.position - 1}")

    handler = _HANDLERS.get(token_byte)
    if handler is None:
        raise NotImplementedError(
            f"Unimplemented EExprToken: EX_{inst} (0x{token_byte:02X})"
        )

    fields = handler(reader, asset)
    return KismetExpression(token=token_byte, inst=inst, fields=fields)


def read_expression_array(
    reader: 'BinaryReader',
    asset: 'UAsset',
    end_token: int,
) -> list:
    """
    读取表达式列表，直到遇到 end_token 为止。
    end_token 本身不包含在返回列表中。
    """
    exprs = []
    while True:
        expr = read_expression(reader, asset)
        if expr.token == end_token:
            break
        exprs.append(expr)
    return exprs
