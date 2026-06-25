// Fill out your copyright notice in the Description page of Project Settings.


#include "FSphereTopology.h"

FSphereTopology::FSphereTopology()
{
	SubdivisionLevel = 4;
	Build();
}

FSphereTopology::FSphereTopology(int32 _SubdivisionLevel)
{
	SubdivisionLevel = FMath::Clamp(_SubdivisionLevel, 1, 6);
	Build();
}

FSphereTopology::~FSphereTopology()
{
	// 递归释放整棵三角形树，避免内存泄漏
	// 由于细分时所有新建的子节点都挂在父节点的Children[0..3]下，
	// 因此从TriTreeRoots出发递归即可释放所有节点。
	TFunction<void(FTriTreeNode*)> DeleteTree = [&](FTriTreeNode* Node)
		{
			if (Node == nullptr) return;
			for (int32 I = 0; I < 4; ++I)
			{
				DeleteTree(Node->Children[I]);
				Node->Children[I] = nullptr;
			}
			delete Node;
		};
	for (FTriTreeNode* Root : TriTreeRoots)
	{
		DeleteTree(Root);
	}
	TriTreeRoots.Empty();
	PrimalTriTreeNodes.Empty();
}

void FSphereTopology::Build()
{
	BuildIcosahedronUnit();
	for (int32 I = 0; I < SubdivisionLevel; I++)
	{
		SubdividePrimalOnce();
	}
	BuildDualFromPrimal();

	// 构建完成后，递归填充每个TriTreeNode的Center（三个顶点Cell的UnitCenter之和的单位向量）
	TFunction<void(FTriTreeNode*)> BuildCenter = [&](FTriTreeNode* Node)
		{
			if (Node == nullptr) return;
			FVector Sum = FVector::ZeroVector;
			for (int32 I = 0; I < 3; ++I)
			{
				Sum += Cells[Node->CellIds[I]].UnitCenter;
			}
			Node->Center = Sum.GetSafeNormal();
			for (int32 I = 0; I < 4; ++I)
			{
				BuildCenter(Node->Children[I]);
			}
		};
	for (FTriTreeNode* Root : TriTreeRoots)
	{
		BuildCenter(Root);
	}
}

int32 FSphereTopology::AddVertex(FVector Vertex)
{
    PrimalVertsUnit.Add(Vertex.GetSafeNormal());
	return PrimalVertsUnit.Num() - 1;
}

int32 FSphereTopology::AddUV(FVector2D UV)
{
	PrimalUVsUnit.Add(UV);
	return PrimalUVsUnit.Num() - 1;
}

