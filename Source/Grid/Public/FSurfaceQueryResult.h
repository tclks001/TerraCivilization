// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class GRID_API FSurfaceQueryResult
{
public:
	int32 CellId = INDEX_NONE;                  // 该点所在的CellId
	FVector ClosestPoint = FVector::ZeroVector; // 该点在Surface上的最近点
	float Distance = 0.0f;                      // 该点到Surface的距离
	FSurfaceQueryResult();
	~FSurfaceQueryResult();
};
