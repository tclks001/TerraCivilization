// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

// =====================================================================
//  R8RecipeTable.h
//
//  R8 阶段拍死的"17 种地形配方常量表" + Knuth 哈希索引 + 中性默认配方。
//
//  历史：
//    - 2026-06：原本 file-static 在 PlanetTopologyDebugMesh.cpp（L46~L121）。
//    - T3 子里程碑（详见 Docs/T3_TerrainMeshRender.md §4.3 / D14 例外条款）抽出到本头文件，
//      让 APlanetTessellatedMesh 与（未来回头收编时的）APlanetTopologyDebugMesh 共享。
//
//  约束：
//    - 这是"产品级常量数据"，不持有任何状态，纯 POD + constexpr namespace 函数。
//    - 不依赖 UObject / Engine 模块（仅 CoreMinimal）。
//    - W4 联调时整体迁移到 UTerrainDefinition::FTerrainMaterialParams DataAsset 后，
//      本头文件可整体作废 —— 届时再删。
//
//  NS 设计：
//    全部塞进 TerraCivilization::R8 命名空间，避免与 R8 actor cpp 内的旧 file-static
//    名字撞车（在 R8 actor 完成迁移之前两份共存，编译期 namespace 隔离）。
// =====================================================================
namespace TerraCivilization
{
namespace R8
{

/**
 * 单条地形配方。字段顺序：BaseTexIdx, Tint(R,G,B), Sat, Bri, RoughMin, RoughMax,
 *                     OverlayBlend, NormalStr, TriScale。
 *
 * 详见 Docs/R8_ParametricTint.md §1.4 / §3.2.1。
 */
struct FRecipe
{
    uint8  BaseTexIdx;       // 0=Soil, 1=Rock, 2=Forest（不应作为 base）
    float  TintR, TintG, TintB;
    float  SatMul;
    float  BriMul;
    float  RoughMin;
    float  RoughMax;
    float  OverlayBlend;     // 0=纯 base，>0 叠加 Forest
    float  NormalStr;
    float  TriScale;
};

/** 17 种地形配方常量表。索引顺序与 Docs/R8_ParametricTint.md §3.2.1 严格一致。 */
inline constexpr FRecipe GRecipes[17] = {
    // 0  Plain.Grass
    { 0, 0.40f, 0.70f, 0.30f, 1.0f, 1.0f,  0.5f, 0.8f,  0.15f, 1.0f, 1.0f },
    // 1  Plain.Savanna
    { 0, 0.70f, 0.60f, 0.30f, 0.9f, 1.1f,  0.6f, 0.9f,  0.0f,  1.0f, 1.0f },
    // 2  Forest.Temperate
    { 0, 0.30f, 0.55f, 0.25f, 1.1f, 0.9f,  0.5f, 0.8f,  0.65f, 1.2f, 1.0f },
    // 3  Forest.Tropical
    { 0, 0.20f, 0.50f, 0.20f, 1.3f, 0.85f, 0.4f, 0.7f,  0.85f, 1.5f, 1.0f },
    // 4  Forest.Taiga
    { 0, 0.25f, 0.40f, 0.30f, 0.7f, 0.85f, 0.5f, 0.8f,  0.55f, 1.2f, 1.0f },
    // 5  Wetland
    { 0, 0.30f, 0.50f, 0.35f, 1.0f, 0.85f, 0.2f, 0.5f,  0.0f,  0.8f, 1.0f },
    // 6  Desert.Sand
    { 0, 0.95f, 0.85f, 0.60f, 0.9f, 1.2f,  0.7f, 0.95f, 0.0f,  0.5f, 0.5f },
    // 7  Desert.Rocky
    { 1, 0.70f, 0.60f, 0.45f, 0.7f, 1.0f,  0.7f, 0.95f, 0.0f,  1.0f, 1.0f },
    // 8  Coast.Beach
    { 0, 0.95f, 0.90f, 0.70f, 0.7f, 1.15f, 0.7f, 0.95f, 0.0f,  0.5f, 0.5f },
    // 9  Coast.Rocky
    { 1, 0.55f, 0.55f, 0.50f, 0.5f, 0.9f,  0.6f, 0.9f,  0.0f,  1.2f, 1.5f },
    // 10 Mountain.Hill
    { 1, 0.55f, 0.50f, 0.42f, 0.7f, 0.9f,  0.6f, 0.9f,  0.30f, 1.2f, 1.5f },
    // 11 Mountain.Peak
    { 1, 0.50f, 0.48f, 0.45f, 0.4f, 0.85f, 0.7f, 0.95f, 0.0f,  1.5f, 2.0f },
    // 12 Mountain.Snow
    { 1, 0.92f, 0.94f, 0.98f, 0.2f, 1.4f,  0.1f, 0.4f,  0.0f,  1.0f, 1.5f },
    // 13 Tundra
    { 0, 0.70f, 0.70f, 0.65f, 0.3f, 1.05f, 0.7f, 0.95f, 0.0f,  0.8f, 1.0f },
    // 14 Glacier
    { 1, 0.85f, 0.92f, 0.98f, 0.4f, 1.45f, 0.1f, 0.3f,  0.0f,  0.5f, 2.0f },
    // 15 Ocean.Shallow
    { 0, 0.20f, 0.50f, 0.70f, 1.5f, 0.8f,  0.05f, 0.2f, 0.0f,  0.3f, 1.5f },
    // 16 Ocean.Deep
    { 0, 0.05f, 0.15f, 0.40f, 1.5f, 0.5f,  0.05f, 0.2f, 0.0f,  0.2f, 2.0f },
};
static_assert(UE_ARRAY_COUNT(GRecipes) == 17, "R8 must have exactly 17 recipes");

/**
 * R8 placeholder：CellId → 配方索引（0..16）。Knuth 整数哈希常数（黄金分割比 × 2^32）保证
 * 相邻 CellId 也能落到不同配方。
 */
FORCEINLINE constexpr int32 PlaceholderRecipeIndex(int32 CellId)
{
    constexpr uint32 KnuthHashConst = 2654435761u;
    return static_cast<int32>((static_cast<uint32>(CellId) * KnuthHashConst) % 17u);
}

/**
 * 中性默认配方：W4 联调期 bUseR8PlaceholderRecipes=false 时填入 Cell*LUT，
 * 让 R8 4 通道 LUT 不参与微调，HLSL 端等价于纯 R7 视觉。
 */
inline constexpr FRecipe GNeutralRecipe = {
    /*BaseTexIdx*/  0,
    /*Tint*/       1.0f, 1.0f, 1.0f,
    /*Sat*/        1.0f,
    /*Bri*/        1.0f,
    /*RoughMin*/   0.5f,
    /*RoughMax*/   0.8f,
    /*OvlBlend*/   0.0f,
    /*NormalStr*/  1.0f,
    /*TriScale*/   1.0f,
};

/** 根据 bUsePlaceholder 与 CellId 解析出该 cell 应使用的配方。 */
FORCEINLINE constexpr const FRecipe& PickRecipe(bool bUsePlaceholder, int32 CellId)
{
    return bUsePlaceholder ? GRecipes[PlaceholderRecipeIndex(CellId)] : GNeutralRecipe;
}

}   // namespace R8
}   // namespace TerraCivilization
