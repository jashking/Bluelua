#include "BlueluaLibrary.h"

#include "DelayAction.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LatentActionManager.h"
#include "Engine/World.h"
#include "LatentActions.h"
#include "Misc/Guid.h"

#include "LuaFunctionDelegate.h"

UObject* UBlueluaLibrary::GetWorldContext()
{
	if (GEngine && GEngine->GameViewport)
	{
		return Cast<UObject>(GEngine->GameViewport->GetWorld());
	}

	return nullptr;
}

int32 UBlueluaLibrary::Delay(UObject* WorldContextObject, float Duration, int32 InDelegateId, class ULuaFunctionDelegate* InDelegate)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		const int32 DelegateUUID = InDelegateId >= 0 ? InDelegateId : GetTypeHash(FGuid::NewGuid());

		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		if (LatentActionManager.FindExistingAction<FDelayAction>(InDelegate, DelegateUUID) == NULL)
		{
			FDelayAction* DelayAction = new FDelayAction(Duration, FLatentActionInfo());
			DelayAction->ExecutionFunction = ULuaFunctionDelegate::DelegateFunctionName;
			DelayAction->OutputLink = 0;
			DelayAction->CallbackTarget = InDelegate;

			LatentActionManager.AddNewAction(InDelegate, DelegateUUID, DelayAction);
		}

		return DelegateUUID;
	}

	return -1;
}