void FSphereTopology::BuildIcosahedronUnit()
{
	// 构建正20面体
	const float Phi = (1.0f + FMath::Sqrt(5.0f)) / 2.0f;

	const float Length = FMath::Sqrt(1 + Phi * Phi);

	auto AddIcoVertex = [&](float X, float Y, float Z)
		{
			PrimalVertsUnit.Add(FVector(X, Y, Z).GetSafeNormal());
		};
	auto AddTriangle = [&](int32 A, int32 B, int32 C)
		{
			PrimalTris.Add(FIntVector(A, B, C));
		};
	auto AddMainUV = [&](FVector2D UV0, FVector2D UV1, FVector2D UV2)
		{
			FVector2D Center = (UV0 + UV1 + UV2) / 3.0f;
			UV0 = (UV0 - Center) * 0.95f + Center; // Padding
			UV1 = (UV1 - Center) * 0.95f + Center;
			UV2 = (UV2 - Center) * 0.95f + Center;
			int32 UV0Id = AddUV(UV0);
			int32 UV1Id = AddUV(UV1);
			int32 UV2Id = AddUV(UV2);
			PrimalUVTris.Add(FIntVector(UV0Id, UV1Id, UV2Id));
		};
	float A = Phi / Length;
	float B = 1.0f / Length;

	AddIcoVertex(-B, A, 0);  // 0
	AddIcoVertex(B, A, 0);   // 1
	AddIcoVertex(-B, -A, 0); // 2
	AddIcoVertex(B, -A, 0);  // 3

	AddIcoVertex(0, -B, A);  // 4
	AddIcoVertex(0, B, A);   // 5
	AddIcoVertex(0, -B, -A); // 6
	AddIcoVertex(0, B, -A);  // 7

	AddIcoVertex(A, 0, -B);  // 8
	AddIcoVertex(A, 0, B);   // 9
	AddIcoVertex(-A, 0, -B); // 10
	AddIcoVertex(-A, 0, B);  // 11

	// 20个面（三角形）
	// 顶部5个
	AddTriangle(0, 5, 11);
	AddTriangle(0, 1, 5);
	AddTriangle(0, 7, 1);
	AddTriangle(0, 10, 7);
	AddTriangle(0, 11, 10);
	// 中部10个
	AddTriangle(1, 9, 5);
	AddTriangle(5, 4, 11);
	AddTriangle(11, 2, 10);
	AddTriangle(10, 6, 7);
	AddTriangle(7, 8, 1);
	AddTriangle(4, 5, 9);
	AddTriangle(2, 11, 4);
	AddTriangle(6, 10, 2);
	AddTriangle(8, 7, 6);
	AddTriangle(9, 1, 8);
	// 底部5个
	AddTriangle(3, 4, 9);
	AddTriangle(3, 2, 4);
	AddTriangle(3, 6, 2);
	AddTriangle(3, 8, 6);
	AddTriangle(3, 9, 8);

	// 为每个初始正20面体三角形创建一个TriTreeNode，作为根节点；同时与PrimalTris保持索引对应
	TriTreeRoots.Reset();
	PrimalTriTreeNodes.Reset();
	TriTreeRoots.Reserve(PrimalTris.Num());
	PrimalTriTreeNodes.Reserve(PrimalTris.Num());
	for (int32 I = 0; I < PrimalTris.Num(); ++I)
	{
		FTriTreeNode* Node = new FTriTreeNode();
		Node->CellIds[0] = PrimalTris[I].X;
		Node->CellIds[1] = PrimalTris[I].Y;
		Node->CellIds[2] = PrimalTris[I].Z;
		TriTreeRoots.Add(Node);
		PrimalTriTreeNodes.Add(Node);
	}

	// UV，没确定顺序，不是很有必要
	for (int32 I = 0; I < 5; ++I)
	{
		// 分成五组，每组四个，按<<<<<型排列，边缘留padding
		float Width = 1.0f / 5.5f;
		float Height = Width * 0.5f / FMath::Sqrt(3.0f);
		FVector2D UV0((I + 0.5f) * Width, 0.5f - Height);
		FVector2D UV1((I + 1.5f) * Width, 0.5f - Height);
		FVector2D UV2(I * Width, 0.5f);
		FVector2D UV3((I + 1) * Width, 0.5f);
		FVector2D UV4((I + 0.5f) * Width, 0.5f + Height);
		FVector2D UV5((I + 1.5f) * Width, 0.5f + Height);
		AddMainUV(UV0, UV1, UV3);
		AddMainUV(UV0, UV3, UV2);
		AddMainUV(UV2, UV3, UV4);
		AddMainUV(UV3, UV5, UV4);
	}
}

