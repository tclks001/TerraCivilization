// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FSphereTopology.h"
#include "FSurfaceFrame.h"

/**
 * 
 */
class GRID_API FSphereTopologyQuery
{
public:
	explicit FSphereTopologyQuery(const FSphereTopology* InTopology);
	~FSphereTopologyQuery();

	const FSphereTopology* Topology = nullptr;

	FSurfaceQueryResult FindNearestCell(const FVector& WorldPos) const;
	FSurfaceFrame BuildCellFrame(int32 CellId, float Radius) const;
	void CollectCellRing(int32 CenterCellId, int32 RingCount, TArray<int32>& OutCellIds) const;
	void CollectCellDisk(int32 CenterCellId, int32 RingCount, TArray<int32>& OutCellIds) const;
};
