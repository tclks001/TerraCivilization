// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */

class GRID_API FRenderTri
{
public:
	int32 TriId;
	TStaticArray<int32, 3> VertexIds{ INDEX_NONE, INDEX_NONE, INDEX_NONE };
	TStaticArray<int32, 3> UVIds{ INDEX_NONE, INDEX_NONE, INDEX_NONE };
	//TStaticArray<FVector2D, 3> UVs{ FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector };
	//FVector FaceNormal;               // 单位法向量
	FRenderTri();
	~FRenderTri();
};