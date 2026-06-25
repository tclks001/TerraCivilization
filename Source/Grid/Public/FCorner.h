// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class GRID_API FCorner
{
public:
	int32 CornerId;
	FVector UnitDir;                          // 本Corner的位置，即负责的三个Cells的Center的重心的归一化
    TStaticArray<int32, 3> CellIds{ INDEX_NONE, INDEX_NONE, INDEX_NONE };           // 本Corner负责的三个Cells
    TStaticArray<int32, 3> EdgeIds{ INDEX_NONE, INDEX_NONE, INDEX_NONE };           // 连接本Corner的Edges
    TStaticArray<int32, 3> NeighborCornerIds{ INDEX_NONE, INDEX_NONE, INDEX_NONE }; // 通过Edges相连的Corners
	FCorner();
	~FCorner();

	int32 GetEdgeIdWithNeighborCornerId(int32 NeighborCornerId) const;
	int32 GetNeighborCornerIdWithEdgeId(int32 EdgeId) const;
};
