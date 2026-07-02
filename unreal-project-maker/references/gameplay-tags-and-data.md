# Gameplay Tags and Data

## Scope

本文件记录 GameplayTag、Tag Source、配置文件和数据驱动路径的通用做法。

## Prefer Data-Driven Systems

把可配置玩法与分类数据放到：

- `GameplayTag`
- `DataAsset`
- 配置文件
- 查表结构

不要把大量规则写死在 `if/else` 里。

## `Config/Tags/*.ini` Is Not “Drop In and Done”

对新增 GameplayTag Source，默认走更保守的流程：

1. 在编辑器里创建或注册新的 Tag Source
2. 让引擎把它认作合法 Source
3. 再填充完整 ini 内容
4. 重启或刷新后检查下拉框

如果某个 Tag 是启动期硬依赖、必须非常稳定，可考虑放在更标准的默认 GameplayTags 配置入口，而不是完全押在新 Source 注册流程上。

## Check `meta=(Categories="...")`

如果某个 `FGameplayTag` 字段下拉框为空，不要只怀疑 ini 没加载，也检查：

- `Categories` 前缀是否正确
- Tag 命名空间是否匹配
- 是否重复定义了同一个 Tag

## Validation Checklist

- Tag Source 已被编辑器识别
- ini 内容格式正确
- 下拉框能看到期望命名空间
- 没有重复 Tag 定义
- 代码侧引用与配置侧命名一致
