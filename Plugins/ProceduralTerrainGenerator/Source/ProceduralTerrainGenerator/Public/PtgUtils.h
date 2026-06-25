// Copyright 2021 VICTOR HERNANDEZ MOLPECERES (Rockam). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PtgUtils.generated.h"

/**
* Defines the debug message types.
*/
UENUM(BlueprintType)
enum class EPtgDebugMessageTypes : uint8
{
	Info		UMETA(DisplayName = "Info"),
	Warning		UMETA(DisplayName = "Warning"),
	Error		UMETA(DisplayName = "Error")
};

/**
 *  Utils library. Can be used in blueprints, if exposed.
 */
UCLASS()
class PROCEDURALTERRAINGENERATOR_API UPtgUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	* Prints debug message to screen and log. Color depends on the type.
	*
	* @param caller			- Caller object.
	* @param msg			- Message to print.
	* @param type			- Message type.
	* @param timeOnScreen	- Time (in seconds) message will be on screen.
	*/
	UFUNCTION(BlueprintCallable, Category = "PTG Utils")
	static void PrintDebugMessage(const UObject* caller = nullptr, const FString& msg = TEXT(""), const EPtgDebugMessageTypes type = EPtgDebugMessageTypes::Info, const float timeOnScreen = 5.0f);

};
