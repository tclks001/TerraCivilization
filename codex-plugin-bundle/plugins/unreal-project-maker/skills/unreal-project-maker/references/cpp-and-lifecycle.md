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

也不要在 `OnConstruction` 中用 `NewObject` 临时制造本应持久跟随 Owner 的组件；默认子组件应在构造函数中通过 `CreateDefaultSubobject` 建立。

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

## Rebuild Non-serialized Runtime State in Game Worlds

`OnConstruction` 中得到的纯 C++ 状态不一定存在于 cooked GameWorld。以下数据默认不能假设会序列化进地图：

- `TUniquePtr` 持有的拓扑、生成器、查询结构和规则容器
- `CreateTransient` / `NewObject` / `UMaterialInstanceDynamic::Create` 得到的资源
- Editor construction 派生但没有直接 `UPROPERTY` 序列化的表现缓存

会进入实际游戏的 Actor 应在 `BeginPlay` 的 `World->IsGameWorld()` 路径检查关键运行态是否缺失或未初始化；缺失时调用统一 `Rebuild`/初始化入口。遇到“PIE 正常，打包后缺对象/材质/缓存”，先在 `BeginPlay` 输出这些状态，不要先归因于 cook。

## Treat Native Default Subobject Changes as Asset Migrations

Native Default Subobject 名称会被 Blueprint 资产序列化为模板兼容标识。删除、改名、改类型或移动 subobject 不是纯源码重构。

执行这类重构前：

1. 搜索派生 Blueprint、地图实例、Binder/软硬引用和 External Actor。
2. 设计迁移顺序和兼容窗口，不承诺旧 Blueprint 自动恢复。
3. 修复 C++ 后创建全新派生 Blueprint，重新应用仍受支持的设置。
4. 替换地图实例与引用，在 fresh Standalone 中加载验证。
5. 只在迁移完成后退役旧资产；不要为了 stale export 恢复已经废弃的组件。

Fresh process 比当前 Editor 会话更可靠：已加载的 CDO/模板可能暂时掩盖 `Could not find template object` 和序列化问题。

## Avoid PropertyEditor Recursion from Nested Components

- 不要把一个 `UActorComponent` 作为另一个可见 `UActorComponent` 的默认子对象暴露给 Details。
- Runtime-only helper component 优先惰性 `NewObject`，保存为 `Transient` 且不 inline 展开。
- Actor 拥有的原生组件可以 `VisibleAnywhere`，但复杂引用增加 `meta=(NoEditInline)`；用户通过 Blueprint Components 面板选择组件编辑。
- `OnConstruction` 对模板和 `EWorldType::EditorPreview` 做防御，避免 Blueprint 预览构造时填充大量 HISM/Gameplay 运行态。

新 Blueprint 也会触发的 PropertyEditor stack overflow 多半来自原生反射树；只有旧 Blueprint/地图触发时，再优先怀疑 stale subobject exports。

## Resolve Component Owners at Runtime

原生默认组件的构造函数也会在 Blueprint CDO 构造/重实例化时执行。不要在 Actor 构造函数中调用 `Component->Initialize(this)` 并长期缓存该指针；它可能是 `Default__BP_*`，其 `GetWorld()` 为 null。

组件需要 Host 时：

```cpp
AMyActor* UMyComponent::GetHost() const
{
    return Cast<AMyActor>(GetOwner());
}
```

Actor 构造函数只创建和挂接默认子对象。长期 owner 状态从 `GetOwner()` 动态解析。

## Componentize by Responsibility, Keep Compatibility Bridges Thin

大型 Actor 拆分时，让 Gameplay 编排、交互/高亮、相机、表现等组件各自拥有运行态和稳定入口。原 Actor 可暂留薄兼容桥，但不要同时保留两份真实状态。先迁移数据所有权，再迁移调用编排，最后迁移 Blueprint/地图资产；每一步单独编译和 fresh-process 验收。