void FSphereTopology::SubdividePrimalOnce()
{
	auto GetMidPoint = [&](int32 P1, int32 P2) -> int32
		{
			uint64 Key = ((uint64)FMath::Min(P1, P2) << 32) | FMath::Max(P1, P2);
			if (int32* Found = MidpointCache.Find(Key)) {
				return *Found;
			}
			FVector NewPos = (PrimalVertsUnit[P1] + PrimalVertsUnit[P2]) * 0.5f;
			NewPos = NewPos.GetSafeNormal();
			int32 Index = AddVertex(NewPos);
			MidpointCache.Add(Key, Index);
			return Index;
		};
	auto GetMidUV = [&](int32 T1, int32 T2) -> int32
		{
			uint64 Key = ((uint64)FMath::Min(T1, T2) << 32) | FMath::Max(T1, T2);
			if (int32* Found = UVCache.Find(Key)) {
				return *Found;
			}
			FVector2D NewUV = (PrimalUVsUnit[T1] + PrimalUVsUnit[T2]) * 0.5f;
			AddUV(NewUV);
			int32 NewUVId = PrimalUVsUnit.Num() - 1;
            UVCache.Add(Key, NewUVId);
            return NewUVId;
		};
	TArray<FIntVector> NewTris;
	TArray<FIntVector> NewUVs;
	TArray<FTriTreeNode*> NewPrimalTriTreeNodes;
	NewTris.Reserve(PrimalTris.Num() * 4);
	NewUVs.Reserve(PrimalTris.Num() * 4);
	NewPrimalTriTreeNodes.Reserve(PrimalTris.Num() * 4);
	for (int32 Index = 0; Index < PrimalTris.Num(); Index++)
	{
		FIntVector& Tri = PrimalTris[Index];
		FIntVector& UV = PrimalUVTris[Index];

		int32 A = Tri.X;
		int32 B = Tri.Y;
		int32 C = Tri.Z;

		int32 UVAId = UV.X;
        int32 UVBId = UV.Y;
        int32 UVCId = UV.Z;

		int32 A2B = GetMidPoint(A, B);
		int32 B2C = GetMidPoint(B, C);
		int32 C2A = GetMidPoint(C, A);

		int32 UVA2BId = GetMidUV(UVAId, UVBId);
		int32 UVB2CId = GetMidUV(UVBId, UVCId);
		int32 UVC2AId = GetMidUV(UVCId, UVAId);

		// 父节点（当前这个PrimalTri对应的TriTreeNode），分裂出四个子三角形：
		// Child0：A角小三角(A, A2B, C2A)
		// Child1：B角小三角(B, B2C, A2B)
		// Child2：C角小三角(C, C2A, B2C)
		// ChildMid：中间反向小三角(A2B, B2C, C2A)
		// 全部挂在父节点的Children[0..3]下，递归释放时即可统一回收。
		FTriTreeNode* Parent = PrimalTriTreeNodes[Index];

		FTriTreeNode* Child0 = new FTriTreeNode();
		Child0->CellIds[0] = A;       Child0->CellIds[1] = A2B;     Child0->CellIds[2] = C2A;
		Child0->Father = Parent;

		FTriTreeNode* Child1 = new FTriTreeNode();
		Child1->CellIds[0] = B;       Child1->CellIds[1] = B2C;     Child1->CellIds[2] = A2B;
		Child1->Father = Parent;

		FTriTreeNode* Child2 = new FTriTreeNode();
		Child2->CellIds[0] = C;       Child2->CellIds[1] = C2A;     Child2->CellIds[2] = B2C;
		Child2->Father = Parent;

		FTriTreeNode* ChildMid = new FTriTreeNode();
		ChildMid->CellIds[0] = A2B;   ChildMid->CellIds[1] = B2C;   ChildMid->CellIds[2] = C2A;
		ChildMid->Father = Parent;

		Parent->Children[0] = Child0;
		Parent->Children[1] = Child1;
		Parent->Children[2] = Child2;
		Parent->Children[3] = ChildMid;

		// 严格保持PrimalTris和PrimalTriTreeNodes的索引对应关系：
		// 每次AddPrimalTri都同步Add对应的TriTreeNode指针。
		NewTris.Add(FIntVector(A, A2B, C2A));
        NewUVs.Add(FIntVector{ UVAId, UVA2BId, UVC2AId });
		NewPrimalTriTreeNodes.Add(Child0);
		NewTris.Add(FIntVector(B, B2C, A2B));
        NewUVs.Add(FIntVector{ UVBId, UVB2CId, UVA2BId });
		NewPrimalTriTreeNodes.Add(Child1);
        NewTris.Add(FIntVector(C, C2A, B2C));
        NewUVs.Add(FIntVector{ UVCId, UVC2AId, UVB2CId });
		NewPrimalTriTreeNodes.Add(Child2);
        NewTris.Add(FIntVector(A2B, B2C, C2A));
        NewUVs.Add(FIntVector{ UVA2BId, UVB2CId, UVC2AId });
		NewPrimalTriTreeNodes.Add(ChildMid);
	}
	PrimalTris = MoveTemp(NewTris);
	PrimalUVTris = MoveTemp(NewUVs);
	PrimalTriTreeNodes = MoveTemp(NewPrimalTriTreeNodes);
	MidpointCache.Empty();
	UVCache.Empty();
}

