# C++ and Lifecycle

## Scope

本文件记录 UE5 项目里最容易反复踩到的 C++、模块边界、UPROPERTY、UObject 生命周期和 Editor/PIE 生命周期规则。

## Real Source First

写任何 `obj->field` / `obj.field` 之前，先打开真实 `.h`：

- 核对字段名
- 核对类型
- 核对是否 `TArray`、`TStaticArray`、`TMap`
- 核对默认值、空位约定、`INDEX_NONE`

写完后再搜索一次刚写的字段名，确认没有拼错或混用近义字段。

不要：

- 凭语义猜字段名
- 只看设计稿不看头文件
- 从别的项目拷近义命名
- 只因为编译通过就默认语义正确

## Module Boundaries

- 上游模块输出 POD 数据或稳定查询接口。
- 下游渲染 / 玩法 / UI 模块只读消费，不把依赖反灌回上游。
- 新模块设计时先写：模块依赖图、文件落点、关键结构、Build.cs 依赖、编辑器集成点、验收口径。

优先数据驱动：

- GameplayTag
- DataAsset
- 配置表
- 查表结构

C++ 负责机制内核、查询、验证和装配。

## UPROPERTY Rules

公开给编辑器的字段要像真正的 API 一样设计：

- 清晰 Category
- `ClampMin` / `ClampMax` / `UIMin` / `UIMax`
- 简短但有信息量的注释
- 默认值与典型值
- 参数名、日志名、文档名一致

新增影响材质、LUT、Mesh 重建、生成逻辑的字段时，同步更新：

- `Rebuild()` / 注入路径
- 日志摘要
- 验收文档
- 排错表

## `TUniquePtr` + Forward Declaration + UObject

如果 `UObject` / `AActor` 持有 `TUniquePtr<ForwardDeclaredType>`，按以下模板写：

```cpp
// .h
class FMyImpl;

UCLASS()
class AMyActor : public AActor
{
    GENERATED_BODY()
public:
    AMyActor();
    AMyActor(FVTableHelper& Helper);
    virtual ~AMyActor();

private:
    TUniquePtr<FMyImpl> Impl;
};

// .cpp
#include "MyImpl.h"

AMyActor::AMyActor() = default;
AMyActor::AMyActor(FVTableHelper& Helper) : Super(Helper) {}
AMyActor::~AMyActor() = default;
```

规则：

- `.h` 显式声明默认构造、析构、`FVTableHelper` 构造。
- `.cpp` 定义它们，并 include 完整类型。
- 不要把 `= default` 写在 `.h`。
- 不要为了省事把重型头直接 include 进 `.h`。

## Do Not Reset `TUniquePtr` in `BeginDestroy`

`BeginDestroy` 是 GC 早期回调，不等于 C++ 析构函数。

- `TUniquePtr`
- `TMap`
- 纯 C++ builder
- 纯数据对象

默认交给析构函数释放。只有必须更早释放的 RHI / GPU / 委托资源才在 `BeginDestroy` 处理。

如果 `BeginDestroy` 里只是手动 `Reset()` 一堆 `TUniquePtr`，通常就是错误信号。

## Prefer Components over `SpawnActor` in `OnConstruction`

凡是“跟随 Owner 一起构建、销毁、显示、隐藏”的视觉子对象，优先做 `CreateDefaultSubobject`：

- `UProceduralMeshComponent`
- `UStaticMeshComponent`
- `UHierarchicalInstancedStaticMeshComponent`
- `ULightComponent`
- 其他纯视觉挂件

不要把它们做成 `OnConstruction` 里 `SpawnActor` 出来的子 Actor，除非它真的是独立游戏实体。

好处：

- Editor / PIE 世界复制更稳
- UPROPERTY 引用更稳
- 生命周期更可控
- 不容易出现“Editor 正常、PIE 丢材质/丢引用”

## Recover Editor Resources after PIE

如果在 `OnConstruction` / `Rebuild()` 里创建：

- `UMaterialInstanceDynamic`
- `CreateTransient` 纹理
- 动态 LUT
- 只在编辑器构建路径填充的视觉资源

考虑监听 `FWorldDelegates::OnPostWorldCleanup`，在 PIE 退出后为 Editor 世界重建一次。

至少做这些防御：

- `this` 仍然有效
- 当前世界不是正在 cleanup 的世界
- 确实是会话结束
- 当前对象属于 Editor / EditorPreview 世界
