// Copyright 2021 VICTOR HERNANDEZ MOLPECERES (Rockam). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PtgModifier.generated.h"

/**
* Base class for nature generation modifiers.
*/
UCLASS(Abstract)
class PROCEDURALTERRAINGENERATOR_API APtgModifier : public AActor
{
	GENERATED_BODY()

public:

	APtgModifier();

	/**
	* Gets the modifier shape component.
	*
	* @return The modifier shape component.
	*/
	UFUNCTION(BlueprintImplementableEvent, BlueprintPure, Category = "PTG Modifier")
	UShapeComponent* GetShapeComponent() const;
};
