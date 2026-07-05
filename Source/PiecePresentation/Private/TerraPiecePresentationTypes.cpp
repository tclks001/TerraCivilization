#include "TerraPiecePresentationTypes.h"

#include "Engine/SkeletalMesh.h"

USkeletalMesh* FTerraPieceVisualConfig::ResolveMesh(ETerraGameplayPieceType PieceType) const
{
    switch (PieceType)
    {
    case ETerraGameplayPieceType::Commander:
        return CommanderMesh;
    case ETerraGameplayPieceType::Cavalry:
        return CavalryMesh;
    case ETerraGameplayPieceType::Archer:
        return ArcherMesh;
    case ETerraGameplayPieceType::Infantry:
    default:
        return InfantryMesh;
    }
}

UAnimationAsset* FTerraPieceVisualConfig::ResolveAttackAnimation(ETerraGameplayPieceType PieceType) const
{
    switch (PieceType)
    {
    case ETerraGameplayPieceType::Commander:
        return CommanderMagicAttackAnimation;
    case ETerraGameplayPieceType::Archer:
        return ArcherRangedAttackAnimation;
    case ETerraGameplayPieceType::Cavalry:
        return CavalryMeleeAttackAnimation;
    case ETerraGameplayPieceType::Infantry:
    default:
        return InfantryMeleeAttackAnimation;
    }
}

float FTerraPieceVisualConfig::ResolveAttackToHitSeconds(ETerraGameplayPieceType PieceType) const
{
    switch (PieceType)
    {
    case ETerraGameplayPieceType::Commander:
        return P3CommanderAttackToHitSeconds;
    case ETerraGameplayPieceType::Archer:
        return P3ArcherAttackToHitSeconds;
    case ETerraGameplayPieceType::Cavalry:
        return P3CavalryAttackToHitSeconds;
    case ETerraGameplayPieceType::Infantry:
    default:
        return P3InfantryAttackToHitSeconds;
    }
}
