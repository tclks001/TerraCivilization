#include "TerraPiecePresentationTypes.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"

USkeletalMesh* FTerraPieceVisualConfig::ResolveMesh(ETerraGameplayPieceType PieceType) const
{
    switch (PieceType)
    {
    case ETerraGameplayPieceType::Commander:
        return CommanderMesh;
    case ETerraGameplayPieceType::Cavalry:
        return CavalryMesh;
    case ETerraGameplayPieceType::ArcherCavalry:
        return ArcherMesh;
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
    case ETerraGameplayPieceType::ArcherCavalry:
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
    case ETerraGameplayPieceType::ArcherCavalry:
        return ArcherCavalryAttackToHitSeconds;
    case ETerraGameplayPieceType::Cavalry:
        return P3CavalryAttackToHitSeconds;
    case ETerraGameplayPieceType::Infantry:
    default:
        return P3InfantryAttackToHitSeconds;
    }
}

UTexture2D* FTerraPieceVisualConfig::ResolveP7BaseTexture(ETerraGameplayPieceType PieceType) const
{
    switch (PieceType)
    {
    case ETerraGameplayPieceType::Commander:
        return P7CommanderBaseTexture.Get();
    case ETerraGameplayPieceType::Archer:
    case ETerraGameplayPieceType::ArcherCavalry:
        return P7ArcherBaseTexture.Get();
    case ETerraGameplayPieceType::Cavalry:
        return P7CavalryRiderBaseTexture.Get();
    case ETerraGameplayPieceType::Infantry:
    default:
        return P7InfantryBaseTexture.Get();
    }
}

const FTerraPiecePaletteMask* FTerraPieceVisualConfig::ResolveP7PaletteMask(ETerraGameplayPieceType PieceType) const
{
    const ETerraGameplayPieceType LookupPieceType = PieceType == ETerraGameplayPieceType::ArcherCavalry
        ? ETerraGameplayPieceType::Archer
        : PieceType;
    for (const FTerraPiecePaletteMask& Mask : P7PaletteMasksByPieceType)
    {
        if (Mask.PieceType == LookupPieceType)
        {
            return &Mask;
        }
    }

    return nullptr;
}

const FTerraPieceFactionPalette* FTerraPieceVisualConfig::ResolveP7FactionPalette(int32 FactionId) const
{
    if (P7FactionPalettes.IsValidIndex(FactionId))
    {
        return &P7FactionPalettes[FactionId];
    }

    return P7FactionPalettes.Num() > 0 ? &P7FactionPalettes[0] : nullptr;
}
