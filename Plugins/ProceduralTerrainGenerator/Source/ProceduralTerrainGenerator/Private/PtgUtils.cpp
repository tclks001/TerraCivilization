// Copyright 2021 VICTOR HERNANDEZ MOLPECERES (Rockam). All Rights Reserved.

#include "PtgUtils.h"
#include "ProceduralTerrainGenerator.h"
#include "Engine/Engine.h"
#include "Async/Async.h"

void UPtgUtils::PrintDebugMessage(const UObject* caller, const FString& msg, const EPtgDebugMessageTypes type, const float timeOnScreen)
{
	// If we're not on the game thread, forward the call there to be safe (GEngine and UObjects must be used on game thread)
	if (!IsInGameThread())
	{
		const FString msgCopy = msg;
		const UObject* callerCopy = caller;
		const EPtgDebugMessageTypes typeCopy = type;
		const float timeCopy = timeOnScreen;

		AsyncTask(ENamedThreads::GameThread, [callerCopy, msgCopy, typeCopy, timeCopy]() {
			UPtgUtils::PrintDebugMessage(callerCopy, msgCopy, typeCopy, timeCopy);
		});

		return;
	}

	const FString callerName = (caller == nullptr) ? TEXT("None") : caller->GetName();
	const uint32 threadId = FPlatformTLS::GetCurrentThreadId();
	const double timestamp = FPlatformTime::Seconds();
	const FString composedMsg = FString::Printf(TEXT("[PTG][t=%.3f][Thread:%u][%s] %s"), timestamp, threadId, *callerName, *msg);
	FColor msgColor;

	// Choose message color and verbosity depending on type
	switch (type)
	{
	case EPtgDebugMessageTypes::Error:
		msgColor = FColor::Red;
		UE_LOG(LogProceduralTerrainGenerator, Error, TEXT("%s"), *composedMsg);
		break;

	case EPtgDebugMessageTypes::Warning:
		msgColor = FColor::Yellow;
		UE_LOG(LogProceduralTerrainGenerator, Warning, TEXT("%s"), *composedMsg);
		break;

	case EPtgDebugMessageTypes::Info:
	default:
		msgColor = FColor::Green;
		UE_LOG(LogProceduralTerrainGenerator, Log, TEXT("%s"), *composedMsg);
		break;
	}

#if !UE_BUILD_SHIPPING
	// Print to screen and log (already logged above)
	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, timeOnScreen, msgColor, composedMsg);
	}
#endif
}
