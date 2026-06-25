// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */

class GRID_API FCell
{
public:
	int32 CellId;
	FVector UnitCenter; // 单位球面法向，归一化
	bool bIsPentagon;   // 12个五边形，其他都是六边形

    TStaticArray<int32, 6> NeighborCellIds{INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE}; // 5或6个邻居Cells
    TStaticArray<int32, 6> EdgeIds{INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE};         // 5或6个边
    TStaticArray<int32, 6> CornerIds{INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE};       // 5或6个角，位于本CellCenter直接连接的Tri的中点上
	FCell();
	~FCell();

	int32 GetEdgeIdWithNeighborCellId(int32 NeighborCellId) const;
	int32 GetNeighborCellIdWithEdgeId(int32 EdgeId) const;
};
