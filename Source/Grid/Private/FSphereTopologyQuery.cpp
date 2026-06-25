// Fill out your copyright notice in the Description page of Project Settings.


#include "FSphereTopologyQuery.h"

FSphereTopologyQuery::FSphereTopologyQuery(const FSphereTopology* InTopology)
	:Topology(InTopology)
{
}

FSphereTopologyQuery::~FSphereTopologyQuery()
{
}

FSurfaceQueryResult FSphereTopologyQuery::FindNearestCell(const FVector& WorldPos) const
{
	FSurfaceQueryResult Result;
	if (Topology == nullptr || Topology->TriTreeRoots.Num() == 0)
	{
		return Result;
	}

	// 将WorldPos投影到单位球面进行方向比较
	const FVector UnitPos = WorldPos.GetSafeNormal();
	if (UnitPos.IsNearlyZero())
	{
		return Result;
	}

	// 第一步：在20个根节点中选取Center·UnitPos最大的根
	const FTriTreeNode* CurNode = nullptr;
	{
		float BestDot = -FLT_MAX;
		for (const FTriTreeNode* Root : Topology->TriTreeRoots)
		{
			if (Root == nullptr) continue;
			const float Dot = FVector::DotProduct(Root->Center, UnitPos);
			if (Dot > BestDot)
			{
				BestDot = Dot;
				CurNode = Root;
			}
		}
	}

	// 第二步：逐层向下，挑选Center·UnitPos最大的子节点，直到叶子节点（Children[0] == nullptr）
	while (CurNode != nullptr && CurNode->Children[0] != nullptr)
	{
		const FTriTreeNode* BestChild = nullptr;
		float BestDot = -FLT_MAX;
		for (int32 I = 0; I < 4; ++I)
		{
			const FTriTreeNode* Child = CurNode->Children[I];
			if (Child == nullptr) continue;
			const float Dot = FVector::DotProduct(Child->Center, UnitPos);
			if (Dot > BestDot)
			{
				BestDot = Dot;
				BestChild = Child;
			}
		}
		if (BestChild == nullptr)
		{
			break;
		}
		CurNode = BestChild;
	}

	if (CurNode == nullptr)
	{
		return Result;
	}

	// 第三步：在叶子节点的三个角Cell中找UnitCenter·UnitPos最大的Cell
	int32 BestCellId = INDEX_NONE;
	float BestCellDot = -FLT_MAX;
	for (int32 I = 0; I < 3; ++I)
	{
		const int32 CellId = CurNode->CellIds[I];
		if (CellId == INDEX_NONE || CellId < 0 || CellId >= Topology->Cells.Num())
		{
			continue;
		}
		const FVector& UnitCenter = Topology->Cells[CellId].UnitCenter;
		const float Dot = FVector::DotProduct(UnitCenter, UnitPos);
		if (Dot > BestCellDot)
		{
			BestCellDot = Dot;
			BestCellId = CellId;
		}
	}

	if (BestCellId == INDEX_NONE)
	{
		return Result;
	}

	// 第四步：构建FSurfaceQueryResult
	const FVector& BestUnitCenter = Topology->Cells[BestCellId].UnitCenter;
	const float WorldRadius = WorldPos.Size();
	Result.CellId = BestCellId;
	Result.ClosestPoint = BestUnitCenter * WorldRadius; // 球面上最接近的方向，半径取WorldPos的长度
	Result.Distance = (WorldPos - Result.ClosestPoint).Size();
	return Result;
}
FSurfaceFrame FSphereTopologyQuery::BuildCellFrame(int32 CellId, float Radius) const
{
	FSurfaceFrame Frame;
	if (Topology == nullptr || CellId < 0 || CellId >= Topology->Cells.Num())
	{
		return Frame;
	}
	Frame.Origin = Topology->Cells[CellId].UnitCenter * Radius;
	Frame.Up = Topology->Cells[CellId].UnitCenter; // Up方向指向球心
	Frame.Forward = FVector::UpVector.ProjectOnToNormal(Frame.Up).GetSafeNormal(); // 北
	Frame.Right = FVector::CrossProduct(Frame.Forward, Frame.Up);
	return Frame;
}
void FSphereTopologyQuery::CollectCellRing(int32 CenterCellId, int32 RingCount, TArray<int32>& OutCellIds) const
{
	if (Topology == nullptr || CenterCellId < 0 || CenterCellId >= Topology->Cells.Num() || RingCount < 0)
	{
		return;
	}
	OutCellIds.Empty();
	if (RingCount == 0)
	{
		OutCellIds.Add(CenterCellId);
		return;
	}
	TArray<int32> Steps;
    Steps.Init(-1, Topology->Cells.Num());

	TQueue<int32> Queue;
	Queue.Empty();
	Queue.Enqueue(CenterCellId);
    Steps[CenterCellId] = 0;
	while (!Queue.IsEmpty())
	{
		int32 CurCellId;
		Queue.Dequeue(CurCellId);
		for (int32 I = 0; I < 6; ++I)
		{
			int32 NextCellId = Topology->Cells[CurCellId].NeighborCellIds[I];
			if (NextCellId == INDEX_NONE || NextCellId < 0 || NextCellId >= Topology->Cells.Num())
			{
				continue;
			}
			if (Steps[NextCellId] != -1)
			{
				continue;
			}
			Steps[NextCellId] = Steps[CurCellId] + 1;
			if (Steps[NextCellId] == RingCount)
			{
				OutCellIds.Add(NextCellId);
			}
			else
			{
				Queue.Enqueue(NextCellId);
			}
		}
	}
}
void FSphereTopologyQuery::CollectCellDisk(int32 CenterCellId, int32 RingCount, TArray<int32>& OutCellIds) const
{
	if (Topology == nullptr || CenterCellId < 0 || CenterCellId >= Topology->Cells.Num() || RingCount < 0)
	{
		return;
	}
    OutCellIds.Empty();
	OutCellIds.Add(CenterCellId);

	TArray<int32> Steps;
	Steps.Init(-1, Topology->Cells.Num());

	TQueue<int32> Queue;
	Queue.Empty();
	Queue.Enqueue(CenterCellId);
	Steps[CenterCellId] = 0;
	while (!Queue.IsEmpty())
	{
		int32 CurCellId;
		Queue.Dequeue(CurCellId);
		for (int32 I = 0; I < 6; ++I)
		{
			int32 NextCellId = Topology->Cells[CurCellId].NeighborCellIds[I];
			if (NextCellId == INDEX_NONE || NextCellId < 0 || NextCellId >= Topology->Cells.Num())
			{
				continue;
			}
			if (Steps[NextCellId] != -1)
			{
				continue;
			}
			Steps[NextCellId] = Steps[CurCellId] + 1;
			if (Steps[NextCellId] <= RingCount)
			{
				OutCellIds.Add(NextCellId);
			}
			if (Steps[NextCellId] < RingCount)
			{
				Queue.Enqueue(NextCellId);
			}
		}
	}
}