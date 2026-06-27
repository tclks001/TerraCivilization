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
	// 幂等保证：清空所有可变状态，无论之前 Build 过几次都得到相同结果。
	// 如果不清空，由于 BuildIcosahedronUnit / SubdividePrimalOnce 都是
	// Add/SetNum 追加式写入，二次调用会在原有数据上继续细分，导致
	// Cells/Corners 数量呈倍数膨胀（实测 sub=3 重复构建一次会变 41604 Cells）。
	{
		// 1) 释放已有的三角形树（避免内存泄漏；同 ~FSphereTopology 的逻辑）
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
		TriTreeRoots.Reset();
		PrimalTriTreeNodes.Reset();

		// 2) 清空所有几何 / 拓扑数组
		Cells.Reset();
		Edges.Reset();
		Tris.Reset();
		Corners.Reset();
		PrimalVertsUnit.Reset();
		PrimalUVsUnit.Reset();
		UVToPrimalVert.Reset();
		PrimalTris.Reset();
		PrimalUVTris.Reset();
		MidpointCache.Reset();
		UVCache.Reset();
	}

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
	// Corners的UnitDir就是当前三角形的"球面外心"（详见循环体内实现 + Docs/SphereTopologyReference.md §3.4 / §4.1）
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
		// 外心方向 = 三顶点所在平面的法向单位化（详见 SDF 设计稿 §14.7）。
		// 几何性质：外心方向到三个顶点的角距相等，因此对偶 hex/pent 的边
		// 在视觉上是平滑测地线，边中点权重严格 (0.5, 0.5, 0)，无折角。
		// 与之相对的"重心方向 = (V_A+V_B+V_C).GetSafeNormal()"在非等边
		// 三角形上会让 hex 边在中点出现折角，材质 SDF 边界产生可见裂缝。
		{
			const FVector& VA = PrimalVertsUnit[A];
			const FVector& VB = PrimalVertsUnit[B];
			const FVector& VC = PrimalVertsUnit[C];
			FVector Circumcenter = FVector::CrossProduct(VB - VA, VC - VA).GetSafeNormal();
			// 保证朝外（与重心方向同侧）。细分球三角形面法线本应朝外，
			// 但叉乘符号取决于 (A,B,C) 绕序，此处显式校正以保鲁棒性。
			if (FVector::DotProduct(Circumcenter, VA + VB + VC) < 0.0f)
			{
				Circumcenter = -Circumcenter;
			}
			Corners[I].UnitDir = Circumcenter;
		}
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

	// 先用临时方式把NeighborCornerIds填上（顺序不重要，下面统一会重排）
	for (int32 I = 0; I < Corners.Num(); ++I)
	{
		for (int32 EdgeId : Corners[I].EdgeIds)
		{
			int32 NeighborCornerId = Edges[EdgeId].CornerIds[0] == I ? Edges[EdgeId].CornerIds[1] : Edges[EdgeId].CornerIds[0];
			AddElement(Corners[I].NeighborCornerIds, NeighborCornerId);
		}
	}

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

	// ============================================================================
	// 索引数组顺时针对齐（关键不变量，下游高亮/边界遍历都依赖于此）
	// ============================================================================
	//
	// 视角：从球心向外看（站在Cell中心朝Cell外侧看），CW = Clockwise。
	//
	// 【对Cell】NeighborCellIds[i]、EdgeIds[i]、CornerIds[i]在第i项强对齐：
	//   - EdgeIds[i] 这条边连接 本Cell 和 NeighborCellIds[i]；
	//   - EdgeIds[i] 的两个Corner端点正好是 CornerIds[i] 和 CornerIds[(i+1) % N]。
	//   故"沿Cell边界走一圈"等价于：
	//       for (i = 0..N-1) drawArc(Corners[CornerIds[i]], Corners[CornerIds[(i+1)%N]]);
	//
	// 【对Corner】CellIds[i]、EdgeIds[i]、NeighborCornerIds[i]在第i项强对齐：
	//   - EdgeIds[i] 从本Corner出发指向 NeighborCornerIds[i]；
	//   - EdgeIds[i] 两侧的Cell正好是 CellIds[i] 和 CellIds[(i+1) % 3]。
	//
	// 实现要点：
	//   1. 先按"相邻元素围绕本元素中心"的极角排序（atan2 in tangent plane）。
	//   2. 旋转方向通过法向量的方向决定 - 我们要求逆时针on-screen角度排序映射成
	//      "从球心向外看的顺时针"（这两个等价：从外向球心看是逆时针 == 从球心向外看是顺时针）。
	//   3. 对Cell：以NeighborCellIds为参考排出环，再据此对齐EdgeIds、CornerIds。
	//   4. 对Corner：以NeighborCornerIds为参考排出环，再据此对齐EdgeIds、CellIds。

	auto SortIndicesByAngleCW = [&](const FVector& Center, const TArray<FVector>& Dirs, TArray<int32>& OutOrder)
		{
			// 计算每个 Dirs[k] 相对 Dirs[0] 绕 Center（球面外法向）的"有符号方位角"，
			// 然后按 CW（从球心向外看的顺时针）排序，且让 Dirs[0] 始终留在首位。
			//
			// 推导：
			//   令 N = Center 单位法向；A = D0 - (D0·N)N（D0 在切平面投影，单位化）；B = D - ...同理。
			//   A 到 B 绕 N 的有符号角 θ = atan2( (A × B) · N, A · B )。
			//   "从球心向外看"= 视角朝 -N，因此该视角下 CW = "绕 +N 的 CCW" 的反向 = θ 递增的反向，
			//   即 θ 递减。但让 Dirs[0] 留首位的最简方式是：把 θ 全部映射到 [0, 2π)（D0 处为 0），
			//   再按 (2π - θ) 升序，即可得到"以 D0 起步、CW 巡环"。
			OutOrder.Reset();
			OutOrder.SetNum(Dirs.Num());
			for (int32 K = 0; K < Dirs.Num(); ++K) OutOrder[K] = K;
			if (Dirs.Num() <= 1) return;

			const FVector N = Center.GetSafeNormal();
			auto Project = [&](const FVector& D) -> FVector
				{
					return (D - FVector::DotProduct(D, N) * N).GetSafeNormal();
				};
			const FVector A0 = Project(Dirs[0]);

			TArray<float> Theta;
			Theta.SetNum(Dirs.Num());
			Theta[0] = 0.0f;
			for (int32 K = 1; K < Dirs.Num(); ++K)
			{
				const FVector Bk = Project(Dirs[K]);
				const FVector Cross = FVector::CrossProduct(A0, Bk);
				const float SinTheta = FVector::DotProduct(Cross, N); // 绕 +N 的有符号 sin
				const float CosTheta = FVector::DotProduct(A0, Bk);
				float Th = FMath::Atan2(SinTheta, CosTheta);          // (-π, π]
				if (Th < 0.0f) Th += 2.0f * PI;                        // [0, 2π)
				Theta[K] = Th;
			}

			OutOrder.Sort([&](int32 P, int32 Q)
				{
					// CW from outside = 绕 +N 的 CW = 绕 +N 的 θ 递减；
					// 在 [0, 2π) 区间里把 D0(θ=0) 留首位，等价于按 (2π - θ) 升序，
					// 其中 D0 强制按 0 处理。
					const float Ap = (P == 0) ? 0.0f : (2.0f * PI - Theta[P]);
					const float Aq = (Q == 0) ? 0.0f : (2.0f * PI - Theta[Q]);
					return Ap < Aq;
				});
		};

	// ---- (A) 对每个Cell按CW排序 NeighborCellIds，并据此重排 EdgeIds / CornerIds ----
	for (int32 CellId = 0; CellId < Cells.Num(); ++CellId)
	{
		FCell& Cell = Cells[CellId];

		// 邻居数：五边形=5，六边形=6
		int32 N = 0;
		while (N < 6 && Cell.NeighborCellIds[N] != INDEX_NONE) ++N;
		if (N < 3) continue;

		// 收集邻居方向（每个邻居Cell的UnitCenter）
		TArray<FVector> Dirs; Dirs.SetNum(N);
		TArray<int32>   NbCells; NbCells.SetNum(N);
		for (int32 K = 0; K < N; ++K)
		{
			NbCells[K] = Cell.NeighborCellIds[K];
			Dirs[K] = Cells[NbCells[K]].UnitCenter;
		}

		TArray<int32> Order;
		SortIndicesByAngleCW(Cell.UnitCenter, Dirs, Order);

		// 写回 NeighborCellIds 按CW顺序
		for (int32 K = 0; K < 6; ++K) Cell.NeighborCellIds[K] = INDEX_NONE;
		for (int32 K = 0; K < N; ++K) Cell.NeighborCellIds[K] = NbCells[Order[K]];

		// 重排 EdgeIds：使 EdgeIds[i] = 连接 Cell 与 NeighborCellIds[i] 的那条边。
		// 注意：不能再用 Cell.GetEdgeIdWithNeighborCellId(...)，因为 Cell.EdgeIds 已经被清空。
		// 用全局 EdgeCache 反查：对 (CellId, NbCell) 的有序键直接拿到 EdgeId。
		for (int32 K = 0; K < 6; ++K) Cell.EdgeIds[K] = INDEX_NONE;
		for (int32 K = 0; K < N; ++K)
		{
			const int32 NbCell = Cell.NeighborCellIds[K];
			const int32 Min = FMath::Min(CellId, NbCell);
			const int32 Max = FMath::Max(CellId, NbCell);
			const uint64 Key = ((uint64)Min << 32) | Max;
			const int32* Found = EdgeCache.Find(Key);
			Cell.EdgeIds[K] = Found ? *Found : INDEX_NONE;
		}

		// 重排 CornerIds：约定 EdgeIds[i] 的两个Corner端点 = CornerIds[i] 和 CornerIds[(i+1) % N]。
		// 实现：对每条 Edge i，它的两个 Corner 端点中，必有一个被 Edge (i+1) 也共享、另一个被 Edge (i-1) 共享；
		// 与 Edge (i-1) 共享的那个 = CornerIds[i]，与 Edge (i+1) 共享的那个 = CornerIds[(i+1) % N]。
		for (int32 K = 0; K < 6; ++K) Cell.CornerIds[K] = INDEX_NONE;
		for (int32 I = 0; I < N; ++I)
		{
			const int32 EdgeI = Cell.EdgeIds[I];
			const int32 EdgePrev = Cell.EdgeIds[(I - 1 + N) % N];
			if (EdgeI == INDEX_NONE || EdgePrev == INDEX_NONE) continue;

			// EdgeI 的两个 Corner 端点
			const int32 C0 = Edges[EdgeI].CornerIds[0];
			const int32 C1 = Edges[EdgeI].CornerIds[1];
			// 选与 EdgePrev 共享的那个
			const FCorner& Cn0 = Corners[C0];
			bool C0Shared = (Cn0.EdgeIds[0] == EdgePrev || Cn0.EdgeIds[1] == EdgePrev || Cn0.EdgeIds[2] == EdgePrev);
			Cell.CornerIds[I] = C0Shared ? C0 : C1;
		}
	}

	// ---- (B) 对每个Corner按CW排序 NeighborCornerIds，并据此重排 EdgeIds / CellIds ----
	for (int32 CornerId = 0; CornerId < Corners.Num(); ++CornerId)
	{
		FCorner& Cn = Corners[CornerId];

		// Corner 一定有 3 个邻居 / 3 条边 / 3 个 Cell
		const int32 N = 3;
		// 收集邻居方向（每个邻居 Corner 的 UnitDir）
		TArray<FVector> Dirs; Dirs.SetNum(N);
		TArray<int32>   NbCorners; NbCorners.SetNum(N);
		for (int32 K = 0; K < N; ++K)
		{
			NbCorners[K] = Cn.NeighborCornerIds[K];
			if (NbCorners[K] == INDEX_NONE) { Dirs[K] = FVector::ZeroVector; continue; }
			Dirs[K] = Corners[NbCorners[K]].UnitDir;
		}

		TArray<int32> Order;
		SortIndicesByAngleCW(Cn.UnitDir, Dirs, Order);

		// 写回 NeighborCornerIds
		TStaticArray<int32, 3> NewNb { INDEX_NONE, INDEX_NONE, INDEX_NONE };
		for (int32 K = 0; K < N; ++K) NewNb[K] = NbCorners[Order[K]];
		for (int32 K = 0; K < N; ++K) Cn.NeighborCornerIds[K] = NewNb[K];

		// 重排 EdgeIds：使 EdgeIds[i] = 从本 Corner 通向 NeighborCornerIds[i] 的边。
		// 直接在 Edges[] 里反查端点为 (CornerId, NbCorner) 的边。
		// 注意：每个 Corner 的旧 Cn.EdgeIds（共3条）成员是确定的，遍历它就够了。
		TStaticArray<int32, 3> NewEdges { INDEX_NONE, INDEX_NONE, INDEX_NONE };
		for (int32 K = 0; K < N; ++K)
		{
			const int32 NbCorner = Cn.NeighborCornerIds[K];
			if (NbCorner == INDEX_NONE) { NewEdges[K] = INDEX_NONE; continue; }
			NewEdges[K] = INDEX_NONE;
			for (int32 EId : Cn.EdgeIds)
			{
				if (EId == INDEX_NONE) continue;
				const int32 EA = Edges[EId].CornerIds[0];
				const int32 EB = Edges[EId].CornerIds[1];
				if ((EA == CornerId && EB == NbCorner) || (EB == CornerId && EA == NbCorner))
				{
					NewEdges[K] = EId; break;
				}
			}
		}
		for (int32 K = 0; K < N; ++K) Cn.EdgeIds[K] = NewEdges[K];

		// 重排 CellIds：约定 EdgeIds[i] 两侧的 Cell = CellIds[i] 和 CellIds[(i+1) % 3]。
		// 实现：对每个 i，CellIds[i] = 同时属于 EdgeIds[(i-1+3)%3] 和 EdgeIds[i] 的那个 Cell。
		TStaticArray<int32, 3> NewCells { INDEX_NONE, INDEX_NONE, INDEX_NONE };
		for (int32 I = 0; I < N; ++I)
		{
			const int32 EdgeI = Cn.EdgeIds[I];
			const int32 EdgePrev = Cn.EdgeIds[(I - 1 + N) % N];
			if (EdgeI == INDEX_NONE || EdgePrev == INDEX_NONE) continue;

			// EdgeI 的两侧 Cell（西/东）
			const int32 CA = Edges[EdgeI].CellIds[0];
			const int32 CB = Edges[EdgeI].CellIds[1];
			// 选择同时属于 EdgePrev 的那个
			const int32 PA = Edges[EdgePrev].CellIds[0];
			const int32 PB = Edges[EdgePrev].CellIds[1];
			NewCells[I] = (CA == PA || CA == PB) ? CA : CB;
		}
		for (int32 K = 0; K < N; ++K) Cn.CellIds[K] = NewCells[K];
	}
}