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

public:
	void PreInit(TSharedPtr<FLuaState> InLuaState = nullptr);
	bool IsLuaBound() const;
	bool FetchLuaModule();

protected:
	void PreRegisterLua(const FString& InLuaFilePath);

	virtual bool OnInit(const FString& InLuaFilePath, TSharedPtr<FLuaState> InLuaState = nullptr);
	virtual void OnRelease();
	virtual bool OnProcessEvent(UFunction* Function, void* Parameters);

protected:
	TSharedPtr<FLuaState> LuaState;

	FString BindingLuaPath;

	int ModuleReferanceIndex = -2;
};