void FSphereTopology::BuildDualFromPrimal()
{
	// 每个PrimalVertsUnit对应一个Cells
	// 每个PrimalTris对应一个Corners，也对应一个Tris
	// 每个PrimalTris的一条边对应一个Edges
	Cells.SetNum(PrimalVertsUnit.Num());
	Corners.SetNum(PrimalTris.Num());
	Tris.SetNum(PrimalTris.Num());
	UVToPrimalVert.Init(0, PrimalUVsUnit.Num());
	// 遍历PrimalTris，根据PrimalTris的顶点索引，构建Cells和Corners
	// 
	// Cells的CellId就是当前顶点的索引
	// Cells的UnitCenter就是当前顶点的坐标
	// Cells的bIsPentagon前12个为true
	// Cells的NeighborCellIds就是另外两个顶点的索引
	// Cells的CornerIds就是当前三角形的索引
	// Cells的EdgeIds就是当前三角形内包含本顶点的两条边的索引
	// 
	// Edges的EdgeId就是当前边的索引
	// Edges的CellIds就是两端点的索引，在GetEdge构造时就已经设置好了
	// Edges的CornerIds就是当前三角形的索引
	// Edges的bIsPlateBoundary和BoundaryStrength涉及到板块划分，暂时不设置
	// 
	// Corners的CornerId就是当前三角形的索引
	// Corners的UnitDir就是当前三角形的重心归一化
	// Corners的CellIds就是三个顶点的索引
	// Corners的EdgeIds就是当前三角形的三条边的索引
	// Corners的NeighborCornerIds在遍历完之后，重新遍历Edges设置
	// 
	// Tris的TriId就是当前三角形的索引
	// Tris的VertexIds就是当前三角形的三个顶点的PrimalTris索引
	// Tris的UVIds就是当前三角形的三个顶点的PrimalUVTris索引
	// 别的不用了
	// 

	TMap<uint64, int32> EdgeCache; // 用于给边去重
	auto GetEdge = [&](int32 P1, int32 P2) -> int32
		{
			int32 Min = FMath::Min(P1, P2);
			int32 Max = FMath::Max(P1, P2);
			uint64 Key = ((uint64)Min << 32) | Max;
			if (int32* Found = EdgeCache.Find(Key)) {
				return *Found;
			}

			int32 Index = Edges.Add(FCellEdge(Min, Max));
			Edges[Index].EdgeId = Index;
			EdgeCache.Add(Key, Index);
			return Index;
		};

	for (int32 I = 0; I < PrimalVertsUnit.Num(); ++I)
	{
		Cells[I].CellId = I;
		Cells[I].UnitCenter = PrimalVertsUnit[I];
		Cells[I].bIsPentagon = I < 12;
	}

	auto AddElement = []<uint32 N>(TStaticArray<int32, N>& Arr, const int32& Element) -> void
		{
			for (int32 I = 0; I < N; ++I)
			{
				if (Arr[I] == Element)
				{
					return;
				}
				if (Arr[I] == INDEX_NONE)
				{
					Arr[I] = Element;
					return;
				}
			}
		};

	for (int32 I = 0; I < PrimalTris.Num(); ++I)
	{
		int32 A = PrimalTris[I].X;
		int32 B = PrimalTris[I].Y;
		int32 C = PrimalTris[I].Z;

		int32 A2B = GetEdge(A, B);
		int32 B2C = GetEdge(B, C);
		int32 C2A = GetEdge(C, A);

		AddElement(Cells[A].NeighborCellIds, B);
		AddElement(Cells[A].NeighborCellIds, C);
		AddElement(Cells[B].NeighborCellIds, C);
		AddElement(Cells[B].NeighborCellIds, A);
		AddElement(Cells[C].NeighborCellIds, A);
		AddElement(Cells[C].NeighborCellIds, B);

		AddElement(Cells[A].CornerIds, I);
		AddElement(Cells[B].CornerIds, I);
		AddElement(Cells[C].CornerIds, I);

		AddElement(Cells[A].EdgeIds, A2B);
		AddElement(Cells[A].EdgeIds, C2A);
		AddElement(Cells[B].EdgeIds, B2C);
		AddElement(Cells[B].EdgeIds, A2B);
		AddElement(Cells[C].EdgeIds, C2A);
		AddElement(Cells[C].EdgeIds, B2C);

		AddElement(Edges[A2B].CornerIds, I);
		AddElement(Edges[B2C].CornerIds, I);
		AddElement(Edges[C2A].CornerIds, I);

		Corners[I].CornerId = I;
		Corners[I].UnitDir = (PrimalVertsUnit[A] + PrimalVertsUnit[B] + PrimalVertsUnit[C]).GetSafeNormal();
		Corners[I].CellIds = { A, B, C };
		Corners[I].EdgeIds = { A2B, B2C, C2A };

		Tris[I].TriId = I;
		Tris[I].VertexIds = { A, B, C };
		Tris[I].UVIds = { PrimalUVTris[I].X, PrimalUVTris[I].Y, PrimalUVTris[I].Z };

		// 叶子节点 ↔ Corner/Tri 一一对应：
		// 每次SubdividePrimalOnce都把PrimalTriTreeNodes整体替换成新叶子层，
		// 因此循环结束后PrimalTriTreeNodes[I]就是第I个叶子节点，其CornerId与Tris[I].TriId一致。
		// 非叶子节点（如TriTreeRoots或中间层）的CornerId保持默认的INDEX_NONE。
		PrimalTriTreeNodes[I]->CornerId = I;

		// 可能重复，但是不会冲突
		UVToPrimalVert[PrimalUVTris[I].X] = A;
		UVToPrimalVert[PrimalUVTris[I].Y] = B;
		UVToPrimalVert[PrimalUVTris[I].Z] = C;
		//Tris[I].FaceNormal = FVector::CrossProduct(PrimalVertsUnit[B] - PrimalVertsUnit[A], PrimalVertsUnit[C] - PrimalVertsUnit[A]).GetSafeNormal();
		//if (FVector::DotProduct(Tris[I].FaceNormal, Corners[I].UnitDir) < 0) 
		//{
		//	Tris[I].FaceNormal = -Tris[I].FaceNormal;
		//}
	}

	for (int32 I = 0; I < Corners.Num(); ++I)
	{
		for (int32 EdgeId : Corners[I].EdgeIds)
		{
			int32 NeighborCornerId = Edges[EdgeId].CornerIds[0] == I ? Edges[EdgeId].CornerIds[1] : Edges[EdgeId].CornerIds[0];
			AddElement(Corners[I].NeighborCornerIds, NeighborCornerId);
		}
	}

	// 这一版顺序没对上
	// for (int32 I = 0; I < Edges.Num(); ++I)
	// {
	// 	   int32 A = Edges[I].CornerIds[0];
	//	   int32 B = Edges[I].CornerIds[1];
    //     AddElement(Corners[A].NeighborCornerIds, B);
    //     AddElement(Corners[B].NeighborCornerIds, A);
	// }

	// 把Edges的CellIds和CornerIds都改为从西往东
	for (int32 I = 0; I < Edges.Num(); ++I)
	{
		int32 CellAId = Edges[I].CellIds[0];
		int32 CellBId = Edges[I].CellIds[1];
		if (FVector::CrossProduct(Cells[CellAId].UnitCenter, Cells[CellBId].UnitCenter).Z < 0.0f) 
		{
			Swap(Edges[I].CellIds[0], Edges[I].CellIds[1]);
		}
		int32 CornerAId = Edges[I].CornerIds[0];
		int32 CornerBId = Edges[I].CornerIds[1];
		if (FVector::CrossProduct(Corners[CornerAId].UnitDir, Corners[CornerBId].UnitDir).Z < 0.0f)
		{
			Swap(Edges[I].CornerIds[0], Edges[I].CornerIds[1]);
		}
	}
}