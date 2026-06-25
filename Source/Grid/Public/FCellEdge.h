// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class GRID_API FCellEdge
{
public:
	int32 EdgeId;
    TStaticArray<int32, 2> CellIds{ INDEX_NONE, INDEX_NONE };   // 相邻的Cell，从西往东
    TStaticArray<int32, 2> CornerIds{ INDEX_NONE, INDEX_NONE }; // 两端的Corner， 从西往东

	bool bIsPlateBoundary = false;
    float BoundaryStrength = 0.0f;
	FCellEdge(int32 A, int32 B);
	~FCellEdge();
};
