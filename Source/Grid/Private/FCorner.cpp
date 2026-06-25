// Fill out your copyright notice in the Description page of Project Settings.


#include "FCorner.h"

FCorner::FCorner()
{
}

FCorner::~FCorner()
{
}
int32 FCorner::GetEdgeIdWithNeighborCornerId(int32 _NeighborCornerId) const
{
	for (int32 I = 0; I < NeighborCornerIds.Num(); ++I) 
	{
		if (NeighborCornerIds[I] == _NeighborCornerId)
		{
			return EdgeIds[I];
		}
	}
	return INDEX_NONE;
}

int32 FCorner::GetNeighborCornerIdWithEdgeId(int32 _EdgeId) const
{
	for (int32 I = 0; I < EdgeIds.Num(); ++I)
	{
		if (EdgeIds[I] == _EdgeId)
		{
			return NeighborCornerIds[I];
		}
	}
	return INDEX_NONE;
}