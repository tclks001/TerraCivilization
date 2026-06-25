#include "Misc/AutomationTest.h"
#include "PtgProcMeshDataHelper.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPtgProcMeshDataHelper_GeneratePlaneDataTest, "ProceduralTerrainGenerator.PtgProcMeshDataHelper.GeneratePlaneData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPtgProcMeshDataHelper_GeneratePlaneDataTest::RunTest(const FString& Parameters)
{
    FPtgProcMeshData ProcMeshData;
    float LowestGeneratedHeight = 0.0f;
    float HighestGeneratedHeight = 0.0f;

    // Generate a small plane and verify basic invariants
    UPtgProcMeshDataHelper::GeneratePlaneData(ProcMeshData, LowestGeneratedHeight, HighestGeneratedHeight, 1000.0f, 10, nullptr, 1.0f, 100.0f, FVector::ZeroVector);

    TestTrue(TEXT("ProcMeshData should contain vertices"), ProcMeshData.Vertices.Num() > 0);
    TestTrue(TEXT("ProcMeshData triangles count should be divisible by 3"), (ProcMeshData.Triangles.Num() % 3) == 0);
    TestTrue(TEXT("LowestGeneratedHeight should be <= HighestGeneratedHeight"), LowestGeneratedHeight <= HighestGeneratedHeight);

    return true;
}
