#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "TerraPiecePresentationTypes.h"

class APlanetTessellatedMesh;
class UAnimationAsset;
struct FTerraPiecePresentationCaptureEvent;
struct FTerraPiecePresentationMoveEvent;

/**
 * SimpleGameplay camera helper for APlanetTessellatedMesh.
 *
 * The planet actor still owns serialized camera settings for Blueprint/map
 * compatibility; this class owns the camera behavior and transient state.
 */
class TERRACIVILIZATION_API FPlanetCameraController
{
public:
    explicit FPlanetCameraController(APlanetTessellatedMesh& InHost);

    void Tick(float DeltaSeconds);
    void Reset();
    void ClearDelayedTurnStartFocusTimer();
    bool IsDelayedTurnStartFocusTimerActive() const;

    void FocusCameraOnCell(int32 CellId, bool bMoveCamera);
    void FocusCameraOnSelectedCellSmart(int32 CellId);
    bool IsCellInC4ComfortView(int32 CellId) const;
    bool RequestC4SelectionFocus(int32 CellId) const;
    void RequestC6ActionCameraTrackingForMoveEvents(const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents) const;
    float GetC6ActionCameraBlendSeconds(ETerraPiecePresentationMoveType MoveType) const;
    bool TryRequestC6_5DelayedTurnStartFocus(
        int32 ExpectedTurnIndex,
        int32 ExpectedFactionId,
        const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents,
        const TArray<FTerraPiecePresentationCaptureEvent>& CaptureEvents);
    void ExecuteC6_5DelayedTurnStartFocus(int32 ExpectedTurnIndex, int32 ExpectedFactionId);
    float GetC6_5ActionPresentationDelaySeconds(
        const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents,
        const TArray<FTerraPiecePresentationCaptureEvent>& CaptureEvents) const;
    float GetC6_5AttackAnimationDurationSeconds(ETerraGameplayPieceType PieceType, UAnimationAsset* AttackAnimation, float FallbackSeconds) const;
    float GetC6_5AnimationLengthSeconds(UAnimationAsset* AnimationAsset, float FallbackSeconds) const;

    bool FocusCameraOnCurrentFactionWarZoneHard();
    bool BlendCameraFocusToCurrentFactionWarZone();
    bool TryBuildCurrentFactionWarZoneDirection(FVector& OutLocalWarZoneDir) const;
    bool TryBuildCurrentFactionCommanderDirection(FVector& OutLocalCommanderDir) const;
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
    APlanetTessellatedMesh& Host;
    int32 LastFocusedTurnIndex = INDEX_NONE;
    bool bGameStartCameraApplied = false;
    FTimerHandle DelayedTurnStartFocusTimerHandle;
};
