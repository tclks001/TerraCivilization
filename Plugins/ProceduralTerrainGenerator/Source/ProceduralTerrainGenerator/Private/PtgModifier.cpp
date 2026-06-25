// Copyright 2021 VICTOR HERNANDEZ MOLPECERES (Rockam). All Rights Reserved.

#include "PtgModifier.h"

APtgModifier::APtgModifier()
{
	// Disable tick, cause is not needed
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.bCanEverTick = false;

	// Disable another actor unused stuff
	SetCanBeDamaged(false);
	bFindCameraComponentWhenViewTarget = false;
	bEnableAutoLODGeneration = false;
}
