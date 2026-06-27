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
	FVector UnitDir;                          // 本Corner的位置 = 负责的三个Cells所张成primal三角形的"球面外心"（朝外单位向量）；详见 Docs/SphereTopologyReference.md §3.4 / §4.1
    TStaticArray<int32, 3> CellIds{ INDEX_NONE, INDEX_NONE, INDEX_NONE };           // 本Corner负责的三个Cells（同时也是primal三角形的三个顶点）
    TStaticArray<int32, 3> EdgeIds{ INDEX_NONE, INDEX_NONE, INDEX_NONE };           // 连接本Corner的Edges
    TStaticArray<int32, 3> NeighborCornerIds{ INDEX_NONE, INDEX_NONE, INDEX_NONE }; // 通过Edges相连的Corners
	FCorner();
	~FCorner();

	int32 GetEdgeIdWithNeighborCornerId(int32 NeighborCornerId) const;
	int32 GetNeighborCornerIdWithEdgeId(int32 EdgeId) const;
};
