#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "TerraPiecePresentationTypes.h"
#include "PlanetCameraComponent.generated.h"

class APlanetTessellatedMesh;
class UAnimationAsset;
struct FTerraPiecePresentationCaptureEvent;
struct FTerraPiecePresentationMoveEvent;

/**
 * SimpleGameplay camera component for APlanetTessellatedMesh.
 *
 * Owns serialized camera settings and runtime camera state. The planet actor
 * remains the gameplay/render host and exposes thin compatibility wrappers.
 */
UCLASS(ClassGroup = (TerraCivilization), meta = (BlueprintSpawnableComponent))
class TERRACIVILIZATION_API UPlanetCameraComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPlanetCameraComponent();

    virtual void BeginPlay() override;

    /** true：回合开始自动切到当前阵营大本营上方，选中棋子时自动旋转视角对准。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay G2.5")
    bool bEnableG2_5CameraAssist = true;

    /** 回合开始时摄像机位于大本营球面外侧的额外高度（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay G2.5", meta = (ClampMin = "0.0", ClampMax = "100000.0"))
    float G2_5TurnStartCameraHeightCM = 8000.0f;

    /** C2：true 时游戏开始硬设置到当前阵营活棋子的战区中心斜俯视。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay C2 Camera", meta = (DisplayName = "Enable C2 Game Start War Zone Camera"))
    bool bEnableC2GameStartWarZoneCamera = true;

    /** C2.5：true 时每次回合开始平滑把 C3 视角中心切到当前阵营战区中心。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay C2.5 Camera")
    bool bEnableC2_5TurnStartWarZoneFocusBlend = true;

    /** C2.5：回合开始平滑切换视角中心的 Blend 时长（秒），不改变当前距离。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay C2.5 Camera", meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float C2_5TurnStartFocusBlendSeconds = 0.45f;

    /** C2：已禁用。回合开始距离改由 C3InitialDistanceToFocusCM 控制。字段仅保留以兼容旧资产。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SimpleGameplay C2 Camera|Deprecated", meta = (DeprecatedProperty, DeprecationMessage = "Disabled. Use C3InitialDistanceToFocusCM instead."))
    float C2TurnStartCameraDistanceCM = 18000.0f;

    /** C2：已禁用。回合开始倾角改由 C3.7 自动倾角逻辑根据距离派生。字段仅保留以兼容旧资产。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SimpleGameplay C2 Camera|Deprecated", meta = (DeprecatedProperty, DeprecationMessage = "Disabled. C3.7 auto tilt derives tilt from distance."))
    float C2TurnStartCameraTiltDeg = 55.0f;

    /** true：允许 PlayerController 通过 WSAD + 滚轮驱动球面轨道相机。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay G8", meta = (DisplayName = "Enable Manual Camera Control"))
    bool bEnableG8ManualCameraControl = true;

    /** G8：每秒经纬度变化速度（度 / 秒）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay G8", meta = (ClampMin = "1.0", ClampMax = "360.0"))
    float G8CameraOrbitDegreesPerSecond = 45.0f;

    /** G8：每次滚轮缩放的高度步长（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay G8", meta = (ClampMin = "10.0", ClampMax = "100000.0"))
    float G8CameraZoomStepCM = 800.0f;

    /** G8：相机允许的最小离地高度（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay G8", meta = (ClampMin = "0.0", ClampMax = "100000.0"))
    float G8CameraMinHeightOffsetCM = 2500.0f;

    /** G8：相机允许的最大离地高度（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay G8", meta = (ClampMin = "0.0", ClampMax = "200000.0"))
    float G8CameraMaxHeightOffsetCM = 30000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay G8", meta = (ClampMin = "0.0", ClampMax = "200000.0"))
    float C3AutoTiltMinDistanceCM = 6000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay G8", meta = (ClampMin = "0.0", ClampMax = "200000.0"))
    float C3AutoTiltMaxDistanceCM = 20000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay G8", meta = (ClampMin = "5.0", ClampMax = "85.0"))
    float C3AutoTiltAtMinDistanceDeg = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay G8", meta = (ClampMin = "5.0", ClampMax = "85.0"))
    float C3AutoTiltAtMaxDistanceDeg = 85.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay G8", meta = (ClampMin = "0.0", ClampMax = "200000.0"))
    float C3InitialDistanceToFocusCM = 8000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay C4 Camera")
    bool bEnableC4SmartSelectionFocus = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay C4 Camera", meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float C4ComfortFocusAngleDeg = 9.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay C4 Camera", meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float C4SelectedPieceFocusBlendSeconds = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay C6 Camera")
    bool bEnableC6ActionCameraTracking = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay C6.5 Camera")
    bool bEnableC6_5DelayTurnStartFocusUntilActionPresentationEnds = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimpleGameplay C6.5 Camera", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float C6_5TurnStartFocusDelayPaddingSeconds = 0.1f;

    void TickCamera(float DeltaSeconds);
    void ResetCameraState();
    void ClearDelayedTurnStartFocusTimer();
    bool IsDelayedTurnStartFocusTimerActive() const;

    void FocusCameraOnCell(int32 CellId, bool bMoveCamera);
    void FocusCameraOnSelectedCellSmart(int32 CellId);
    bool IsCellInC4ComfortView(int32 CellId) const;
    bool RequestC4SelectionFocus(int32 CellId) const;
    void RequestC6ActionCameraTrackingForMoveEvents(const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents) const;
    float GetC6ActionCameraBlendSeconds(ETerraPiecePresentationMoveType MoveType) const;
    float GetC6_5ActionPresentationDelaySeconds(
        const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents,
        const TArray<FTerraPiecePresentationCaptureEvent>& CaptureEvents) const;
    bool TryRequestC6_5DelayedTurnStartFocus(
        int32 ExpectedTurnIndex,
        int32 ExpectedFactionId,
        const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents,
        const TArray<FTerraPiecePresentationCaptureEvent>& CaptureEvents);
    void ExecuteC6_5DelayedTurnStartFocus(int32 ExpectedTurnIndex, int32 ExpectedFactionId);
    void FocusCameraOnCurrentFactionBase();

    bool SyncOrbitCameraStateFromWorldPosition(const FVector& CameraWorldPosition, float& InOutLongitudeDeg, float& InOutLatitudeDeg, float& InOutHeightOffsetCM) const;
    bool ApplyOrbitCameraState(float LongitudeDeg, float LatitudeDeg, float HeightOffsetCM);
    bool SyncFocusCameraStateFromView(
        const FVector& CameraWorldPosition,
        const FRotator& CameraWorldRotation,
        FVector& OutFocusUnitDir,
        float& OutDistanceToFocusCM,
        float& OutTiltDeg,
        float& OutYawAroundFocusDeg) const;
    bool ApplyFocusCameraState(
        const FVector& FocusUnitDir,
        float DistanceToFocusCM,
        float TiltDeg,
        float YawAroundFocusDeg);
    bool OffsetFocusCameraStateOnTangent(
        float RightDeltaDeg,
        float ForwardDeltaDeg,
        FVector& InOutFocusUnitDir,
        float& InOutYawAroundFocusDeg) const;

    int32 GetLastFocusedTurnIndex() const { return LastFocusedTurnIndex; }
    void SetLastFocusedTurnIndex(int32 TurnIndex) { LastFocusedTurnIndex = TurnIndex; }

private:
    APlanetTessellatedMesh* GetHost() const;
    float GetC6_5AttackAnimationDurationSeconds(ETerraGameplayPieceType PieceType, UAnimationAsset* AttackAnimation, float FallbackSeconds) const;
    float GetC6_5AnimationLengthSeconds(UAnimationAsset* AnimationAsset, float FallbackSeconds) const;
    bool FocusCameraOnCurrentFactionWarZoneHard();
    bool BlendCameraFocusToCurrentFactionWarZone();
    bool TryBuildCurrentFactionWarZoneDirection(FVector& OutLocalWarZoneDir) const;
    bool TryBuildCurrentFactionCommanderDirection(FVector& OutLocalCommanderDir) const;
    int32 LastFocusedTurnIndex = INDEX_NONE;
    bool bGameStartCameraApplied = false;
    FTimerHandle DelayedTurnStartFocusTimerHandle;
};
