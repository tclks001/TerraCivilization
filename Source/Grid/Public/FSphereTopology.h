// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FCell.h"
#include "FCellEdge.h"
#include "FRenderTri.h"
#include "FCorner.h"
#include "FSurfaceFrame.h"
#include "FSurfaceQueryResult.h"

class FSphereTopology; // 前向声明

struct FTriTreeNode
{
	TStaticArray<int32, 3> CellIds{ INDEX_NONE, INDEX_NONE, INDEX_NONE }; // Tri三个角的Cell索引
    TStaticArray<FTriTreeNode*, 4> Children{ nullptr, nullptr, nullptr, nullptr };   // 子节点
	FTriTreeNode* Father = nullptr; // 上级节点（根节点为nullptr）
	FVector Center; // 三角形中心位置，单位球坐标，用于搜索
	int32 CornerId = INDEX_NONE; // 仅叶子节点有效，对应FSphereTopology::Corners的索引；非叶子节点为INDEX_NONE
};

/**
 * 
 */
class GRID_API FSphereTopology
{
public:
	int32 SubdivisionLevel = 4; // 分割层数

	// 构建完成后使用的字段
	TArray<FCell> Cells;     // 逻辑层：五边形或六边形
	TArray<FCellEdge> Edges; // 逻辑边
	TArray<FRenderTri> Tris; // 渲染三角形
	TArray<FCorner> Corners; // 渲染角点
	TArray<FTriTreeNode*> TriTreeRoots; // 三角形树的根节点列表，每个根Tri对应一个树根，共有20个树根（对应20个根Tri）
	
	FSphereTopology();
	FSphereTopology(int32 _SubdivisionLevel);
	~FSphereTopology();

	TArray<FVector> PrimalVertsUnit;  // 顶点位置
	TArray<FVector2D> PrimalUVsUnit;  // UV坐标
	TArray<int32> UVToPrimalVert;     // UV索引到顶点索引的映射
	TArray<FIntVector> PrimalTris;    // 顶点索引的三角形列表
	TArray<FIntVector> PrimalUVTris;  // UV索引的三角形列表，同样Id和PrimalTris对应同一个三角形
	TArray<FTriTreeNode*> PrimalTriTreeNodes; // Primal三角形树的节点列表
	TMap<uint64, int32> MidpointCache;
	TMap<uint64, int32> UVCache;
	void BuildIcosahedronUnit();
	void SubdividePrimalOnce();
	void BuildDualFromPrimal();
	int AddVertex(FVector Vertex);
	int32 AddUV(FVector2D UV);
	void Build();
};
