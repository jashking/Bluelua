#include "BlueluaLibrary.h"

#include "DelayAction.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LatentActionManager.h"
#include "Engine/World.h"
#include "Framework/Commands/InputChord.h"
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

void UBlueluaLibrary::BindAction(AActor* TargetActor, FName ActionName, EInputEvent KeyEvent, bool InbConsumeInput, bool InbExecuteWhenPaused, FInputActionHandlerDynamicSignature Action)
{
	if (!TargetActor || !TargetActor->InputComponent)
	{
		return;
	}

	FInputActionBinding AB(ActionName, KeyEvent);
	AB.ActionDelegate.BindDelegate(Action.GetUObject(), Action.GetFunctionName());
	AB.bConsumeInput = InbConsumeInput;
	AB.bExecuteWhenPaused = InbExecuteWhenPaused;
	TargetActor->InputComponent->AddActionBinding(AB);
}

void UBlueluaLibrary::BindAxisAction(AActor* TargetActor, FName AxisName, bool InbConsumeInput, bool InbExecuteWhenPaused, FInputAxisHandlerDynamicSignature Action)
{
	if (!TargetActor || !TargetActor->InputComponent)
	{
		return;
	}

	FInputAxisBinding AB(AxisName);
	AB.bConsumeInput = InbConsumeInput;
	AB.bExecuteWhenPaused = InbExecuteWhenPaused;
	AB.AxisDelegate.BindDelegate(Action.GetUObject(), Action.GetFunctionName());

	TargetActor->InputComponent->AxisBindings.Emplace(AB);
}

void UBlueluaLibrary::BindTouchAction(AActor* TargetActor, EInputEvent InputEvent, bool InbConsumeInput, bool InbExecuteWhenPaused, FInputTouchHandlerDynamicSignature Action)
{
	if (!TargetActor || !TargetActor->InputComponent)
	{
		return;
	}

	FInputTouchBinding TB(InputEvent);
	TB.bConsumeInput = InbConsumeInput;
	TB.bExecuteWhenPaused = InbExecuteWhenPaused;
	TB.TouchDelegate.BindDelegate(Action.GetUObject(), Action.GetFunctionName());

	TargetActor->InputComponent->TouchBindings.Emplace(TB);
}

void UBlueluaLibrary::BindKeyAction(AActor* TargetActor, FInputChord InInputChord, EInputEvent KeyEvent, bool InbConsumeInput, bool InbExecuteWhenPaused, FInputActionHandlerDynamicSignature Action)
{
	if (!TargetActor || !TargetActor->InputComponent)
	{
		return;
	}

	FInputKeyBinding KB(InInputChord, KeyEvent);
	KB.bConsumeInput = InbConsumeInput;
	KB.bExecuteWhenPaused = InbExecuteWhenPaused;
	KB.KeyDelegate.BindDelegate(Action.GetUObject(), Action.GetFunctionName());

	TargetActor->InputComponent->KeyBindings.Emplace(KB);
}
