#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"

#include "LuaImplementableInterface.generated.h"

class FLuaState;
struct lua_State;

UINTERFACE()
class BLUELUA_API ULuaImplementableInterface : public UInterface
{
	GENERATED_BODY()
};

class BLUELUA_API ILuaImplementableInterface
{
	GENERATED_BODY()

protected:
	virtual bool OnInit(const FString& InLuaFilePath, TSharedPtr<FLuaState> InLuaState = nullptr);
	virtual void OnRelease();
	virtual bool OnProcessEvent(UFunction* Function, void* Parameters);

protected:
	TSharedPtr<FLuaState> LuaState;

	int ModuleReferanceIndex;
};
