# kismet/tokens.py
# EExprToken 枚举（UE 蓝图 VM 字节码指令集）和 ECastToken
#
# 设计说明：
# - 直接翻译自 C# UAssetAPI EExprToken.cs
# - 使用 IntEnum 便于序列化和比较

from __future__ import annotations
from enum import IntEnum


class EExprToken(IntEnum):
    EX_LocalVariable             = 0x00
    EX_InstanceVariable          = 0x01
    EX_DefaultVariable           = 0x02
    EX_Return                    = 0x04
    EX_Jump                      = 0x06
    EX_JumpIfNot                 = 0x07
    EX_Assert                    = 0x09
    EX_Nothing                   = 0x0B
    EX_NothingInt32              = 0x0C
    EX_Let                       = 0x0F
    EX_BitFieldConst             = 0x11
    EX_ClassContext              = 0x12
    EX_MetaCast                  = 0x13
    EX_LetBool                   = 0x14
    EX_EndParmValue              = 0x15
    EX_EndFunctionParms          = 0x16
    EX_Self                      = 0x17
    EX_Skip                      = 0x18
    EX_Context                   = 0x19
    EX_Context_FailSilent        = 0x1A
    EX_VirtualFunction           = 0x1B
    EX_FinalFunction             = 0x1C
    EX_IntConst                  = 0x1D
    EX_FloatConst                = 0x1E
    EX_StringConst               = 0x1F
    EX_ObjectConst               = 0x20
    EX_NameConst                 = 0x21
    EX_RotationConst             = 0x22
    EX_VectorConst               = 0x23
    EX_ByteConst                 = 0x24
    EX_IntZero                   = 0x25
    EX_IntOne                    = 0x26
    EX_True                      = 0x27
    EX_False                     = 0x28
    EX_TextConst                 = 0x29
    EX_NoObject                  = 0x2A
    EX_TransformConst            = 0x2B
    EX_IntConstByte              = 0x2C
    EX_NoInterface               = 0x2D
    EX_DynamicCast               = 0x2E
    EX_StructConst               = 0x2F
    EX_EndStructConst            = 0x30
    EX_SetArray                  = 0x31
    EX_EndArray                  = 0x32
    EX_PropertyConst             = 0x33
    EX_UnicodeStringConst        = 0x34
    EX_Int64Const                = 0x35
    EX_UInt64Const               = 0x36
    EX_DoubleConst               = 0x37
    EX_PrimitiveCast             = 0x38
    EX_SetSet                    = 0x39
    EX_EndSet                    = 0x3A
    EX_SetMap                    = 0x3B
    EX_EndMap                    = 0x3C
    EX_SetConst                  = 0x3D
    EX_EndSetConst               = 0x3E
    EX_MapConst                  = 0x3F
    EX_EndMapConst               = 0x40
    EX_Vector3fConst             = 0x41
    EX_StructMemberContext       = 0x42
    EX_LetMulticastDelegate      = 0x43
    EX_LetDelegate               = 0x44
    EX_LocalVirtualFunction      = 0x45
    EX_LocalFinalFunction        = 0x46
    EX_LocalOutVariable          = 0x48
    EX_DeprecatedOp4A            = 0x4A
    EX_InstanceDelegate          = 0x4B
    EX_PushExecutionFlow         = 0x4C
    EX_PopExecutionFlow          = 0x4D
    EX_ComputedJump              = 0x4E
    EX_PopExecutionFlowIfNot     = 0x4F
    EX_Breakpoint                = 0x50
    EX_InterfaceContext          = 0x51
    EX_ObjToInterfaceCast        = 0x52
    EX_EndOfScript               = 0x53
    EX_CrossInterfaceCast        = 0x54
    EX_InterfaceToObjCast        = 0x55
    EX_WireTracepoint            = 0x5A
    EX_SkipOffsetConst           = 0x5B
    EX_AddMulticastDelegate      = 0x5C
    EX_ClearMulticastDelegate    = 0x5D
    EX_Tracepoint                = 0x5E
    EX_LetObj                    = 0x5F
    EX_LetWeakObjPtr             = 0x60
    EX_BindDelegate              = 0x61
    EX_RemoveMulticastDelegate   = 0x62
    EX_CallMulticastDelegate     = 0x63
    EX_LetValueOnPersistentFrame = 0x64
    EX_ArrayConst                = 0x65
    EX_EndArrayConst             = 0x66
    EX_SoftObjectConst           = 0x67
    EX_CallMath                  = 0x68
    EX_SwitchValue               = 0x69
    EX_InstrumentationEvent      = 0x6A
    EX_ArrayGetByRef             = 0x6B
    EX_ClassSparseDataVariable   = 0x6C
    EX_FieldPathConst            = 0x6D
    EX_AutoRtfmTransact          = 0x70
    EX_AutoRtfmStopTransact      = 0x71
    EX_AutoRtfmAbortIfNot        = 0x72


class ECastToken(IntEnum):
    """EExprToken 0x38 (EX_PrimitiveCast) 内嵌的转换类型"""
    CST_ObjectToInterface        = 0x46
    CST_ObjectToBool             = 0x47
    CST_InterfaceToBool          = 0x49
    CST_IntToFloat               = 0x9A
    CST_IntToDouble              = 0x9B
    CST_ByteToInt                = 0x9C
    CST_ByteToFloat              = 0x9D
    CST_ByteToDouble             = 0x9E
    CST_IntToByte                = 0x9F
    CST_IntSubtitlePriority      = 0xA0
    CST_FloatToInt               = 0xA1
    CST_FloatToByte              = 0xA2
    CST_StringToVector           = 0xA3
    CST_StringToRotator          = 0xA4
    CST_VectorToRotator          = 0xA5
    CST_RotatorToVector          = 0xA6
    CST_StringToFloat            = 0xA7
    CST_StringToInt              = 0xA8
    CST_VectorToBool             = 0xA9
    CST_RotatorToBool            = 0xAA
    CST_BoolToString             = 0xAB
    CST_IntToString              = 0xAC
    CST_FloatToString            = 0xAD
    CST_ObjectToString           = 0xAE
    CST_NameToString             = 0xAF
    CST_VectorToString           = 0xB0
    CST_RotatorToString          = 0xB1
    CST_StringToBool             = 0xB2
    CST_FloatToDouble            = 0xB3
    CST_DoubleToFloat            = 0xB4
    CST_Max                      = 0xFF
