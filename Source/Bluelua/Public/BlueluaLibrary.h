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
};
