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
