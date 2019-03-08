#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlueluaLibrary.generated.h"

UCLASS()
class BLUELUA_API UBlueluaLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Utilities|BlueluaLibrary")
	static UObject* GetWorldContext();

	/**
	* Perform a lua function delegate with a delay (specified in seconds).  Calling again while it is counting down will be ignored.
	*
	* @param WorldContext	World context.
	* @param Duration 		length of delay (in seconds).
	* @param InDelegateId 		DelegateId.
	* @param InDelegate 	The lua function delegate.
	*/
	UFUNCTION(BlueprintCallable, Category = "Utilities|BlueluaLibrary", meta = (WorldContext = "WorldContextObject", Duration = "0.2"))
	static int32 Delay(UObject* WorldContextObject, float Duration, int32 InDelegateId, class ULuaFunctionDelegate* InDelegate);
};
