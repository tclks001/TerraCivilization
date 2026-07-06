#include "TerraMountedRiderAnimInstance.h"

#include "Animation/AnimMontage.h"

void UTerraMountedRiderAnimInstance::SetSittingAnimation(UAnimationAsset* InSittingAnimation)
{
    SittingAnimation = InSittingAnimation;
}

void UTerraMountedRiderAnimInstance::EnterSitting()
{
    RiderState = ETerraMountedRiderAnimState::Sitting;
}

float UTerraMountedRiderAnimInstance::PlayUpperBodyMontage(UAnimMontage* Montage, float StartOffsetSeconds)
{
    if (!Montage)
    {
        return 0.0f;
    }

    RiderState = ETerraMountedRiderAnimState::UpperBodyAction;
    const float DurationSeconds = Montage_Play(Montage, 1.0f);
    if (DurationSeconds > 0.0f && StartOffsetSeconds > 0.0f)
    {
        Montage_SetPosition(Montage, FMath::Min(StartOffsetSeconds, Montage->GetPlayLength()));
    }
    return DurationSeconds;
}

void UTerraMountedRiderAnimInstance::EnterDeath()
{
    RiderState = ETerraMountedRiderAnimState::Death;
}
