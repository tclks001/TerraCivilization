// Fill out your copyright notice in the Description page of Project Settings.


#include "FCell.h"

FCell::FCell()
{
}

FCell::~FCell()
{
}

int32 FCell::GetEdgeIdWithNeighborCellId(int32 NeighborCellId) const
{
	for (int32 I = 0; I < NeighborCellIds.Num(); ++I)
	{
		if (NeighborCellIds[I] == NeighborCellId)
		{
			return EdgeIds[I];
		}
	}
	return INDEX_NONE;
}

int32 FCell::GetNeighborCellIdWithEdgeId(int32 EdgeId) const
{
	for (int32 I = 0; I < EdgeIds.Num(); ++I)
	{
		if (EdgeIds[I] == EdgeId)
		{
			return NeighborCellIds[I];
		}
	}
	return INDEX_NONE;
}
